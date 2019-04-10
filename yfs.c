#include <comp421/filesystem.h>
#include <comp421/iolib.h>

struct fs_header header;
int num_blocks;
int num_inodes;



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
	void *node;
	struct list_elem next;
};

struct list_elem dummy;

#define INODE_HASHTABLE_SIZE INODE_CACHESIZE * 2
#define BLOCK_HASHTABLE_SIZE BLOCK_CACHESIZE * 2

struct list_elem inode_hashtable[INODE_HASHTABLE_SIZE];
struct list_elem block_hashtable[BLOCK_HASHTABLE_SIZE];

int num_inodes_cached = 0;
int num_blocks_cached = 0;


// struct inode*
// get_cached_inode(int inode_num) {
// 	struct list_elem *elem = inode_hashtable[inode_num % INODE_HASHTABLE_SIZE];
// 	if (elem == NULL) {
// 		return NULL;
// 	} else {
// 		while (elem != NULL) {
// 			struct inode cache_entry = (struct inode)elem->node
// 			if (cache_entry->num == inode_num) {
// 				return cache_entry->node;
// 			} else {
// 				elem = elem->next;
// 			}
// 		}
// 		return NULL;
// 	}
// }

// void *
// get_cached_block(int block_num) {
// 	struct list_elem *elem = block_hashtable[block_num % BLOCK_HASHTABLE_SIZE];
// 	if (elem == NULL) {
// 		return NULL;
// 	} else {
// 		while (elem != NULL) {
// 			struct block_cache_entry = (struct block)elem->node
// 			if (cache_entry->num == block_num) {
// 				return cache_entry->node;
// 			} else {
// 				elem = elem->next;
// 			}
// 		}
// 		return NULL;
// 	}
// }


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
			struct cache_entry = elem->node
			if (cache_entry->num == num) {
				return cache_entry->data;
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

	struct list_elem newelem = malloc(sizeof(struct list_elem));
	newelem->node = entry;
	newelem->next = NULL;

	struct list_elem *elem;
	if (block == 1)
		elem = block_hashtable[num % BLOCK_HASHTABLE_SIZE];
		entry->num_used = num_blocks_cached;
		num_blocks_cached ++;
	else
		elem = inode_hashtable[num % INODE_HASHTABLE_SIZE];
		entry->num_used = num_inodes_cached;
		num_inodes_cached ++;

	if (elem == NULL) {
		// No collision
		block_hashtable[num % BLOCK_HASHTABLE_SIZE] = newelem
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
	for (i=0; i < BLOCK_HASHTABLE_SIZE; i++) {
		elem = block_hashtable[i];
		while (elem != NULL) {
			if (elem->node->num < min) {
				lru_num = elem->node->num;
				min = elemn->node->num_used;
			}
			elem = elem->next;
		}
	}
	return lru_num;
}


int
remove_lru_inode() {
	int min = num_inodess_cached + 1;
	int lru_num;
	int i;
	for (i=0; i < INODE_HASHTABLE_SIZE; i++) {
		elem = inode_hashtable[i];
		while (elem != NULL) {
			if (elem->node->num < min) {
				lru_num = elem->node->num;
				min = elemn->node->num_used;
			}
			elem = elem->next;
		}
	}
	return lru_num;
}


char *
get_block(int num) {
	char *block = get_cached_block(num, 1);
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
		read_with_offset(blocknum, node, offset, INODESIZE);

		// Remove LRU block
		if (num_inode_cached >= INODE_CACHESIZE)
			remove_lru_inode();

		// Cache the new block
		insert_elem_in_cache(num, node, 0);
	}
	return node;
}





int
main() {
	// Read FS HEADER
	read_with_offset(1, *header, 0, INODESIZE);
	num_inodes = header.num_inodes;
	num_blocks = header.num_blocks;

	// // Read in inodes
	// inodes = malloc(num_inodes * INODESIZE);

	// int size_of_inodes = num_inodes * INODESIZE;


	// if (size_of_inodes <= SECTORSIZE - INODESIZE) {
	// 	// All inodes are in block 1
	// 	read_with_offset(1, inodes, INODESIZE, size_of_inodes);
	// } else {
	// 	// inodes go past block 1
	// 	read_with_offset(1, inodes, INODESIZE, SECTORSIZE - INODESIZE);

	// 	int size_used = SECTORSIZE - INODESIZE;
	// 	int block = 2;
	// 	while (size_of_inodes - size_used > SECTORSIZE) {
	// 		// Not on last block with inodes - whole block is inodes
	// 		read_with_offset(block, inodes + size_used, 0, SECTORSIZE);
	// 		size_used += SECTORSIZE;
	// 		block ++;
	// 	}
	// 	// On last block with inodes
	// 	read_with_offset(block, inodes + size_used, 0, )
	// }

}


int
read_with_offset(int sectornum, void *buf, int offset, int size) {
	char *temp = malloc(BLOCKSIZE);
	ReadSector(sectornum, temp);
	memcpy(buf, temp + offset, size);
	free(temp);
}