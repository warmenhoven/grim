#include <stdio.h>
#include <string.h>
#include <time.h>
#include "main.h"

nbio_t gnb;
struct session_info si;

int main()
{
	time_t lastnop;
	si.killme = 0;

	memset(&si, 0, sizeof(si));

	if (read_config())	/* this will also create a default one if it doesn't exist yet */
		return 1;
	if (nbio_init(&gnb, 15)) {
		fprintf(stderr, "Couldn't init IO\n");
		return 1;
	}
	if (init_window())	/* this will draw the initial window and add stdin to nbio */
		return 1;

	if (init_server())	/* this will add things to gnb. yay for globals */
		return 1;

	time(&lastnop);
	while (1) {
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
