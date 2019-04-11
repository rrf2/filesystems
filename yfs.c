#include <comp421/filesystem.h>
#include <comp421/iolib.h>
#include <comp421/yalnix.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>


#define OPEN 0
#define CLOSE 1
#define CREATE 2

struct fs_header header;
int num_blocks;
int num_inodes;

struct my_msg1{
    int type;
    int data1;
    char data2[16];
    void *ptr
}

struct my_msg2{
    int type;
    int data1;
    int data2;
    int data3;
    int data4;
    int data5;
    void *ptr
}

// struct inode_cache_entry {
// 	int num;
// 	struct inode *node;
// 	int dirty;
// 	int num_used;
// }

// struct block_cache_entry {
// 	int num;
// 	char *block;
// 	int dirty;
// 	int num_used;
// };

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
int main();
int read_with_offset(int sectornum, void *buf, int offset, int size);
int get_inode_num_from_path(char *pathname, int dir_inode);
void copy_data_from_inode(void *buf, int inodenum, int offset, int size);
char *get_dir_entries(int inum);
int get_inode_in_dir(char *name, int dir_inode_num);
int ReadSector(int sectornum, void *buf);


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
		block_hashtable[num % BLOCK_HASHTABLE_SIZE] = newelem;
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
		// inode is not in cache
		int blocknum = (num + 1) * INODESIZE / SECTORSIZE;
		int offset = ((num + 1) * INODESIZE) % SECTORSIZE;
		node = malloc(INODESIZE);
		read_with_offset(blocknum, node, offset, INODESIZE);

		// Remove LRU block
		if (num_inodes_cached >= INODE_CACHESIZE)
			remove_lru_inode();

		// Cache the new block
		insert_elem_in_cache(num, node, 0);
	}
	return node;
}



int
main() {
	// Read FS HEADER
	read_with_offset(1, &header, 0, INODESIZE);
	num_inodes = header.num_inodes;
	num_blocks = header.num_blocks;

	struct my_msg1 *msg = malloc(sizeof(my_msg));

	int senderid = Receive(msg);

	if (msg->type == OPEN) {
		struct my_msg2 = (my_msg2)msg;
		char *pathname = malloc(msg->data2);
		int len = msg->data2;
		int dir_inode_num = msg->data1;
		CopyFrom(senderid, pathname, msg->ptr, len);
		_Open(msg->ptr, dir_inode_num);
	}

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
	struct inode *dirnode = get_inode(dir_inode_num);
	int inum;

	while (token != NULL && dirnode->type == INODE_DIRECTORY) {
		inum = get_inode_in_dir(token, dir_inode_num);
		dir_inode_num = inum;
		dirnode = get_inode(dir_inode_num);
        printf("Token: %s\n", token);
        token = strtok(NULL, "/");
    }

    if (token != NULL) {
    	printf("Not done parsing path but found non-directory\n");
    } else {
    	return inum;
    }
}


void
copy_data_from_inode(void *buf, int inodenum, int offset, int size) {
	struct inode *node = get_inode(inodenum);
	int size_copied = 0;

	//FIND FIRST BLOCK TO USE and OFFSET
	int num_direct_block = offset / SECTORSIZE;
	offset = offset % SECTORSIZE;
	char *block = get_block(node->direct[num_direct_block]);

	// COPY FROM FIRST BLOCK
	if (offset * size <= SECTORSIZE) {
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

int
get_inode_in_dir(char *name, int dir_inode_num) {
	struct inode *dirnode = get_inode(dir_inode_num);
	char *dir_entries = get_dir_entries(dirnode);
	int namelength = strlen(name);
	int offset = 0;
	while (offset < dirnode->size) {
		int inum = (int)dir_entries[offset];
		if (strncmp(name, dir_entries[offset + sizeof(int)], namelength) == 0 && inum != 0) {
			return inum;
		}
		offset += sizeof(struct dir_entry);
	}
	printf("No file with name: '%s' was found.\n", name);
	return ERROR; // NO FILE FOUND
}



int
_Open(char *pathname, int current_inode) {
	if (pathname[0] == '/') {
         current_inode = ROOTINODE;
    }

    int inum = get_inode_num_from_path(pathname, current_inode);

    return inum;
}


int _Create(char *pathname, int current_inode, int new_inode) {
	if (current_inode == 0) {
		return ERROR;
	}

	char *filename;
	int directory_inum;
	//TODO: Get directory inode number
	//TODO: Get directory inode info


	int new_inum = //TODO:Search for the new filename if it already exists

	int i;
	for (i = 0; i<DIRNAMELEN; i++) {
        dir_entry->name[i] = '\0';
    }
    for (i = 0; filename[i] != '\0'; i++) {
        dir_entry->name[i] = filename[i];
    }

    int block = get_block() //TODO: get block number?
    inodeNum = //TODO: Get next free inode

    struct dir_entry *dir_entry;

    dir_entry -> inum = inodeNum;
    struct inode *inode = get_inode(inodeNum);
    inode->type = INODE_REGULAR;
    inode->size = 0;
    inode->nlink = 1;

    //TODO: add stuff to cache probably?
    return inodeNum;
}





