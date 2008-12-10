#ifndef main_h
#define main_h

#define HAVE_SYS_SOCKET_H
#include <libnbio.h>
#include <stdarg.h>
#include "list.h"

#define PROG "grim"

#define KEEPALIVE "keep alive!"

extern nbio_t gnb;
extern struct session_info si;
extern char keepalive_user[256];

struct session_info {
	char *displayname;

	char *screenname;
	char *password;
	char *authorizer;
	int port;

	char *jid;
	char *jserver;
	int jport;
	char *key;
	char *resource;
	int priority;

	int killme;
};

extern void dvprintf(char *, ...);

extern char *mydir();

extern void add_group(char *, short);
extern void add_buddy(char *, short);
extern void buddy_state(char *, int);

extern int read_config();

extern int init_window();
extern void end_window();

extern void got_im(char *, char *, int);

extern int init_server();
extern void getinfo(char *);
extern void usersearch(char *);
extern void send_im(char *, char *);
extern void presence(char *, int);
extern void keepalive();
extern void away(char *);
extern void priority(int);

#endif
