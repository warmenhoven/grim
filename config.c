#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
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
	fprintf(f, "\n");
	fprintf(f, "jid eric@warmenhoven.org\n");
	fprintf(f, "jpt 5222\n");
	fprintf(f, "key password\n");
	fprintf(f, "res grim\n");
	fclose(f);

	fprintf(stderr, "Please modify your %s\n", path);
	return 1;
}

int read_config()
{
	struct stat sb;
	char path[1024];

	/* make sure the conf file exists and is a file */
	sprintf(path, "%s/config", mydir());
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
			if (!strncmp(line, "jid ", 4)) {
				char *x = strchr(line, '@');
				if (!x) {
					fprintf(stderr, "invalid jid\n");
					return 1;
				}
				*x++ = 0;
				si.jid = strdup(line + 4);
				si.jserver = strdup(x);
			} else if (!strncmp(line, "key ", 4)) {
				si.key = strdup(line + 4);
			} else if (!strncmp(line, "jpt ", 4)) {
				si.jport = atoi(line + 4);
			} else if (!strncmp(line, "res ", 4)) {
				si.resource = strdup(line + 4);
			} else if (!strncmp(line, "user ", 5)) {
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
