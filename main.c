#include <stdio.h>
#include <signal.h>
#include <time.h>
#include "main.h"

nbio_t gnb;

static void cleanup(int sig)
{
	end_window();
	exit(0);
}

int main()
{
	time_t lastnop;
	struct session_info si;
	si.killme = 0;

	signal(SIGINT, cleanup);

	if (read_config(&si))	/* this will also create a default one if it doesn't exist yet */
		return 1;
	if (nbio_init(&gnb, 15)) {
		fprintf(stderr, "Couldn't init IO\n");
		return 1;
	}
	if (init_window())	/* this will draw the initial window and add stdin to nbio */
		return 1;

	if (init_faim(&si))	/* this will add things to gnb. yay for globals */
		return 1;

	time(&lastnop);
	while (1) {
		if (si.killme) {
			aim_session_kill(&si.sess);
			nbio_kill(&gnb);
			if (nbio_init(&gnb, 15))
				return 1;
			if (watch_stdin())
				return 1;
			if (init_faim(&si))
				return 1;
			si.killme = 0;
		}

		if ((time(NULL) - lastnop) > 60) {
			aim_conn_t *cur;

			for (cur = si.sess.connlist; cur; cur = cur->next)
				aim_flap_nop(&si.sess, cur);

			lastnop = time(NULL);
		}

		if (nbio_poll(&gnb, 60 * 1000) == -1)
			break;
	}

	cleanup(0);

	return 0;
}
