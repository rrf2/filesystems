#include <comp421/filesystem.h>
#include <comp421/iolib.h>

#define OPEN 0
#define CLOSE 1
#define CREATE 2

struct file_information {
	int inum;
	int position;
};

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

struct file_information open_files[MAX_OPEN_FILES];
int unused_fd = MAX_OPEN_FILES;
int flag = 0;
int inode = ROOTINODE;

int
initialize() {
	int i;
	for (i = 0; i < MAX_OPEN_FILES; i++) {
		open_files[i].inum = 0;
		open_files[i].position = 0;
	}
}

int
get_unused_fd(){
	int i;
	for (i = 0; i < MAX_OPEN_FILES; i++) {
		if (open_files[i].inum == 0) {
			return i; 
		}
	}
}

int 
Open(char *pathname) {

	struct my_msg2 *msg;
	if (flag = 0) {
		initialize();
		flag = 1;
	}

	if (unused_fd == 0) {
		return -1;
	}

	//Create new msg
	msg -> type = 0;
	msg -> data1 = strlen(pathname);
	//TODO: what do I fill in for data field?
	msg -> ptr = &pathname;

	//Send message to kernel
	int send_message = Send(&msg, -FILE_SERVER);
	if (send_message == -1) {
		return -1;
	}

	int fd;

	//if file is already in open_files array, return fd number
	//TODO: how do I get the file inode number??
	int i;
	for(i = 0; i < MAX_OPEN_FILES; i++) {
		if (open_files[i].inum != 0 && open_files[i].inum == msg -> data1) {
			fd = i;
			return fd;
		}
	}

	fd = get_unused_fd();

	open_files[fd].inum = msg -> data1;
	open_files[fd].position = 0;

	unused_fd--;
	return fd;

}


int
Close(int fd) {
	if (open_files[fd].inum == 0) {
		return -1;
	}

	open_files[fd].inum = 0;
	open_files[fd].position = 0;
	unused_fd++;
	return 0;
} 

// int
// Create(char *pathname) {
// 	if (flag == 0) {
// 		init();
// 		flag = 1;
// 	}

// 	msg -> type = 2;
// 	msg -> data1 = strlen(pathname);
// 	//TODO: what do I fill in for data field?
// 	msg -> ptr = pathname;

// 	int send_message = Send(&msg, -FILE_SERVER);
// 	if (send_message == -1) {
// 		return -1;
// 	}

// 	fd = get_unused_fd();

// 	open_files[fd] -> inum = msg -> data1;
// 	open_files[fd] -> position = 0;

// 	unused_fd--;
// 	return fd;
// }

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
ChDir(char *pathname){
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
	struct my_msg1 *msg;
	msg -> type = 16;

	int send_message = Send(&msg, -FILE_SERVER);
	if (send_message == -1) {
		return -1;
	}

	return 0;
}