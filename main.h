#ifndef main_h
#define main_h

#include <aim.h>
#include <libnbio.h>
#include <stdarg.h>

#define PROG "grim"

extern nbio_t gnb;
extern struct session_info si;

struct session_info {
	aim_session_t sess;
	char *authorizer;
	int port;

	char *screenname;
	char *password;

	int killme;
};

extern void dvprintf(char *, ...);

extern int init_faim();
extern void add_group(char *, short);
extern void add_buddy(char *, short);
extern void buddy_state(char *, int);

extern int read_config();

extern int init_window();
extern void end_window();

extern void got_im(char *, char *, int);

extern void play();

#endif
