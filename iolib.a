#include <comp421/filesystem.h>
#include <comp421/iolib.h>

struct file_information {
	int inum;
	int position;
}

file_information open_files[MAX_OPEN_FILES];
int unused_fd = MAX_OPEN_FILES;


int
main() {
	int i;
	for (i = 0; i < MAX_OPEN_FILES; i++) {
		open_files[i] -> inum = 0;
		open_files[i] -> position = 0;
	}
}

int
get_unused_fd(){
	int i;
	for (i = 0; i < MAX_OPEN_FILES; i++) {
		if (open_files[i] -> inum == 0) {
			return i; 
		}
	}
}

int 
Open(char *pathname) {
	if (unused_fd == 0) {
		return ERROR;
	}

	int fd;

	//if file is already in open_files array, return fd number
	//TODO: how do I get the file inode number??
	int i;
	for(i = 0; i < MAX_OPEN_FILES; i++) {
		if (open_files[i] -> inum != 0 && open_files[i] -> inum == <INSERT FILE INODE NUMBER>) {
			fd = i;
			return fd;
		}
	}

	fd = get_unused_fd();


	open_files[fd] -> inum = <INSERT FILE INODE NUMBER>;
	open_files[fd] -> position = 0;

	unused_fd--;
	return fd;

}


int
Close(int fd) {
	if (open_files[fd] -> inum == 0) {
		return ERROR;
	}

	open_files[fd] -> inum = 0;
	open_files[fd] -> position = 0;
	unused_fd++;
	return 0;
} 

int
Create(char *pathname) {
	return 0;
}

int
Read(int fd, void *buf, int size) {
	return 0;
}

int Write(int fd, void *buf, int size){
	return 0;
}

int
Seek(int fd, int offset, int whence){
	return 0;
}

int 
Link(char *oldname, char *newname){
	return 0;
}

int
Unlink(char *pathname){
	return 0;
}

int 
Symlink(char* oldname, char* newname){
	return 0;
}

int 
ReadLink(char *pathname, char *buf, int len){
	return 0;
}

int 
MkDir(char *pathname){
	return 0;
}

int 
RmDir(char *pathname){
	return 0;
}


int 
Stat(char *pathname, struct Stat *statbuf){
	return 0;
}

int
Sync(){
	return 0;
}

int
Shutdown(){
	return 0;
}




