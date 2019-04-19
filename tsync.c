#include <stdio.h>
#include <comp421/yalnix.h>
#include <comp421/iolib.h>

/*
 * Create empty files named "file00" through "file31" in "/".
 */
int
main()
{
	int fd;
	fd = Open("/file");
	int s = Seek(fd, 0, SEEK_SET);
	printf("%d\n", s);
	char *read = malloc(11);
	read[10] = '\0';
	Read(fd, read, 10);
	printf("%s\n", read);
	s = Seek(fd, 50, SEEK_CUR);
	printf("%d\n", s);
	Read(fd, read, 10);
	printf("%s\n", read);
	s = Seek(fd, -60, SEEK_END);
	printf("%d\n", s);
	Read(fd, read, 10);
	printf("%s\n", read);


	Shutdown();
}