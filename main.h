#include <aim.h>
#include <libnbio.h>
#include <stdarg.h>

#define PROG "grim"

extern nbio_t gnb;

struct session_info {
	aim_session_t sess;
	char *authorizer;
	int port;

	char *screenname;
	char *password;

	int killme;
};

extern void dvprintf(char *, ...);

extern int init_faim(struct session_info *);

extern int read_config(struct session_info *);

extern int watch_stdin();
extern int init_window();
extern void end_window();
