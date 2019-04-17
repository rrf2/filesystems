#include <comp421/filesystem.h>
#include <comp421/iolib.h>
#include <comp421/yalnix.h>
#include <stdio.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
// #include <libgen.h>

#define OPEN 0
#define CLOSE 1
#define CREATE 2
#define READ 3
#define WRITE 4
#define SEEK 5
#define LINK 6
#define UNLINK 7
#define SYMLINK 8
#define READLINK 9
#define MKDIR 10
#define RMDIR 11
#define CHDIR 12
#define STAT 13
#define SYNC 14
#define SHUTDOWN 15

struct fs_header header;
int num_blocks;
int num_inodes;

int *blockbitmap;

struct my_msg1{
    int type;
    int data1;
    char data2[16];
    void *ptr;
};


struct my_msg2{
    int type;
    int data1;
    int data2;
    int data3;
    int data4;
    int data5;
    void *ptr;
};

struct my_msg3{
	int type;
	int len;
	int cur_inode;
	int data;
	char *buf;
	void *ptr;
};

struct my_msg4{
	int type;
	int len_oldname;
	int cur_inode;
	int len_newname;
	char *oldname;
	char *newname;
};

struct cache_entry {
	int num;
	void *data;
	int dirty;
	int num_used;
};

struct list_elem {
	struct cache_entry *entry;
	struct list_elem *next;
};

struct list_elem dummy;

#define INODE_HASHTABLE_SIZE INODE_CACHESIZE * 2
#define BLOCK_HASHTABLE_SIZE BLOCK_CACHESIZE * 2

struct list_elem *inode_hashtable[INODE_HASHTABLE_SIZE];
struct list_elem *block_hashtable[BLOCK_HASHTABLE_SIZE];

int num_inodes_cached = 0;
int num_blocks_cached = 0;


struct cache_entry *get_cached_elem(int num, int block);
void insert_elem_in_cache(int num, void *data, int block);
int remove_lru_block();
int remove_lru_inode();
char *get_block(int num);
struct inode *get_inode(int num);
int main();
void read_with_offset(int sectornum, void *buf, int offset, int size);
int get_inode_num_from_path(char *pathname, int dir_inode);
int copy_data_from_inode(void *buf, int inodenum, int offset, int size);
int write_data_to_inode(void *buf, int inodenum, int offset, int size);
char *get_dir_entries(int inum);
int get_inode_in_dir(char *name, int dir_inode_num, int length);
int ReadSector(int sectornum, void *buf);
int get_free_block();
int get_free_inode_num();

int
get_free_block() {
	int i = 1;
	while (blockbitmap[i] && i <= num_blocks) {
		i ++;
	}
	if (i > num_blocks) {
		printf("NO MORE FREE BLOCKS!\n");
		return -1;
	} else {
		blockbitmap[i] = 1;
		return i;
	}
}

// block = 1 -> use block hashtable, otherwise use inode
void
set_dirty(int num, int block) {
	struct list_elem *elem;
	if (block == 1)
		elem = block_hashtable[num % BLOCK_HASHTABLE_SIZE];
	else
		elem = inode_hashtable[num % INODE_HASHTABLE_SIZE];

	if (elem == NULL) {
		printf("TRIED TO SET ELEMENT DIRTY THAT IS NOT IN CACHE!\n");
		return;
	} else {
		while (elem != NULL) {
			struct cache_entry *entry = elem->entry;
			if (entry->num == num) {
				entry->dirty = 1;
				return;
			} else {
				elem = elem->next;
			}
		}
		printf("TRIED TO SET ELEMENT DIRTY THAT IS NOT IN CACHE!\n");
		return;
	}
}

void
free_inode_and_blocks(int inum) {
	struct inode *node = get_inode(inum);
	node->type = INODE_FREE;
	node->size = 0;
	node->nlink = 0;

	int i;
	for (i=0; i<NUM_DIRECT;i++) {
		int blocknum = node->direct[i];
		blockbitmap[blocknum] = 0;
		memset(get_block(blocknum), '\0', SECTORSIZE);
		set_dirty(blocknum, 1);
		node->direct[i] = 0;
	}
	set_dirty(inum, 0);
	//TODO: FREE INDIRECT blocks
}

int
get_free_inode_num() {
	int i = 1;
	while(i <= num_inodes) {
		struct inode *node = get_inode(i);
		if (node->type == INODE_FREE)
			return i;
		i ++;
	}
	return ERROR;
}


// block = 1 -> use block hashtable, otherwise use inode
struct cache_entry*
get_cached_elem(int num, int block) {
	struct list_elem *elem;
	if (block == 1)
		elem = block_hashtable[num % BLOCK_HASHTABLE_SIZE];
	else
		elem = inode_hashtable[num % INODE_HASHTABLE_SIZE];

	if (elem == NULL) {
		return NULL;
	} else {
		while (elem != NULL) {
			struct cache_entry *entry = elem->entry;
			if (entry->num == num) {
				return entry;
			} else {
				elem = elem->next;
			}
		}
		return NULL;
	}
}

void
insert_elem_in_cache(int num, void *data, int block) {

	struct cache_entry *entry = malloc(sizeof(struct cache_entry));
	entry->num = num;
	entry->data = data;
	entry->dirty = 0;



	struct list_elem *newelem = malloc(sizeof(struct list_elem));
	newelem->entry = entry;
	newelem->next = NULL;

	struct list_elem *elem;
	if (block == 1) {
		elem = block_hashtable[num % BLOCK_HASHTABLE_SIZE];
		entry->num_used = num_blocks_cached;
		num_blocks_cached ++;
	} else {
		elem = inode_hashtable[num % INODE_HASHTABLE_SIZE];
		entry->num_used = num_inodes_cached;
		num_inodes_cached ++;
	}

	if (elem == NULL) {
		// No collision
		// printf("No collision on insert\n");
		if (block == 1){
			block_hashtable[num % BLOCK_HASHTABLE_SIZE] = newelem;
			// printf("Put elem at block_hashtable: %d\n", num % BLOCK_HASHTABLE_SIZE);
		}
		else {
			inode_hashtable[num % INODE_HASHTABLE_SIZE] = newelem;
			// printf("Put elem at inode_hashtable index: %d\n", num % INODE_HASHTABLE_SIZE);
		}

	} else {
		// Collision - loop to end of list
		while(elem->next != NULL)
			elem = elem->next;

		elem->next = newelem;
	}

}

int
remove_lru_block() {
	int min = num_blocks_cached + 1;
	int lru_num;
	int i;
	struct list_elem *elem;
	for (i=0; i < BLOCK_HASHTABLE_SIZE; i++) {
		elem = block_hashtable[i];
		while (elem != NULL) {
			if (elem->entry->num < min) {
				lru_num = elem->entry->num;
				min = elem->entry->num_used;
			}
			elem = elem->next;
		}
	}
	// return lru_num

	WriteSector(lru_num, get_block(lru_num));


	elem = block_hashtable[lru_num % BLOCK_HASHTABLE_SIZE];

	if (elem == NULL) {
		printf("Error in remove_lru_block 1\n");
		return -1;
	} else if (elem->entry->num == lru_num) {
		block_hashtable[lru_num % BLOCK_HASHTABLE_SIZE] = NULL;
	} else {
		struct list_elem *next_elem = elem->next;
		while (next_elem != NULL) {
			struct cache_entry *entry = next_elem->entry;
			if (entry->num == lru_num) {
				elem->next = next_elem->next;
				return;
			}
			elem = next_elem;
			next_elem = next_elem->next;
		}
		printf("Error in remove_lru_block 2\n");
		return -1;
	}


}


int
remove_lru_inode() {
	int min = num_inodes_cached + 1;
	int lru_num;
	int i;
	struct list_elem *elem;
	for (i=0; i < INODE_HASHTABLE_SIZE; i++) {
		elem = inode_hashtable[i];
		while (elem != NULL) {
			if (elem->entry->num < min) {
				lru_num = elem->entry->num;
				min = elem->entry->num_used;
			}
			elem = elem->next;
		}
	}

	int blocknum = lru_num * INODESIZE / SECTORSIZE + 1;
	int offset = (lru_num * INODESIZE) % SECTORSIZE;
	struct cache_entry *block_entry = get_cached_elem(blocknum, 1);
	char *block = block_entry->data;
	memcpy(block + offset, get_inode(lru_num), INODESIZE);
	set_dirty(blocknum, 1);

	elem = inode_hashtable[lru_num % INODE_HASHTABLE_SIZE];

	if (elem == NULL) {
		printf("Error in remove_lru_inode 1\n");
		return -1;
	} else if (elem->entry->num == lru_num) {
		inode_hashtable[lru_num % INODE_HASHTABLE_SIZE] = NULL;
	} else {
		struct list_elem *next_elem = elem->next;
		while (next_elem != NULL) {
			struct cache_entry *entry = next_elem->entry;
			if (entry->num == lru_num) {
				elem->next = next_elem->next;
				return lru_num;
			}
			elem = next_elem;
			next_elem = next_elem->next;
		}
		printf("Error in remove_lru_inode 2\n");
		return -1;
	}

}


char *
get_block(int num) {
	struct cache_entry *block_entry = get_cached_elem(num, 1);
	char *block;
	if (block_entry == NULL) {
		// block is not in cache
		// printf("Blocknum %d not in cache\n", num);

		block = malloc(SECTORSIZE); // Allocate space for the block
		ReadSector(num, block); // Read the block into the allocated space

		// Remove LRU block
		if (num_blocks_cached >= BLOCK_CACHESIZE)
			remove_lru_block();

		// Cache the new block
		insert_elem_in_cache(num, block, 1);
	} else {
		block = block_entry->data;
	}
	return block;
}


struct inode*
get_inode(int num) {
	struct cache_entry *node_entry = get_cached_elem(num, 0);
	struct inode *node;
	if (node_entry == NULL){
		// printf("inode num: %d not in cache\n", num);
		// inode is not in cache
		int blocknum = num * INODESIZE / SECTORSIZE + 1;
		int offset = (num * INODESIZE) % SECTORSIZE;
		node = malloc(INODESIZE);
		read_with_offset(blocknum, node, offset, INODESIZE);

		// Remove LRU block
		if (num_inodes_cached >= INODE_CACHESIZE)
			remove_lru_inode();

		// Cache the new block
		insert_elem_in_cache(num, node, 0);
		if (((struct cache_entry*)(get_cached_elem(num, 0)))->data == NULL)
			printf("Cached elem is null just after caching\n");
	} else {
		node = node_entry->data;
	}
	return node;
}



void
read_with_offset(int sectornum, void *buf, int offset, int size) {
	char *temp = get_block(sectornum);
	memcpy(buf, temp + offset, size);
}


int
get_inode_num_from_path(char *pathname, int dir_inode_num) {
	printf("%s\n", pathname);
	if (strlen(pathname) == 0) {
		// printf("%s\n", "Pathname has length 0");
		// return ERROR;
		return dir_inode_num;
	}


	// char* token = strtok(pathname, "/");
	char* currstr = pathname;
	char* nextslash = strchr(pathname, '/');

	if (nextslash == pathname) {
		dir_inode_num = ROOTINODE;
		currstr = pathname + 1;
		nextslash = strchr(currstr, '/');
	}

	int length;


	// printf("token: %s\n", token);
	// struct inode *dirnode = get_inode(dir_inode_num);
	// printf("dir_inode_num: %d\tdirnode->type: %d\n", dir_inode_num, dirnode->type);
	int inum = dir_inode_num;

	while (nextslash != NULL) {// && dirnode->type == INODE_DIRECTORY) {
		// printf("Token: %s\n", token);
		length = nextslash - currstr;
		inum = get_inode_in_dir(currstr, inum, length);
		if (inum == -1) {
			printf("File not found\n");
			return -1;
		} else if (inum == -2) {
			printf("Non-directory file on path\n");
			return -1;
		}

		currstr = nextslash + 1;

		nextslash = strchr(currstr, '/');
    }

    if (strlen(currstr) == 0) {
    	// Path terminated in /
    	if (get_inode(inum)->type != INODE_DIRECTORY) {
    		printf("Pathname ends in '/' but inode is not a directory\n");
    		return -1;
    	} else {
    		return inum;
    	}
    } else {
    	// Path not terminated in /
    	inum = get_inode_in_dir(currstr, inum, strlen(currstr));

    	if (inum == -1) {
			printf("File not found\n");
			return -1;
		} else if (inum == -2) {
			printf("Non-directory file on path\n");
			return -1;
		}

		return inum;
    	// if (get_inode(inum)->type != INODE_REGULAR) {
    	// 	printf("Pathname does not end in '/' but inode is not a regular file\n");
    	// 	return -1;
    	// } else {
    	// 	return inum;
    	// }
    }

	return inum;
}


int
copy_data_from_inode(void *buf, int inodenum, int offset, int size) {
	// printf("COPYING DATA FROM INODE num: %d\toffset: %d\tsize: %d\n", inodenum, offset, size);
	struct inode *node = get_inode(inodenum);
	int size_copied = 0;

	if (offset + size > node->size)
		size = node->size - offset;

	//FIND FIRST BLOCK TO USE and OFFSET
	int *indirect_blocks = get_block(node->indirect);
	int num_direct_block = offset / SECTORSIZE;
	// if (node->size > SECTORSIZE * NUM_DIRECT)
	// 	int indirect_blocks[SECTORSIZE / sizeof(int)] = ;
	offset = offset % SECTORSIZE;
	if (num_direct_block < NUM_DIRECT) {

	}
	int blocknum;
	if (num_direct_block < NUM_DIRECT)
		blocknum = node->direct[num_direct_block];
	else
		blocknum = indirect_blocks[num_direct_block - NUM_DIRECT];

	char *block = get_block(blocknum);

	// COPY FROM FIRST BLOCK
	if (offset + size <= SECTORSIZE) {
		// just copy part of the first block starting at offset
		if (blocknum == 0) {
			memset(buf + offset, '\0', size);
		}
		else
			memcpy(buf, block + offset, size);
		return size;

	} else {
		// copy whole first block starting at offset
		if (blocknum == 0)
			memset(buf + offset, '\0', SECTORSIZE - offset);
		else
			memcpy(buf, block + offset, SECTORSIZE - offset);
		size_copied = SECTORSIZE - offset;
		num_direct_block ++;
	}

	// COPY FROM FULL BLOCKS
	while(size - size_copied >= SECTORSIZE) {
		// copy the whole block
		if (num_direct_block < NUM_DIRECT)
			blocknum = node->direct[num_direct_block];
		else
			blocknum = indirect_blocks[num_direct_block - NUM_DIRECT];
		if (blocknum == 0)
			memset(buf + size_copied, '\0', SECTORSIZE);
		else {
			block = get_block(blocknum);
			memcpy(buf + size_copied, block, SECTORSIZE);
		}
		size_copied = SECTORSIZE - offset;
		num_direct_block ++;
	}

	// // COPY FROM LAST BLOCK
	// if (num_direct_block != NUM_DIRECT) {
	// 	// COPY FROM LAST BLOCK

	if (num_direct_block < NUM_DIRECT)
		blocknum = node->direct[num_direct_block];
	else
		blocknum = indirect_blocks[num_direct_block - NUM_DIRECT];
	if (blocknum == 0) {
		memset(buf + size_copied, '\0', size - size_copied);
	} else {
		block = get_block(blocknum);
		memcpy(buf + size_copied, block, size - size_copied);
	}
	return size;
}


int
write_data_to_inode(void *buf, int inodenum, int offset, int size) {
	printf("WRITING DATA TO INODE num: %d\toffset: %d\tsize: %d\n", inodenum, offset, size);
	set_dirty(inodenum, 0);
	struct inode *node = get_inode(inodenum);
	if (offset + size > node->size)
		node->size = offset + size;
	int size_written = 0;

	//FIND FIRST BLOCK TO USE and OFFSET
	int blocknum;
	int *indirect_blocks = get_block(node->indirect);
	int num_direct_block = offset / SECTORSIZE;
	offset = offset % SECTORSIZE;

	if (num_direct_block < NUM_DIRECT)
		blocknum = node->direct[num_direct_block];
	else
		blocknum = indirect_blocks[num_direct_block - NUM_DIRECT];
	if (blocknum == 0) {
		blocknum = get_free_block();
		if (num_direct_block < NUM_DIRECT)
			node->direct[num_direct_block] = blocknum;
		else
			indirect_blocks[blocknum - NUM_DIRECT]  = get_free_block();
	}
	char *block = get_block(blocknum);
	// COPY FROM FIRST BLOCK
	if (offset + size <= SECTORSIZE) {
		// printf("here1\n");
		// just copy part of the first block starting at offset
		memcpy(block + offset, buf, size);
		set_dirty(blocknum, 1);
		return size;

	} else {
		// copy whole first block starting at offset
		memcpy(block + offset, buf, SECTORSIZE - offset);
		set_dirty(blocknum, 1);
		size_written = SECTORSIZE - offset;
		num_direct_block ++;
	}

	// COPY FROM FULL BLOCKS
	while(size - size_written >= SECTORSIZE) {
		// copy the whole block
		if (num_direct_block < NUM_DIRECT)
			blocknum = node->direct[num_direct_block];
		else
			blocknum = indirect_blocks[num_direct_block - NUM_DIRECT];
		if (blocknum == 0) {
			blocknum = get_free_block();
			if (num_direct_block < NUM_DIRECT)
				node->direct[num_direct_block] = blocknum;
			else
				indirect_blocks[blocknum - NUM_DIRECT]  = get_free_block();
		}
		block = get_block(blocknum);
		memcpy(block, buf + size_written, SECTORSIZE);
		set_dirty(blocknum, 0);
		size_written = SECTORSIZE - offset;
		num_direct_block ++;
	}

	// // COPY FROM LAST BLOCK / INDIRECT
	// if (num_direct_block != NUM_DIRECT) {
	// 	// COPY FROM LAST BLOCK
	if (num_direct_block < NUM_DIRECT)
		blocknum = node->direct[num_direct_block];
	else
		blocknum = indirect_blocks[num_direct_block - NUM_DIRECT];
	if (blocknum == 0) {
		blocknum = get_free_block();
		if (num_direct_block < NUM_DIRECT)
			node->direct[num_direct_block] = blocknum;
		else
			indirect_blocks[blocknum - NUM_DIRECT]  = get_free_block();
	}
	block = get_block(blocknum);
	memcpy(block, buf + size_written, size - size_written);
	set_dirty(blocknum, 0);
	return size;

	// check if on indirect or on last block
}


char *
get_dir_entries(int inum) {
	// struct inode *dirnode = get_inode(num);
	struct inode *dirnode = get_inode(inum);
	if (dirnode->type != INODE_DIRECTORY) {
		printf("%s\n", "inode num given is not a directory");
		return NULL;
	}

	// int num_entries = dirnode->size / sizeof(dir_entry);
	char *dir_entries = malloc(dirnode->size);
	copy_data_from_inode(dir_entries, inum, 0, dirnode->size);

	return dir_entries;
}

void
add_dir_entry(int dir_inode_num, struct dir_entry *new_entry) {
	struct inode *dirnode = get_inode(dir_inode_num);
	char *dir_entries = get_dir_entries(dir_inode_num);
	int offset;
	for (offset=0; offset < dirnode->size; offset += sizeof(struct dir_entry)) {
		struct dir_entry *entry = (struct dir_entry*)&dir_entries[offset];
		int inum = entry->inum;
		if (inum == 0) {
			write_data_to_inode(new_entry, dir_inode_num, offset, sizeof(struct dir_entry));
			return;
		}
	}
	write_data_to_inode(new_entry, dir_inode_num, dirnode->size, sizeof(struct dir_entry));
	return;
}


void
remove_dir_entry(int dir_inode_num, int rm_inum) {
	struct inode *dirnode = get_inode(dir_inode_num);
	char *dir_entries = get_dir_entries(dir_inode_num);
	struct dir_entry *empty_dir_entry = malloc(sizeof(struct dir_entry));
	empty_dir_entry->inum = 0;
	int offset;
	for (offset=0; offset < dirnode->size; offset += sizeof(struct dir_entry)) {
		struct dir_entry *entry = (struct dir_entry*)&dir_entries[offset];
		int inum = entry->inum;
		if (inum == rm_inum) {
			write_data_to_inode(empty_dir_entry, dir_inode_num, offset, sizeof(struct dir_entry));
			return;
		}
	}
	printf("Inode to remove: %d does not exist in directory: %d\n", rm_inum, dir_inode_num);
	return;
}


int
get_inode_in_dir(char *name, int dir_inode_num, int length) {
	printf("GETTING INODE IN DIR  %s\n", name);

	struct inode *dirnode = get_inode(dir_inode_num);
	if (dirnode->type != INODE_DIRECTORY) {
		printf("Dir_inode_num given: %d is not a directory\n", dir_inode_num);
		return -2;
	}

	char *dir_entries = get_dir_entries(dir_inode_num);
	// int namelength = strlen(name);
	int offset = 0;
	while (offset < dirnode->size) {
		struct dir_entry *entry = (struct dir_entry*)&dir_entries[offset];
		int inum = entry->inum;

		if (strncmp(name, entry->name, length) == 0 && inum != 0) {
			return inum;
		}
		offset += sizeof(struct dir_entry);
	}
	printf("No file with name: '%s' was found.\n", name);
	return ERROR; // NO FILE FOUND
}



int
_Open(char *pathname, int current_inode) {
	char *filename;
    char *dirname;

	printf("Opening in yfs\n");
	if (pathname[0] == '/') {
         current_inode = ROOTINODE;
    }
    if (pathname[strlen(pathname) - 1] == '/') {
    	char *newpath = malloc(strlen(pathname) + 2);
    	memcpy(newpath, pathname, strlen(pathname));
    	newpath[strlen(pathname)] = '.';
    	newpath[strlen(pathname) + 1] = '\0';
    	pathname = newpath;
    }

    if (strchr(pathname, '/') == NULL) {
    	filename = pathname;
    	dirname = "";
    } else {
    	filename = strrchr(pathname, '/') + 1;
	    printf("%ld\n", &filename - &pathname + 1);
	    dirname = malloc(&filename - &pathname + 1);
	    memcpy(dirname, pathname, &filename - &pathname);
	    dirname[filename-pathname] = '\0';
    }


    int directory_inum = get_inode_num_from_path(dirname, current_inode);
	printf("Directory inum: %d\n", directory_inum);
	char *dir_entries = get_dir_entries(directory_inum);

	if (dir_entries == NULL) {
		return -1;
	}

    printf("Current inode: %d\tPathname: %s\n", current_inode, pathname);
    // int inum = get_inode_num_from_path(pathname, current_inode);
    int inum = get_inode_in_dir(filename, directory_inum, strlen(filename));
    printf("Done opening in yfs with inum: %d\n", inum);
    return inum;
}


int _Create(char *pathname, int current_inode) {
	if (current_inode == 0) {
		return ERROR;
	}
	if (pathname[0] == '/') {
         current_inode = ROOTINODE;
    }
    if (pathname[strlen(pathname) - 1] == '/') {
    	printf("Cannot Create . file\n");
    	return ERROR;
    }

    char *filename;
    char *dirname;

    if (strchr(pathname, '/') == NULL) {
    	filename = pathname;
    	dirname = "";
    } else {
    	filename = strrchr(pathname, '/') + 1;
	    printf("%ld\n", &filename - &pathname + 1);
	    dirname = malloc(&filename - &pathname + 1);
	    memcpy(dirname, pathname, &filename - &pathname);
	    dirname[filename-pathname] = '\0';
    }

    printf("Dirname: %s, dirnamelen: %d\n", dirname, (int)strlen(dirname));
    printf("Filename: %s, filenamelen: %d\n", filename, (int)strlen(filename));


	if (strlen(filename) > DIRNAMELEN){
		printf("Filename too long!");
		return -1;
	}
	printf("Creating file: %s in directory %s from pathname %s\n", filename, dirname, pathname);

	int directory_inum = get_inode_num_from_path(dirname, current_inode);
	struct inode *dir_inode = get_inode(directory_inum);
	if (dir_inode->type != INODE_DIRECTORY) {
		return ERROR;
	}
	printf("Directory inum: %d\n", directory_inum);
	char *dir_entries = get_dir_entries(directory_inum);

	if (dir_entries == NULL) {
		return -1;
	}




	int current_inode_num = get_inode_in_dir(filename, directory_inum, strlen(filename));
	if (current_inode_num != ERROR) {
		struct inode *old_inode = get_inode(current_inode_num);
		if (old_inode->type == INODE_DIRECTORY) {
			printf("You tried to create file with same name as directory!\n");
			return ERROR;
		} else {
			old_inode->size = 0;
			int i;
			for (i=0; i<NUM_DIRECT; i++) {
				old_inode->direct[i] = 0;
			}
			old_inode->indirect = 0;
			return current_inode_num;
		}
	}

    // Create inode
    int new_inum = get_free_inode_num();
    struct inode *node = get_inode(new_inum);
    node->type = INODE_REGULAR;
    node->size = 0;
    node->nlink = 1;
    node->direct[0] = get_free_block();
    node->reuse ++;
    set_dirty(new_inum, 0);

    // Create dir_entry
	struct dir_entry *dir_entry = malloc(sizeof(dir_entry));
    dir_entry->inum = new_inum;
    int i;
    for (i=0; i<strlen(filename); i++) {
    	dir_entry->name[i] = filename[i];
    }
    if (strlen(filename) < DIRNAMELEN){
    	dir_entry->name[strlen(filename)] = (char)'\0';
    }

    add_dir_entry(directory_inum, dir_entry);

    for (i = 0; filename[i] != '\0'; i++) {
        dir_entry->name[i] = filename[i];
    }

    //TODO: add stuff to cache probably?
    return new_inum;
}

int
_Read(int inum, char *buf, int offset, int size) {
	return copy_data_from_inode(buf, inum, offset, size);
}

int
_Write(int inum, char *buf, int offset, int size) {
	struct inode *node = get_inode(inum);
	if (node->type == INODE_DIRECTORY) {
		printf("Cannot write to directory file\n");
		return ERROR;
	}
	return write_data_to_inode(buf, inum, offset, size);
}


int
_Link(char *oldname, char *newname, int current_inode) {
	if (current_inode == 0) {
		return ERROR;
	}

	if (oldname[strlen(oldname) - 1] == '/') {
    	char *newpath = malloc(strlen(oldname) + 2);
    	memcpy(newpath, oldname, strlen(oldname));
    	newpath[strlen(oldname)] = '.';
    	newpath[strlen(oldname) + 1] = '\0';
    	oldname = newpath;
    }

    printf("Current inode: %d\tOldname: %s\n", current_inode, oldname);
	int old_inode_num = get_inode_num_from_path(oldname, current_inode);
	if (old_inode_num == -1) {
		printf("No file %s exists\n", oldname);
	}
	struct inode *old_inode = get_inode(old_inode_num);
	if (old_inode->type == INODE_DIRECTORY) {
		printf("You cannot link to a directory!\n");
		return ERROR;
	}



	if (newname[0] == '/') {
         current_inode = ROOTINODE;
    }
    if (newname[strlen(newname) - 1] == '/') {
    	printf("Cannot Create . file\n");
    	return ERROR;
    }

    char *filename;
    char *dirname;

    if (strchr(newname, '/') == NULL) {
    	filename = newname;
    	dirname = "";
    } else {
    	filename = strrchr(newname, '/') + 1;
	    printf("%ld\n", &filename - &newname + 1);
	    dirname = malloc(&filename - &newname + 1);
	    memcpy(dirname, newname, &filename - &newname);
	    dirname[filename-newname] = '\0';
    }


    printf("Dirname: %s, dirnamelen: %d\n", dirname, (int)strlen(dirname));
    printf("Filename: %s, filenamelen: %d\n", filename, (int)strlen(filename));

    struct dir_entry *new_dir_entry = malloc(sizeof(struct dir_entry));
    new_dir_entry->inum = old_inode_num;
    int i;
	for (i=0; i<strlen(filename); i++) {
    	new_dir_entry->name[i] = filename[i];
    }
    if (strlen(filename) < DIRNAMELEN){
    	new_dir_entry->name[strlen(filename)] = (char)'\0';
    }
    int dir_inum = get_inode_num_from_path(dirname, current_inode);
    add_dir_entry(dir_inum, new_dir_entry);

	old_inode->nlink++;
	set_dirty(old_inode_num, 0);
	return 0;
}

int
_UnLink(char *pathname, int current_inode) {
	if (current_inode == 0) {
		return ERROR;
	}
	if (pathname[0] == '/') {
         current_inode = ROOTINODE;
    }
    if (pathname[strlen(pathname) - 1] == '/') {
    	char *newpath = malloc(strlen(pathname) + 2);
    	memcpy(newpath, pathname, strlen(pathname));
    	newpath[strlen(pathname)] = '.';
    	newpath[strlen(pathname) + 1] = '\0';
    	pathname = newpath;
    }

    char *filename;
    char *dirname;

    if (strchr(pathname, '/') == NULL) {
    	filename = pathname;
    	dirname = "";
    } else {
    	filename = strrchr(pathname, '/') + 1;
	    printf("%ld\n", &filename - &pathname + 1);
	    dirname = malloc(&filename - &pathname + 1);
	    memcpy(dirname, pathname, &filename - &pathname);
	    dirname[filename-pathname] = '\0';
    }

    int directory_inum = get_inode_num_from_path(dirname, current_inode);
    // struct node *directory_inode = get_inode(directory_inum);
    if (directory_inum == 0) {
    	return ERROR;
    }
	printf("Directory inum: %d\n", directory_inum);
	char *dir_entries = get_dir_entries(directory_inum);

	if (dir_entries == NULL) {
		return -1;
	}

	int unlinking_inum = get_inode_in_dir(filename, directory_inum, strlen(filename));

	struct inode *unlinking_inode = get_inode(unlinking_inum);

	if (unlinking_inode->nlink > 1) {
		//remove from containing directory
		// dir_entries->inum = 0;
		// memset(dir_entries->name,'\0',DIRNAMELEN);
		// directory_inode->size -= sizeof(dir_entry);
		// set_dirty(directory_inum, 0);

		remove_dir_entry(directory_inum, unlinking_inum);

		//changing the inode
		unlinking_inode->nlink--;
		set_dirty(unlinking_inum, 0);
	}

	else if (unlinking_inode->nlink == 1){
		// dir_entries->inum = 0;
		// memset(dir_entries->name,'\0',DIRNAMELEN);
		// directory_inode->size -= sizeof(dir_entry);
		// set_dirty(directory_inum, 0);

		remove_dir_entry(directory_inum, unlinking_inum);
		free_inode_and_blocks(unlinking_inum);
	}

	else {
		return ERROR;
	}

	return 0;

}


int
_SymLink(char *oldname, char *newname, int current_inode) {
	if (current_inode == 0) {
		return ERROR;
	}

	int new_inum = _Create(newname, current_inode);
	if (new_inum == -1) {
		return ERROR;
	}
	write_data_to_inode(oldname, new_inum, 0, strlen(oldname));
	return 0;

}

int
_ReadLink(char *pathname, char *buf, int len, int current_inode, int sender_pid) {
	if (pathname[0] == '/') {
         current_inode = ROOTINODE;
    }

    int symlink_inum = get_inode_num_from_path(pathname, current_inode);

    if (symlink_inum == 0) {
    	return ERROR;
    }

   struct inode *symlink_inode = get_inode(symlink_inum);
   int symlink_blocknum = symlink_inode->direct[0];
   char *symlink_block = get_block(symlink_blocknum);

   int i;
   for (i = 0; i < len; i++) {
   		if (symlink_block[i] == '\0') {
   			break;
   		}
   }

   int check = CopyTo(sender_pid, buf, symlink_block, i);
   if (check == ERROR) {
   		return ERROR;
   }

   return i;

}

int
_MkDir(char *pathname, int current_inode) {
	if (pathname[0] == '/') {
         current_inode = ROOTINODE;
    }
    if (pathname[strlen(pathname) - 1] == '/') {
    	char *newpath = malloc(strlen(pathname) + 2);
    	memcpy(newpath, pathname, strlen(pathname));
    	newpath[strlen(pathname)] = '.';
    	newpath[strlen(pathname) + 1] = '\0';
    	pathname = newpath;
    }

    char *filename;
    char *dirname;

    if (strchr(pathname, '/') == NULL) {
    	filename = pathname;
    	dirname = "";
    } else {
    	filename = strrchr(pathname, '/') + 1;
	    printf("%ld\n", &filename - &pathname + 1);
	    dirname = malloc(&filename - &pathname + 1);
	    memcpy(dirname, pathname, &filename - &pathname);
	    dirname[filename-pathname] = '\0';
    }

    printf("Dirname: %s, dirnamelen: %d\n", dirname, (int)strlen(dirname));
    printf("Filename: %s, filenamelen: %d\n", filename, (int)strlen(filename));

	if (strlen(filename) > DIRNAMELEN){
		printf("Filename too long!");
		return -1;
	}

	int directory_inum = get_inode_num_from_path(dirname, current_inode);
	printf("Directory inum: %d\n", directory_inum);
	// char *dir_entries = get_dir_entries(directory_inum);


	if (get_inode_in_dir(filename, directory_inum, strlen(filename)) != ERROR) {
		printf("%s\n", "Attempting to create duplicate directory!");
		return ERROR;
	}



	int new_inum = get_free_inode_num();
    struct inode *new_inode = get_inode(new_inum);
    set_dirty(new_inum, 0);
    new_inode->type = INODE_DIRECTORY;
    new_inode->nlink = 2;
    new_inode->size = 2 * sizeof(struct dir_entry);

    int new_blocknum = get_free_block();
    char* new_block = get_block(new_blocknum);
    new_inode->direct[0] = new_block;


    struct dir_entry *dir_entry1 = malloc(sizeof(struct dir_entry));
	int i;
	for (i=0; i<strlen(filename); i++) {
    	dir_entry1->name[i] = filename[i];
    }
    if (strlen(filename) < DIRNAMELEN){
    	dir_entry1->name[strlen(filename)] = (char)'\0';
    }

    dir_entry1->inum = new_inum;
    add_dir_entry(directory_inum, dir_entry1);

    struct dir_entry *dir_entry2 = malloc(sizeof(struct dir_entry));
    struct dir_entry *dir_entry3 = malloc(sizeof(struct dir_entry));

    dir_entry2->name[0] = '.';
    dir_entry2->name[1] = '\0';
    dir_entry2->inum = new_inum;

    dir_entry3->name[0] = '.';
    dir_entry3->name[1] = '.';
    dir_entry3->name[2] = '\0';
    dir_entry3->inum = directory_inum;

    add_dir_entry(new_inum, dir_entry2);
    add_dir_entry(new_inum, dir_entry3);

    return 0;
}

int
_RmDir(char *pathname, int current_inode) {
	if (current_inode == 0) {
		return ERROR;
	}
	if (pathname[0] == '/') {
         current_inode = ROOTINODE;
    }
    if (pathname[strlen(pathname) - 1] == '/') {
    	printf("Cannot remove . directory\n");
    	return -1;
    }
    if (pathname[strlen(pathname) - 1] == '.') {
    	printf("Cannot remove . or .. directory\n");
    	return -1;
    }

    char *filename;
    char *dirname;

    if (strchr(pathname, '/') == NULL) {
    	filename = pathname;
    	dirname = "";
    } else {
    	filename = strrchr(pathname, '/') + 1;
	    printf("%ld\n", &filename - &pathname + 1);
	    dirname = malloc(&filename - &pathname + 1);
	    memcpy(dirname, pathname, &filename - &pathname);
	    dirname[filename-pathname] = '\0';
    }

    printf("Dirname: %s, dirnamelen: %d\n", dirname, (int)strlen(dirname));
    printf("Filename: %s, filenamelen: %d\n", filename, (int)strlen(filename));

    int upper_directory_inum = get_inode_num_from_path(dirname, current_inode);
	printf("Upper directory inum: %d\n", upper_directory_inum);
	// char *upper_dir_entries = get_dir_entries(upper_directory_inum);

	int rm_directory_inum = get_inode_in_dir(filename, upper_directory_inum, strlen(filename));
	if (rm_directory_inum == -1) {
		printf("Directory does not exist\n");
		return ERROR;
	}
	struct inode *rm_dir_inode = get_inode(rm_directory_inum);
	char *rm_dir_entries = get_dir_entries(rm_directory_inum);


	int offset;
	for (offset=0; offset < rm_dir_inode->size; offset += sizeof(struct dir_entry)) {
		struct dir_entry *entry = (struct dir_entry*)&rm_dir_entries[offset];
		int inum = entry->inum;
		if (inum != 0) {
			printf("Directory %d is not empty and cannot be removed (still has inode %d)!\n", rm_directory_inum, inum);
			return ERROR;
		}
	}

	remove_dir_entry(upper_directory_inum, rm_directory_inum);
	free_inode_and_blocks(rm_directory_inum);
	return 0;

}

struct Stat*
_Stat(char *pathname, int current_inode_num) {
	int inum = get_inode_num_from_path(pathname, current_inode_num);
	struct inode *node = get_inode(inum);
	struct Stat *statbuf = malloc(sizeof(struct Stat));
	statbuf->inum = inum;
	statbuf->type = node->type;
	statbuf->size = node->size;
	statbuf->nlink = node->nlink;
	return statbuf;
}


int
_Sync() {
	int i;
	for (i=0; i < INODE_HASHTABLE_SIZE; i++) {
		struct list_elem* elem = inode_hashtable[i];
		while (elem != NULL) {
			if (elem->entry->dirty == 1) {
				elem->entry->dirty = 0;
				int blocknum = elem->entry->num * INODESIZE / SECTORSIZE + 1;
				int offset = (elem->entry->num * INODESIZE) % SECTORSIZE;
				struct cache_entry *block_entry = get_cached_elem(blocknum, 1);
				char *block = block_entry->data;
				memcpy(block + offset, elem->entry->data, INODESIZE);
				block_entry->dirty = 1;
			}
			elem = elem->next;
		}
	}
	for (i=0; i < BLOCK_HASHTABLE_SIZE; i++) {
		struct list_elem *elem = block_hashtable[i];
		while (elem != NULL) {
			if (elem->entry->dirty == 1) {
				WriteSector(elem->entry->num, elem -> entry -> data);
			}
			elem = elem->next;
		}
	}
	return 0;
}

int
_ChDir(char *pathname, int current_inode) {
	if (pathname[0] == '/') {
         current_inode = ROOTINODE;
    }
	if (pathname[strlen(pathname) - 1] == '/') {
    	char *newpath = malloc(strlen(pathname) + 2);
    	memcpy(newpath, pathname, strlen(pathname));
    	newpath[strlen(pathname)] = '.';
    	newpath[strlen(pathname) + 1] = '\0';
    	pathname = newpath;
    }
	printf("Changing directory to: %s\n", pathname);
	int inum = get_inode_num_from_path(pathname, current_inode);
	if (get_inode(inum)->type != INODE_DIRECTORY) {
		printf("Requested pathname is not a directory\n");
		return -1;
	}
	return inum;
}


int
_Shutdown() {
	_Sync();
	printf("%s\n", "File system shutting down...");
	Exit(0);
}

int
main(int argc, char **argv) {
	// Read FS HEADER
	Register(FILE_SERVER);
	read_with_offset(1, &header, 0, INODESIZE);
	num_inodes = header.num_inodes;
	num_blocks = header.num_blocks;


	// Initialize block bitmap
	blockbitmap = malloc(num_blocks * sizeof(int));
	blockbitmap[0] = 1;
	int num_blocks_used = (1 + num_inodes) * sizeof(struct inode) / SECTORSIZE;
	if (((1 + num_inodes) * sizeof(struct inode)) % SECTORSIZE > 0) {
		num_blocks_used ++;
	}


	int i;
	for (i = 0; i < num_blocks_used; i ++) {
		blockbitmap[i + 1] = 1;
	}

	printf("Num_inodes: %d\t Num_blocks: %d\n", num_inodes, num_blocks);

	if (argc > 1) {
		if (Fork() == 0) {
			// Child
			Exec(argv[1], argv + 1);
		}
	}

	struct my_msg1 *msg = malloc(sizeof(struct my_msg1));

	while(1) {
		printf("Receiving\n");
		int senderid = Receive(msg);
		if (senderid == 0) {
			printf("DEADLOCK\n");
			_Shutdown();
		}
		printf("Done receiving from pid: %d, message type: %d\n", senderid, msg->type);
		if (msg->type == OPEN) {
			printf("Received message OPEN\n");
			struct my_msg2 *msg2 = (struct my_msg2*)msg;
			char *pathname = malloc(msg2->data2);
			int len = msg2->data1;
			int dir_inode_num = msg2->data2;
			CopyFrom(senderid, pathname, msg2->ptr, len);
			int inum = _Open(pathname, dir_inode_num);
			struct my_msg1 *msg = malloc(sizeof(struct my_msg2));
			msg->data1 = inum;
			printf("Replying with inum: %d\n", inum);
			Reply(msg ,senderid);
		} else if (msg->type == CLOSE) {
			printf("Received message CLOSE\n");
		} else if (msg->type == CREATE) {
			printf("Received message CREATE\n");
			struct my_msg2 *msg2 = (struct my_msg2*)msg;
			char *pathname = malloc(msg2->data2);
			int len = msg2->data1;
			int dir_inode_num = msg2->data2;
			CopyFrom(senderid, pathname, msg2->ptr, len);
			printf("Pathname: %s\n", pathname);
			int inum = _Create(pathname, dir_inode_num);
			struct my_msg1 *msg = malloc(sizeof(struct my_msg2));
			msg->data1 = inum;
			printf("Replying with inum: %d\n", inum);
			Reply(msg, senderid);
		} else if (msg->type == READ) {
			printf("Received message READ\n");
			struct my_msg2 *msg2 = (struct my_msg2*)msg;
			int inum = msg2->data1;
			int size = msg2->data2;
			int offset = msg2->data3;
			char *readbuf = malloc(size);
			int result = _Read(inum, *readbuf, offset, size);
			CopyTo(senderid, readbuf, msg2->ptr, size);
			struct my_msg1 *msg = malloc(sizeof(struct my_msg2));
			msg->data1 = result;
			printf("Replying with result: %d\n", result);
			Reply(msg, senderid);
		} else if (msg->type == WRITE) {
			printf("Received message WRITE\n");
			struct my_msg2 *msg2 = (struct my_msg2*)msg;
			int inum = msg2->data1;
			int size = msg2->data2;
			int offset = msg2->data3;
			char *writebuf = malloc(size);
			CopyFrom(senderid, writebuf, msg2->ptr, size);
			int result = _Write(inum, *writebuf, offset, size);
			struct my_msg1 *msg = malloc(sizeof(struct my_msg2));
			msg->data1 = result;
			printf("Replying with result: %d\n", result);
			Reply(msg, senderid);
		} else if (msg->type == SEEK) {
			printf("Received message WRITE\n");
			struct my_msg2 *msg2 = (struct my_msg2*)msg;
			int inum = msg2->data1;
			int size = get_inode(inum)->size;
			struct my_msg1 *msg = malloc(sizeof(struct my_msg2));
			msg->data1 = size;
			printf("Replying with size: %d\n", size);
			Reply(msg, senderid);
		} else if (msg->type == LINK) {
			printf("Received message LINK\n");
			struct my_msg4 *msg4 = (struct my_msg4*)msg;
			int len_oldname = msg4->len_oldname;
			char *oldname = malloc(len_oldname);
			int len_newname = msg4->len_newname;
			char *newname = malloc(len_newname);
			int dir_inode_num = msg4->cur_inode;
			CopyFrom(senderid, oldname, msg4->oldname, len_oldname);
			CopyFrom(senderid, newname, msg4->newname, len_newname);
			printf("Old name: %s\n", oldname);
			printf("New name: %s\n", newname);
			_Link(oldname, newname, dir_inode_num);
			struct my_msg4 *msg = malloc(sizeof(struct my_msg4));
			msg->type = LINK;
			printf("Replying with to Link call...\n");
			Reply(msg, senderid);
		} else if (msg->type == UNLINK) {
			printf("Received message UNLINK\n");
			struct my_msg2 *msg2 = (struct my_msg2*)msg;
			char *pathname = malloc(msg2->data2);
			int len = msg2->data1;
			int dir_inode_num = msg2->data2;
			CopyFrom(senderid, pathname, msg2->ptr, len);
			printf("Pathname: %s\n", pathname);
			int inum = _UnLink(pathname, dir_inode_num);
			struct my_msg1 *msg = malloc(sizeof(struct my_msg2));
			msg->data1 = inum;
			printf("Replying with inum: %d\n", inum);
			Reply(msg, senderid);
		} else if (msg->type == SYMLINK) {
			printf("Received message SYMLINK\n");
			struct my_msg4 *msg4 = (struct my_msg4*)msg;
			int len_oldname = msg4->len_oldname;
			char *oldname = malloc(len_oldname);
			int len_newname = msg4->len_newname;
			char *newname = malloc(len_newname);
			int dir_inode_num = msg4->cur_inode;
			CopyFrom(senderid, oldname, msg4->oldname, len_oldname);
			CopyFrom(senderid, newname, msg4->newname, len_newname);
			printf("Old name: %s\n", oldname);
			printf("New name: %s\n", newname);
			_Link(oldname, newname, dir_inode_num);
			struct my_msg4 *msg = malloc(sizeof(struct my_msg4));
			msg->type = SYMLINK;
			printf("Replying with to SymLink call...\n");
			Reply(msg, senderid);
		} else if (msg->type == READLINK) {
			printf("%s\n", "Received message READLINK");
			struct my_msg3 *msg3 = (struct my_msg3*)msg;
			char *pathname = malloc(msg3->data);
			char *buf = malloc(msg3->len);
			int len = msg3->len;
			int dir_inode_num = msg3->cur_inode;
			CopyFrom(senderid, pathname, msg3->ptr, len);
			int readlink_count = _ReadLink(pathname, buf, len, dir_inode_num, senderid);
			struct my_msg3 *msg = malloc(sizeof(struct my_msg3));
			msg->type = READLINK;
			msg->len = readlink_count;
			msg->buf = buf;
			Reply(msg, senderid);
		} else if (msg->type == MKDIR) {
			printf("Received message MKDIR\n");
			struct my_msg2 *msg2 = (struct my_msg2*)msg;
			char *pathname = malloc(msg2->data2);
			int len = msg2->data1;
			int dir_inode_num = msg2->data2;
			CopyFrom(senderid, pathname, msg2->ptr, len);
			printf("Pathname: %s\n", pathname);
			int inum = _MkDir(pathname, dir_inode_num);
			struct my_msg1 *msg = malloc(sizeof(struct my_msg2));
			msg->data1 = inum;
			printf("Replying with inum: %d\n", inum);
			Reply(msg, senderid);
		} else if (msg->type == RMDIR) {
			printf("Received message RMDIR\n");
			struct my_msg2 *msg2 = (struct my_msg2*)msg;
			char *pathname = malloc(msg2->data2);
			int len = msg2->data1;
			int dir_inode_num = msg2->data2;
			CopyFrom(senderid, pathname, msg2->ptr, len);
			printf("Pathname: %s\n", pathname);
			_RmDir(pathname, dir_inode_num);
			struct my_msg1 *msg = malloc(sizeof(struct my_msg2));
			msg->type = RMDIR;
			printf("Replying to RmDir call...\n");
			Reply(msg, senderid);
		} else if (msg->type == CHDIR) {
			printf("Received message CHDIR\n");
			struct my_msg2 *msg2 = (struct my_msg2*)msg;
			char *pathname = malloc(msg2->data2);
			int len = msg2->data1;
			int dir_inode_num = msg2->data2;
			CopyFrom(senderid, pathname, msg2->ptr, len);
			printf("Pathname: %s\n", pathname);
			int inum = _ChDir(pathname, dir_inode_num);
			struct my_msg1 *msg = malloc(sizeof(struct my_msg2));
			msg->data1 = inum;
			printf("Replying with inum: %d\n", inum);
			Reply(msg, senderid);
		} else if (msg->type == STAT) {
			struct my_msg3 *msg = malloc(sizeof(struct my_msg3));
			int len = msg->len;
			int cur_inode = msg->cur_inode;
			int data;
			char *pathname = malloc(len);
			CopyFrom(senderid, pathname, msg->buf, len);
			struct Stat* statbuf = _Stat(pathname, cur_inode);
			CopyTo(senderid, statbuf, msg->ptr, sizeof(struct Stat));
			msg->len = 0;
			Reply(msg, senderid);
		} else if (msg->type == SYNC) {
			_Sync();
			msg->data1 = 0;
			Reply(msg, senderid);
		} else if (msg->type == SHUTDOWN) {
			msg->data1 = 0;
			Reply(msg, senderid);
			_Shutdown();
		}
	}
}





