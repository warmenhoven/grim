#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "main.h"

static int defaults()
{
	char path[8192];
	FILE *f;

	sprintf(path, "%s/.%s/config", getenv("HOME"), PROG);
	if (!(f = fopen(path, "w"))) {
		fprintf(stderr, "Can't write %s\n", path);
		return 1;
	}
	fprintf(f, "user EricWarmenhoven\n");
	fprintf(f, "pass password\n");
	fprintf(f, "auth login.oscar.aol.com\n");
	fprintf(f, "port 5190\n");
	fclose(f);

	fprintf(stderr, "Please modify your %s\n", path);
	return 1;
}

int read_config()
{
	struct stat sb;
	char path[1024];

	/* make sure the directory exists and is a directory */
	sprintf(path, "%s/.%s", getenv("HOME"), PROG);
	if (stat(path, &sb))
		mkdir(path, 0700);
	else if (!S_ISDIR(sb.st_mode)) {
		unlink(path);
		mkdir(path, 0700);
	}

	/* make sure the conf file exists and is a file */
	sprintf(path, "%s/.%s/config", getenv("HOME"), PROG);
	if (stat(path, &sb))
		return defaults();
	else if (!S_ISREG(sb.st_mode)) {
		unlink(path);
		return defaults();
	} else {
		FILE *f = fopen(path, "r");
		char line[8192];
		if (!f) {
			unlink(path);
			return defaults();
		}
		while (fgets(line, 8192, f)) {
			line[strlen(line)-1] = 0;
			if (!strncmp(line, "user ", 5)) {
				si.screenname = strdup(line + 5);
			} else if (!strncmp(line, "pass ", 5)) {
				si.password = strdup(line + 5);
			} else if (!strncmp(line, "auth ", 5)) {
				si.authorizer = strdup(line + 5);
			} else if (!strncmp(line, "port ", 5)) {
				si.port = atoi(line + 5);
			}
		}
		fclose(f);
	}

	return 0;
}
