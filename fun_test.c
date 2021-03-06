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
	int i;
	char name[7];
	for (i = 0; i < 40; i++) {
		sprintf(name, "file%02d", i);
		printf("Creating name: %s\n", name);
		fd = Create(name);
		// Close(fd);
	}
	for (i = 0; i < 40; i++) {
		sprintf(name, "file%02d", i);
		printf("Opening name: %s\n", name);
		fd = Open(name);
	}

	Shutdown();
}