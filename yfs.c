#include <comp421/filesystem.h>
#include <comp421/iolib.h>
#include <comp421/yalnix.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <libgen.h>


#define OPEN 0
#define CLOSE 1
#define CREATE 2

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


void *get_cached_elem(int num, int block);
void insert_elem_in_cache(int num, void *data, int block);
int remove_lru_block();
int remove_lru_inode();
char *get_block(int num);
struct inode *get_inode(int num);
int main();
int read_with_offset(int sectornum, void *buf, int offset, int size);
int get_inode_num_from_path(char *pathname, int dir_inode);
void copy_data_from_inode(void *buf, int inodenum, int offset, int size);
char *get_dir_entries(int inum);
int get_inode_in_dir(char *name, int dir_inode_num);
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

int
get_free_inode_num() {
	int i = 1;
	while(i <= num_inodes) {
		struct inode *node = get_inode(i);
		if (node->type == INODE_FREE)
			return i;
		i ++;
	}
}


// block = 1 -> use block hashtable, otherwise use inode
void *
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
				return entry->data;
			} else {
				elem = elem->next;
			}
		}
		return NULL;
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
	return lru_num;
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
	return lru_num;
}


char *
get_block(int num) {
	char *block = get_cached_elem(num, 1);
	if (block == NULL) {
		// block is not in cache
		// printf("Blocknum %d not in cache\n", num);

		block = malloc(SECTORSIZE); // Allocate space for the block
		ReadSector(num, block); // Read the block into the allocated space

		// Remove LRU block
		if (num_blocks_cached >= BLOCK_CACHESIZE)
			remove_lru_block();

		// Cache the new block
		insert_elem_in_cache(num, block, 1);
	}
	return block;
}


struct inode*
get_inode(int num) {
	struct inode *node = get_cached_elem(num, 0);
	if (node == NULL){
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
		if (get_cached_elem(num, 0) == NULL)
			printf("Cached elem is null just after caching\n");
	}
	return node;
}



int
read_with_offset(int sectornum, void *buf, int offset, int size) {
	char *temp = get_block(sectornum);
	memcpy(buf, temp + offset, size);
}


int
get_inode_num_from_path(char *pathname, int dir_inode_num) {
	if (strlen(pathname) == 0) {
		printf("%s\n", "Pathname has length 0");
		return ERROR;
	}

	char* token = strtok(pathname, "/");
	// printf("token: %s\n", token);
	struct inode *dirnode = get_inode(dir_inode_num);
	// printf("dir_inode_num: %d\tdirnode->type: %d\n", dir_inode_num, dirnode->type);
	int inum = dir_inode_num;

	while (token != NULL && dirnode->type == INODE_DIRECTORY) {
		printf("Token: %s\n", token);
		inum = get_inode_in_dir(token, dir_inode_num);
		if (inum == -1) {
			printf("File not found\n");
			return -1;
		}
		dir_inode_num = inum;
		dirnode = get_inode(dir_inode_num);
        // printf("Token: %s\n", token);
        token = strtok(NULL, "/");
    }

    if (token != NULL) {
    	printf("Not done parsing path but found non-directory\n");
    	return -1;
    } else {
    	return inum;
    }
}


void
copy_data_from_inode(void *buf, int inodenum, int offset, int size) {
	// printf("COPYING DATA FROM INODE num: %d\toffset: %d\tsize: %d\n", inodenum, offset, size);
	struct inode *node = get_inode(inodenum);
	int size_copied = 0;

	//FIND FIRST BLOCK TO USE and OFFSET
	int num_direct_block = offset / SECTORSIZE;
	offset = offset % SECTORSIZE;
	char *block = get_block(node->direct[num_direct_block]);

	// COPY FROM FIRST BLOCK
	if (offset + size <= SECTORSIZE) {
		// just copy part of the first block starting at offset
		memcpy(buf, block + offset, size);
		return;

	} else {
		// copy whole first block starting at offset
		memcpy(buf, block + offset, SECTORSIZE - offset);
		size_copied = SECTORSIZE - offset;
		num_direct_block ++;
	}

	// COPY FROM FULL BLOCKS
	while(size - size_copied >= SECTORSIZE && num_direct_block < NUM_DIRECT) {
		// copy the whole block
		block = get_block(node->direct[num_direct_block]);
		memcpy(buf + size_copied, block, SECTORSIZE);
		size_copied = SECTORSIZE - offset;
		num_direct_block ++;
	}

	// COPY FROM LAST BLOCK / INDIRECT
	if (num_direct_block != NUM_DIRECT) {
		// COPY FROM LAST BLOCK
		block = get_block(node->direct[num_direct_block]);
		memcpy(buf + size_copied, block, size - size_copied);
		return;



	} else {
		// COPY FROM INDIRECT
		printf("%s\n", "ON INDIRECT BLOCKS");
		// TODO - DO THIS
	}

	// check if on indirect or on last block
}

///////MUCH WORK TO BE DONE! WE NEED TO ALLOCATE BLOCKS, ETC
void
write_data_to_inode(void *buf, int inodenum, int offset, int size) {
	printf("WRITING DATA TO INODE num: %d\toffset: %d\tsize: %d\n", inodenum, offset, size);
	set_dirty(inodenum, 0);
	struct inode *node = get_inode(inodenum);
	if (offset + size > node->size)
		node->size = offset + size;
	int size_written = 0;

	//FIND FIRST BLOCK TO USE and OFFSET
	int num_direct_block = offset / SECTORSIZE;
	offset = offset % SECTORSIZE;
	int blocknum = node->direct[num_direct_block];
	if (blocknum == 0) {
		blocknum = get_free_block();
		node->direct[num_direct_block] = blocknum;
	}
	char *block = get_block(blocknum);
	// COPY FROM FIRST BLOCK
	if (offset + size <= SECTORSIZE) {
		printf("here1\n");
		// just copy part of the first block starting at offset
		memcpy(block + offset, buf, size);
		set_dirty(blocknum, 1);
		return;

	} else {
		// copy whole first block starting at offset
		memcpy(block + offset, buf, SECTORSIZE - offset);
		set_dirty(blocknum, 1);
		size_written = SECTORSIZE - offset;
		num_direct_block ++;
	}

	// COPY FROM FULL BLOCKS
	while(size - size_written >= SECTORSIZE && num_direct_block < NUM_DIRECT) {
		// copy the whole block
		int blocknum = node->direct[num_direct_block];
		if (blocknum == 0) {
			blocknum = get_free_block();
			node->direct[num_direct_block] = blocknum;
		}
		block = get_block(blocknum);
		memcpy(block, buf + size_written, SECTORSIZE);
		set_dirty(blocknum, 0);
		size_written = SECTORSIZE - offset;
		num_direct_block ++;
	}

	// COPY FROM LAST BLOCK / INDIRECT
	if (num_direct_block != NUM_DIRECT) {
		// COPY FROM LAST BLOCK
		int blocknum = node->direct[num_direct_block];
		if (blocknum == 0) {
			blocknum = get_free_block();
			node->direct[num_direct_block] = blocknum;
		}
		block = get_block(blocknum);
		memcpy(block, buf + size_written, size - size_written);
		set_dirty(blocknum, 0);
		return;

	} else {
		// COPY FROM INDIRECT
		printf("%s\n", "ON INDIRECT BLOCKS");
		// TODO - DO THIS
	}

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

int
get_inode_in_dir(char *name, int dir_inode_num) {
	printf("GETTING INODE IN DIR\n");
	struct inode *dirnode = get_inode(dir_inode_num);

	char *dir_entries = get_dir_entries(dir_inode_num);
	int namelength = strlen(name);
	int offset = 0;
	while (offset < dirnode->size) {
		struct dir_entry *entry = (struct dir_entry*)&dir_entries[offset];
		int inum = entry->inum;

		if (strncmp(name, entry->name, namelength) == 0 && inum != 0) {
			return inum;
		}
		offset += sizeof(struct dir_entry);
	}
	printf("No file with name: '%s' was found.\n", name);
	return ERROR; // NO FILE FOUND
}



int
_Open(char *pathname, int current_inode) {
	printf("Opening in yfs\n");
	if (pathname[0] == '/') {
         current_inode = ROOTINODE;
    }

    printf("Current inode: %d\tPathname: %s\n", current_inode, pathname);
    int inum = get_inode_num_from_path(pathname, current_inode);
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

    char *dirnamestr = malloc(strlen(pathname));
    char *filename = malloc(strlen(pathname));
    memcpy(dirnamestr, pathname, strlen(pathname));
    memcpy(filename, pathname, strlen(pathname));

	filename = basename(filename);
	dirnamestr = dirname(dirnamestr);
	if (strlen(filename) > DIRNAMELEN){
		printf("Filename too long!");
		return -1;
	}
	printf("Creating file: %s in directory %s from pathname %s\n", filename, dirnamestr, pathname);

	int directory_inum = get_inode_num_from_path(dirnamestr, current_inode);
	printf("Directory inum: %d\n", directory_inum);
	char *dir_entries = get_dir_entries(directory_inum);


	int current_inode_num = get_inode_in_dir(filename, directory_inum);
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
			return;
		}
		printf("Done receiving from pid: %d, message type: %d\n", senderid, msg->type);
		if (msg->type == OPEN) {
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

		} else if (msg->type == CREATE) {
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
			Reply(msg ,senderid);
		}
	}

}





