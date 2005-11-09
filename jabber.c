#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "xml.h"

typedef struct _jiq {
	int id;
	void (*cb)();
} jiq;

static void jabber_send(char *stream)
{
	char *buf = strdup(stream);

	/* dvprintf("sent %d: %s", strlen(buf), buf); */

	if (nbio_addtxvector(&gnb, si.sess.fdt, buf, strlen(buf)) == -1) {
		dvprintf("nbio_addtxvector: %s", strerror(errno));
		free(buf);
	}
}

static void jabber_send_iq(char *stream, int id, void (*cb)())
{
	jiq *j = malloc(sizeof (jiq));
	if (!j)
		return;

	j->id = id;
	j->cb = cb;

	si.sess.iqs = list_append(si.sess.iqs, j);

	jabber_send(stream);
}

static void jabber_process()
{
	if (!strcmp(xml_name(si.sess.curr), "iq")) {
		const char *type = xml_get_attrib(si.sess.curr, "type");
		if (!type)
			return;
		if (!strcasecmp(type, "result")) {
			const char *id = xml_get_attrib(si.sess.curr, "id");
			int q;
			list *l = si.sess.iqs;
			if (!id) {
				dvprintf("result with no id");
				xml_free(si.sess.curr);
				si.sess.curr = NULL;
				return;
			}
			q = atoi(id);
			while (l) {
				jiq *j = l->data;
				if (j->id == q) {
					j->cb();
					break;
				}
				l = l->next;
			}
			if (!l)
				dvprintf("result with no callback");
		} else if (!strcasecmp(type, "error")) {
			dvprintf("error");
		} else {
			dvprintf("unhandled iq");
		}
	} else if (!strcmp(xml_name(si.sess.curr), "presence")) {
		char *from = xml_get_attrib(si.sess.curr, "from");
		char *type = xml_get_attrib(si.sess.curr, "type");
		void *status = xml_get_child(si.sess.curr, "status");
		char *slash;
		if (!from || !status) {
			dvprintf("presence with no %s", from ? "status" : "from");
			xml_free(si.sess.curr);
			si.sess.curr = NULL;
			return;
		}
		if ((slash = strchr(from, '/')) != NULL)
			*slash = 0;
		if (type && !strcmp(type, "unavailable"))
			buddy_state(from, 0);
		else
			buddy_state(from, 1);
	} else if (!strcmp(xml_name(si.sess.curr), "message")) {
		char *from = xml_get_attrib(si.sess.curr, "from");
		char *body = xml_get_child(si.sess.curr, "body");
		char *msg = NULL;
		char *slash;
		if (body)
			msg = xml_get_data(body);
		if (!from || !msg) {
			dvprintf("message with no %s", from ? "msg" : "from");
			xml_free(si.sess.curr);
			si.sess.curr = NULL;
			return;
		}
		if ((slash = strchr(from, '/')) != NULL)
			*slash = 0;
		got_im(from, msg, 0);
	} else
		dvprintf("unhandled xml parent %s", xml_name(si.sess.curr));

	xml_free(si.sess.curr);
	si.sess.curr = NULL;
}

static void jabber_roster_cb()
{
	void *query;

	dvprintf("online");

	query = xml_get_child(si.sess.curr, "query");
	add_group("Buddies", 1);
	if (query) {
		list *children = xml_get_children(query);
		while (children) {
			void *item = children->data;
			char *jid, *sub;
			children = children->next;
			sub = xml_get_attrib(item, "subscription");
			if (!sub) {
				dvprintf("no subscription information");
				continue;
			}
			if (strcmp(sub, "both") && strcmp(sub, "to")) {
				dvprintf("subscription not to");
				continue;
			}
			jid = xml_get_attrib(item, "jid");
			if (!jid) {
				dvprintf("no jid?");
				continue;
			}
			add_buddy(jid, 1);
		}
	}

	jabber_send("<presence><status>Online</status><priority>9</priority></presence>");
}

static void jabber_auth_cb()
{
	char roster[1024];

	dvprintf("authenticated");

	snprintf(roster, sizeof(roster), "<iq id='%d' type='get'><query xmlns='jabber:iq:roster'/></iq>", si.sess.id);
	jabber_send_iq(roster, si.sess.id++, jabber_roster_cb);
}

static void jabber_auth(int type)
{
	char auth[1024];
	int n;

	n = snprintf(auth, sizeof(auth), "<iq id='%d' type='set'><query xmlns='jabber:iq:auth'><username>%s</username><resource>%s</resource>",
		     si.sess.id, si.screenname, si.resource ? si.resource : "grim");

#if 0
	if (type == 0) {
	} else if (type == 1) {
		n += snprintf(auth + n, sizeof(auth) - n, "<digest>%s</digest>", si.password);
	} else if (type == 2) {
		n += snprintf(auth + n, sizeof(auth) - n, "<password>%s</password>", si.password);
	}
#else
	n += snprintf(auth + n, sizeof(auth) - n, "<password>%s</password>", si.password);
#endif

	snprintf(auth + n, sizeof(auth) - n, "</query></iq>");

	jabber_send_iq(auth, si.sess.id++, jabber_auth_cb);
}

static void jabber_start_cb()
{
	void *query = xml_get_child(si.sess.curr, "query");
	if (!query) {
		dvprintf("auth with no query");
		return;
	}
	if (xml_get_child(query, "sequence") && xml_get_child(query, "token"))
		jabber_auth(0);
	else if (xml_get_child(query, "digest"))
		jabber_auth(1);
	else if (xml_get_child(query, "password"))
		jabber_auth(2);
}

static void jabber_start(void *data, const char *el, const char **attr)
{
	int i;

	if (!strcmp(el, "stream:stream")) {
		char iq[1024];
		snprintf(iq, sizeof(iq), "<iq id='%d' type='get'><query xmlns='jabber:iq:auth'><username>%s</username></query></iq>", si.sess.id, si.screenname);
		jabber_send_iq(iq, si.sess.id++, jabber_start_cb);
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

		/* dvprintf("recv %d: %s", strlen(buf), buf); */

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
	int len = strlen(msg) + strlen(to) + 1024;
	char *send = malloc(len);
	if (!send)
		return;
	snprintf(send, len, "<message to='%s' type='chat'><body>%s</body></message>", to, msg);
	jabber_send(send);
	free(send);
}

void keepalive()
{
}
