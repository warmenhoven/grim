#include <stdio.h>
#if !(__GLIBC__ < 2)
#include <error.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define BUF_SIZE 10

int main(int argc, char *argv[])
{
	int fd;
	FILE *f;
	int res,x;
	char buf[BUF_SIZE];
	char file[256];
	int offset = 0, cnt;

	if (argc != 3) {
		fprintf(stderr, "Too few arguments! Are you using gmake? If you're not, then you really should. :-) \n");
		exit(1);
	}

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror("open");
		exit(1);
	}
	f = fopen(argv[2], "w+");
	if (!f) {
		perror("fopen");
		exit(1);
	}
	argv[1][strlen(argv[1])-3]='\0';
	cnt = strlen(argv[1]);
	while (offset != cnt) {
		if (argv[1][cnt - offset] == '/') {
			offset--;
			break;
		}
		offset++;
	}
	sprintf(file, "%s", &argv[1][cnt - offset]);
	fprintf(f, "static unsigned char %s[] = {\n", file);
	read(fd, buf, 8); /* id & offset */
	read(fd, buf, 8); /* len & encoding */
	read(fd, buf, 8); /* rate & count */
	/*  no more click :) */

	while((res = read(fd, buf, BUF_SIZE)) > 0) {
		for (x=0;x<res;x++)
			fprintf(f, "%#x, ", buf[x] & 0xff);
		fprintf(f, "\n");
	}
	fprintf(f,"};\n");
	fclose(f);
	return 0;
}
