#include <comp421/yalnix.h>
#include <comp421/iolib.h>

int
main()
{
	int fd;

	fd = Create("a");
	printf("\n");
	Write(fd, "aaaaaaaaaaaaaaaa", 16);
	printf("\n");
	Close(fd);
	printf("\n");

	fd = Create("b");
	printf("\n");
	Write(fd, "bbbbbbbbbbbbbbbb", 16);
	printf("\n");
	Close(fd);
	printf("\n");

	fd = Create("c");
	printf("\n");
	Write(fd, "cccccccccccccccc", 16);
	printf("\n");
	Close(fd);
	printf("\n");

	MkDir("dir");
	printf("\n");

	fd = Create("/dir/x");
	printf("\n");
	Write(fd, "xxxxxxxxxxxxxxxx", 16);
	printf("\n");
	Close(fd);
	printf("\n");

	fd = Create("/dir/y");
	printf("\n");
	Write(fd, "yyyyyyyyyyyyyyyy", 16);
	printf("\n");
	Close(fd);
	printf("\n");

	fd = Create("/dir/z");
	printf("\n");
	Write(fd, "zzzzzzzzzzzzzzzz", 16);
	printf("\n");
	Close(fd);
	printf("\n");

	Shutdown();
}
