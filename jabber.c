#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "xml.h"

char *c = "<iq id='j6' type='set'><query xmlns='jabber:iq:auth'><username>eric</username><resource>work</resource><hash>fed48b727f0f21f3eb3c53a7e0247bb3028a6794</hash></query></iq>";
char *d = "<iq type='get'><query xmlns='jabber:iq:roster'/></iq>";
char *e = "<presence><status>Online</status><priority>9</priority></presence>";

static void jabber_send(char *stream)
{
	char *buf = strdup(stream);

	dvprintf("sent %d: %s", strlen(buf), buf);

	if (nbio_addtxvector(&gnb, si.sess.fdt, buf, strlen(buf)) == -1) {
		dvprintf("nbio_addtxvector: %s", strerror(errno));
		free(buf);
	}
}

static void jabber_process()
{
}

static void jabber_start(void *data, const char *el, const char **attr)
{
	int i;

	if (!strcmp(el, "stream:stream")) {
		char iq[1024];
		snprintf(iq, sizeof(iq), "<iq id='j%d' type='get'><query xmlns='jabber:iq:auth'><username>%s</username></query></iq>", si.sess.id++, si.screenname);
		jabber_send(iq);
		return;
	}

	if (si.sess.curr)
		si.sess.curr = xml_child(si.sess.curr, el);
	else
		si.sess.curr = xml_new(el);

	for (i = 0; attr[i]; i += 2)
		xml_attrib(si.sess.curr, attr[i], attr[i + 1]);
}

static void jabber_end(void *data, const char *el)
{
	void *parent;

	if (!si.sess.curr)
		return;

	if (!(parent = xml_parent(si.sess.curr)))
		jabber_process();
	else if (!strcmp(xml_name(si.sess.curr), el))
		si.sess.curr = parent;
}

static void jabber_chardata(void *data, const char *s, int len)
{
	xml_data(si.sess.curr, s, len);
}

static int jabber_callback(void *nb, int event, nbio_fd_t *fdt)
{
	if (event == NBIO_EVENT_READ) {
		char buf[1024];
		int len;

		if ((len = recv(fdt->fd, buf, sizeof(buf)-1, 0)) <= 0) {
			if (errno != 0)
				dvprintf("connection error: %s", strerror(errno));
			return -1;
		}
		buf[len] = '\0';

		dvprintf("%s", buf);

		if (!XML_Parse(si.sess.parser, buf, len, 0)) {
			dvprintf("parser error: %s", XML_ErrorString(XML_GetErrorCode(si.sess.parser)));
			return -1;
		}

		return 0;

	} else if (event == NBIO_EVENT_WRITE) {
		unsigned char *buf;
		int offset, len;

		if (!(buf = nbio_remtoptxvector(nb, fdt, &len, &offset))) {
			dvprintf("EVENT_WRITE, but no finished buffer!");
			return -1;
		}

		free(buf);

		return 0;

	} else if ((event == NBIO_EVENT_ERROR) || (event == NBIO_EVENT_EOF)) {
		dvprintf("connection error! (EVENT_%s)", (event == NBIO_EVENT_ERROR) ? "ERROR" : "EOF");

		nbio_closefdt(nb, fdt);

		return -1;
	}

	dvprintf("jabber_callback: unknown event %d", event);

	return -1;
}

static int jabber_connected(void *nb, int event, nbio_fd_t *fdt)
{
	if (event == NBIO_EVENT_CONNECTED) {
		char stream[1024];

		dvprintf("connected to server");

		if (!(fdt = nbio_addfd(nb, NBIO_FDTYPE_STREAM, fdt->fd, 0, jabber_callback, NULL, 0, 128)))
			return -1;

		si.sess.fdt = fdt;
		nbio_setraw(nb, fdt, 2);

		snprintf(stream, sizeof(stream), "<stream:stream to='%s' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams'>", si.authorizer);
		jabber_send(stream);
	} else if (event == NBIO_EVENT_CONNECTFAILED) {
		dvprintf("unable to connect to %s", si.authorizer);
		nbio_closefdt(nb, fdt);
	}

	return 0;
}

int init_server()
{
	struct sockaddr_in sa;
	struct hostent *hp;

	if (!(si.sess.parser = XML_ParserCreate(NULL)))
		return -1;

	XML_SetElementHandler(si.sess.parser, jabber_start, jabber_end);
	XML_SetCharacterDataHandler(si.sess.parser, jabber_chardata);

	si.sess.id = 1;

	if (!(hp = gethostbyname(si.authorizer)))
		return -1;

	memset(&sa, 0, sizeof(struct sockaddr_in));
	sa.sin_port = htons(si.port);
	memcpy(&sa.sin_addr, hp->h_addr, hp->h_length);
	sa.sin_family = hp->h_addrtype;

	if (nbio_connect(&gnb, (struct sockaddr *)&sa, sizeof(sa), jabber_connected, NULL))
		return -1;

	return 0;
}

void getinfo(char *name)
{
}

void usersearch(char *email)
{
}

void send_im(char *to, char *msg)
{
}

void keepalive()
{
}
