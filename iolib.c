#include <comp421/filesystem.h>
#include <comp421/iolib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <comp421/yalnix.h>

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

//here
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

//USED IN READLINK
struct my_msg3{
	int type;
	int len;
	int cur_inode;
	int data;
	void *buf;
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

struct file_information open_files[MAX_OPEN_FILES];
int unused_fd = MAX_OPEN_FILES;
int flag = 0;
int current_dir_inode = ROOTINODE;

void
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
	printf("No more unused fds\n");
	return -1;
}

int
Open(char *pathname) {

	struct my_msg2 *msg = malloc(sizeof(struct my_msg2));
	if (flag == 0) {
		initialize();
		flag = 1;
	}

	if (unused_fd == 0) {
		printf("You have already opened the maximum number of files\n" );
		return -1;
	}

	//Create new msg
	msg -> type = OPEN;
	msg -> data1 = strlen(pathname);
	msg -> data2 = current_dir_inode;
	//TODO: what do I fill in for data field?
	msg -> ptr = pathname;


	//Send message to kernel
	int send_message = Send(msg, -FILE_SERVER);
	if (send_message == -1) {
		printf("SEND MESSAGE = -1\n");
		return -1;
	}

	int inum = msg->data1;
	printf("Got inum: %d back from yfs\n", inum);

	if (inum == -1) {
		printf("File not found\n");
		return -1;
	}

	int fd;


	//if file is already in open_files array, return fd number
	//TODO: how do I get the file inode number??
	int i;
	for(i = 0; i < MAX_OPEN_FILES; i++) {
		if (open_files[i].inum != 0 && open_files[i].inum == inum) {
			fd = i;
			// open_files[i].inum = inum;
			return fd;
		}
	}


	fd = get_unused_fd();


	open_files[fd].inum = inum;
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

int
Create(char *pathname) {
	if (flag == 0) {
		initialize();
		flag = 1;
	}

	struct my_msg2 *msg = malloc(sizeof(struct my_msg2));
	msg -> type = CREATE;
	msg -> data1 = strlen(pathname);
	msg -> data2 = current_dir_inode;
	msg -> ptr = pathname;
	int send_message = Send(msg, -FILE_SERVER);
	// printf("RECEIVED REPLY\n");
	if (send_message == -1) {
		return -1;
	}

	int fd = get_unused_fd();

	int inum = msg->data1;

	if (inum == -1) {
		return -1;
	}

	open_files[fd].inum = inum;
	open_files[fd].position = 0;

	unused_fd--;
	return fd;
}

int
Read(int fd, void *buf, int size) {
	int inum = open_files[fd].inum;

	struct my_msg2 *msg = malloc(sizeof(struct my_msg2));
	msg->type = READ;
	msg->data1 = inum;
	msg->data2 = size;
	msg->data3 = open_files[fd].position;
	msg->ptr = buf;

	int send_message = Send(msg, -FILE_SERVER);
	// printf("RECEIVED REPLY\n");
	if (send_message == -1) {
		return -1;
	}

	int result = msg -> data1;

	if (result == -1) {
		return -1;
	}

	open_files[fd].position += result;
	return result;

}

int Write(int fd, void *buf, int size){
	int inum = open_files[fd].inum;

	struct my_msg2 *msg = malloc(sizeof(struct my_msg2));
	msg->type = WRITE;
	msg->data1 = inum;
	msg->data2 = size;
	msg->data3 = open_files[fd].position;
	msg->ptr = buf;

	int send_message = Send(msg, -FILE_SERVER);
	// printf("RECEIVED REPLY\n");
	if (send_message == -1) {
		return -1;
	}

	int result = msg -> data1;

	if (result == -1) {
		return -1;
	}

	open_files[fd].position += result;
	return result;
}

int
Seek(int fd, int offset, int whence){
	int position;

	if (whence == SEEK_SET) {
		position = offset;
	} else if (whence == SEEK_CUR) {
		position = open_files[fd].position + offset;
	} else if (whence == SEEK_END) {
		struct my_msg2 *msg = malloc(sizeof(struct my_msg2));
		msg->type = SEEK;
		msg->data1 = open_files[fd].inum;
		int send_message = Send(msg, -FILE_SERVER);
		if (send_message == -1) {
			return -1;
		}
		int size = msg -> data1;
		position = size + offset;
	}

	if (position < 0) {
		printf("Attempted to seek to before beginning of file!\n");
		return -1;
	}
	return position;
}

int
Link(char *oldname, char *newname){
	struct my_msg4 *msg = malloc(sizeof(struct my_msg4));

	if (flag == 0) {
		initialize();
		flag = 1;
	}

	msg -> type = LINK;
	msg -> cur_inode = current_dir_inode;
	msg -> oldname = oldname;
	msg -> len_oldname = strlen(oldname);
	msg -> newname = newname;
	msg -> len_newname = strlen(newname);

	//Send message to kernel
	int send_message = Send(msg, -FILE_SERVER);
	if (send_message == -1) {
		printf("SEND MESSAGE = -1\n");
		return -1;
	}
	return 0;
}

int
Unlink(char *pathname){
	struct my_msg2 *msg = malloc(sizeof(struct my_msg2));

	if (flag == 0) {
		initialize();
		flag = 1;
	}

	msg -> type = UNLINK;
	msg -> data1 = strlen(pathname);
	msg -> data2 = current_dir_inode;
	msg -> ptr = pathname;

	//Send message to kernel
	int send_message = Send(msg, -FILE_SERVER);
	if (send_message == -1) {
		printf("SEND MESSAGE = -1\n");
		return -1;
	}

	int result = msg->data1;

	return result;
}

int
SymLink(char* oldname, char* newname){
	struct my_msg4 *msg = malloc(sizeof(struct my_msg4));

	if (flag == 0) {
		initialize();
		flag = 1;
	}

	msg -> type = LINK;
	msg -> cur_inode = current_dir_inode;
	msg -> oldname = oldname;
	msg -> len_oldname = strlen(oldname);
	msg -> newname = newname;
	msg -> len_newname = strlen(newname);

	//Send message to kernel
	int send_message = Send(msg, -FILE_SERVER);
	if (send_message == -1) {
		printf("SEND MESSAGE = -1\n");
		return -1;
	}
	return 0;
}

int
ReadLink(char *pathname, char *buf, int len){
	struct my_msg3 *msg = malloc(sizeof(struct my_msg3));

	if (flag == 0) {
		initialize();
		flag = 1;
	}


	msg->type = READLINK;
	msg->len = len;
	msg->cur_inode = current_dir_inode;
	msg->data = strlen(pathname);
	msg->buf = buf;
	msg->ptr = pathname;

	//Send message to kernel
	int send_message = Send(msg, -FILE_SERVER);
	if (send_message == -1) {
		printf("SEND MESSAGE = -1\n");
		return -1;
	}
	return msg->len;
}

int
MkDir(char *pathname) {
	struct my_msg2 *msg = malloc(sizeof(struct my_msg2));

	if (flag == 0) {
		initialize();
		flag = 1;
	}

	msg -> type = MKDIR;
	msg -> data1 = strlen(pathname);
	msg -> data2 = current_dir_inode;
	msg -> ptr = pathname;

	//Send message to kernel
	int send_message = Send(msg, -FILE_SERVER);
	if (send_message == -1) {
		printf("SEND MESSAGE = -1\n");
		return -1;
	}
	return 0;
}

int
RmDir(char *pathname){
	struct my_msg2 *msg = malloc(sizeof(struct my_msg2));

	if (flag == 0) {
		initialize();
		flag = 1;
	}

	msg -> type = RMDIR;
	msg -> data1 = strlen(pathname);
	msg -> data2 = current_dir_inode;
	msg -> ptr = pathname;

	//Send message to kernel
	int send_message = Send(msg, -FILE_SERVER);
	if (send_message == -1) {
		printf("SEND MESSAGE = -1\n");
		return -1;
	}
	return 0;
}

int
ChDir(char *pathname){
	struct my_msg2 *msg = malloc(sizeof(struct my_msg2));

	if (flag == 0) {
		initialize();
		flag = 1;
	}
	msg -> type = CHDIR;
	msg -> data1 = strlen(pathname);
	msg -> data2 = current_dir_inode;
	msg -> ptr = pathname;

	//Send message to kernel
	int send_message = Send(msg, -FILE_SERVER);
	if (send_message == -1) {
		printf("SEND MESSAGE = -1\n");
		return -1;
	}

	int inum = msg->data1;
	current_dir_inode = inum;
	return 0;

}


int
Stat(char *pathname, struct Stat *statbuf){

	struct my_msg3 *msg = malloc(sizeof(struct my_msg3));
	msg->type = STAT;
	msg->len = strlen(pathname);
	// printf("pathname in iolib: %s, len: %d\n", pathname, msg->len);
	msg->cur_inode = current_dir_inode;
	msg->ptr = statbuf;
	msg->buf = pathname;
	int send_message = Send(msg, -FILE_SERVER);
	if (send_message == -1) {
		printf("SEND MESSAGE = -1\n");
		return -1;
	}
	int result = msg->len;
	return result;
}

int
Sync(){
	struct my_msg1 *msg = malloc(sizeof(struct my_msg1));
	msg -> type = SYNC;

	int send_message = Send(msg, -FILE_SERVER);
	if (send_message == -1) {
		printf("SEND MESSAGE = -1\n");
		return -1;
	}
	return 0;
}

int
Shutdown(){
	struct my_msg1 *msg = malloc(sizeof(struct my_msg1));
	msg -> type = SHUTDOWN;

	int send_message = Send(msg, -FILE_SERVER);
	if (send_message == -1) {
		return -1;
	}

	return 0;
}

