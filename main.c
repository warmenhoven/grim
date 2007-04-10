#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "main.h"

nbio_t gnb;
struct session_info si;
char keepalive_user[256];

char *
mydir()
{
	struct stat sb;
	static int init = 0;
	static char path[1024];
	char env[1024];
	int i;

	if (init)
		return path;

	sprintf(env, "%sDIR", PROG);
	for (i = 0; i < strlen(PROG); i++)
		env[i] = toupper(env[i]);

	if (getenv(env)) {
		sprintf(path, "%s", getenv(env));
	} else {
		sprintf(path, "%s/.%s", getenv("HOME"), PROG);
	}

	/* make sure the directory exists and is a directory */
	if (stat(path, &sb))
		mkdir(path, 0700);
	else if (!S_ISDIR(sb.st_mode)) {
		unlink(path);
		mkdir(path, 0700);
	}

	init = 1;

	return path;
}

int
main(int argc, char **argv)
{
	time_t lastnop;
	si.killme = 0;

	memset(&si, 0, sizeof(si));
	memset(keepalive_user, 0, sizeof(keepalive_user));

	if (read_config())	/* this will also create a default one if it doesn't exist yet */
		return 1;
	if (nbio_init(&gnb, 15)) {
		fprintf(stderr, "Couldn't init IO\n");
		return 1;
	}
	if (init_window())	/* this will draw the initial window and add stdin to nbio */
		return 1;

	if (argc < 2 || strcmp(argv[1], "--no-server")) {
		if (init_server())	/* this will add things to gnb. yay for globals */
			return 1;
	}

	time(&lastnop);
	while (!si.killme) {
		if ((time(NULL) - lastnop) > 60) {
			keepalive();

			lastnop = time(NULL);
		}

		if (nbio_poll(&gnb, 60 * 1000) == -1)
			break;
	}

	end_window();

	return 0;
}
