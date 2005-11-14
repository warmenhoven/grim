#ifndef main_h
#define main_h

#ifdef JABBER
#include <expat.h>
#else
#include <aim.h>
#endif
#define HAVE_SYS_SOCKET_H
#include <libnbio.h>
#include <stdarg.h>
#include "list.h"

#define PROG "grim"

extern nbio_t gnb;
extern struct session_info si;

#ifdef JABBER
typedef struct _jabber_session {
	nbio_fd_t *fdt;
	XML_Parser parser;
	char *streamid;
	int id;
	void *curr;
	list *iqs;
} jabber_session_t;
#endif

struct session_info {
#ifdef JABBER
	jabber_session_t sess;
#else
	aim_session_t sess;
#endif
	char *authorizer;
	int port;

	char *screenname;
	char *password;
	char *resource;

	int killme;
};

extern void dvprintf(char *, ...);

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

#ifdef SOUND
extern void play();
#else
#define play()
#endif

#endif
