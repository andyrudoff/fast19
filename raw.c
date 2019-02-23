#include <sys/mman.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	int fd;
	char *pmaddr;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s filename\n", argv[0]);
		exit(1);
	}

	if ((fd = open(argv[1], O_RDWR)) < 0)
		err(1, "open: %s", argv[2]);

	if ((pmaddr = (char *)mmap(NULL, 4096, PROT_READ|PROT_WRITE,
					MAP_SHARED, fd, 0)) == MAP_FAILED)
		err(1, "mmap: %s", argv[2]);

	close(fd);

	/* write to persistent memory... */
	strcpy(pmaddr, "Hello, persistent memory!");

	/* flush the changes... */
	if (msync((void *)pmaddr, 4096, MS_SYNC) < 0)
		err(1, "msync: %s", argv[2]);

	printf("done.\n");
	exit(0);
}
