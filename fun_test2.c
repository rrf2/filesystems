#include <stdio.h>
#include <comp421/yalnix.h>
#include <comp421/iolib.h>

/*
 * Create empty files named "file00" through "file31" in "/".
 */
int
main()
{
	printf("Making directory: %s\n", "dir");
	MkDir("/dir1");
	printf("Making directory: %s \n", "dir2");
	MkDir("/dir1/dir2");

	printf("Creating SymLink from dir2 to dir1...\n");
	SymLink("/dir1", "a")
	SymLink("/dir1/dir2", "b")

	Open("/dir1/a");
	Open("/dir1/a/b/a/b/a/b/a/b/a/b/a/b/a/b/a/b/a/b/a/b/a/b/a/b/a/b/a/b/a/b/a/b/a/b/a/b/a/b/a/b");

	Shutdown();
}