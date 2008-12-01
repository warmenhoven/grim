#include <errno.h>
#include <expat.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "sha1.h"
#include "xml.h"

typedef struct _jiq {
	int id;
	void (*cb)();
} jiq;

typedef struct _jabber_session {
	nbio_fd_t *fdt;
	XML_Parser parser;
	char *streamid;
	int id;
	void *curr;
	list *iqs;
} jabber_session_t;

static jabber_session_t sess;

static void jabber_roster_cb(void);

static void
log_xml(char *xml, int send)
{
	static FILE *f = NULL;

	if (!f) {
		char path[8192];
		sprintf(path, "%s/xml", mydir());
		if (!(f = fopen(path, "w+"))) {
			fprintf(stderr, "Can't write %s\n", path);
			return;
		}
	}
	fprintf(f, "%s %d: %s\n", send ? "send" : "recv", strlen(xml), xml);
	fflush(f);
}


static void
jabber_send(char *stream)
{
	char *buf = strdup(stream);

	if (*stream != '\t')
		log_xml(stream, 1);

	if (nbio_addtxvector(&gnb, sess.fdt,
						 (uint8_t *)buf, strlen(buf)) == -1) {
		dvprintf("nbio_addtxvector: %s", strerror(errno));
		free(buf);
	}
}

static void
jabber_send_iq(char *stream, int id, void (*cb)())
{
	jiq *j = malloc(sizeof (jiq));
	if (!j)
		return;

	j->id = id;
	j->cb = cb;

	sess.iqs = list_append(sess.iqs, j);

	jabber_send(stream);
}

static void
jabber_process_iq()
{
	const char *type = xml_get_attrib(sess.curr, "type");
	if (!type)
		return;
	if (!strcasecmp(type, "result")) {
		const char *id = xml_get_attrib(sess.curr, "id");
		int q;
		list *l = sess.iqs;
		if (!id) {
			dvprintf("result with no id");
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
	} else if (!strcasecmp(type, "set")) {
		jabber_roster_cb();
	} else if (!strcasecmp(type, "error")) {
		dvprintf("error");
	}
}

static int
from_me(char *jid)
{
	char *server;
	char *x;

	if (!jid)
		return (0);

	x = strchr(jid, '@');
	if (!x) {
		return (!strcmp(jid, si.jid));
	} else {
		*x = '\0';
		if (strcmp(jid, si.jid)) {
			*x = '@';
			return (0);
		}
		*x = '@';
	}

	x++;
	server = x;
	x = strchr(server, '/');
	if (!x) {
		return (!strcmp(server, si.jserver));
	} else {
		*x = '\0';
		if (strcmp(server, si.jserver)) {
			*x = '/';
			return (0);
		}
		*x = '/';
		return 1;
	}
}

static void
jabber_process_presence()
{
	char *from = xml_get_attrib(sess.curr, "from");
	char *type = xml_get_attrib(sess.curr, "type");
	void *status = xml_get_child(sess.curr, "status");
	char *slash;

	if (type && !strcasecmp(type, "error")) {
		dvprintf("error in presence: from %s", from ? from : "not present");
		return;
	}

	if (!from) {
		dvprintf("presence with no from");
		return;
	}
	if (from_me(from)) {
		return;
	}

	if ((slash = strchr(from, '/')) != NULL)
		*slash = 0;

	if (type && !strcasecmp(type, "unavailable")) {
		buddy_state(from, 0);
	} else if (status) {
		buddy_state(from, 1);
	} else if (slash) {
		buddy_state(from, 1);
	} else {
		/* dvprintf("presence with no discernable status from %s", from); */
	}
}

static void
jabber_process_message()
{
	char *from = xml_get_attrib(sess.curr, "from");
	char *type = xml_get_attrib(sess.curr, "type");
	char *body = xml_get_child(sess.curr, "body");
	char *msg = NULL;
	char *slash;
	if (type && !strcasecmp(type, "error")) {
		dvprintf("error sending to %s", from);
		return;
	}
	if (body)
		msg = xml_get_data(body);
	if (!from) {
		dvprintf("message with no from");
		return;
	}
	if (!msg) {
		dvprintf("message with no msg from %s", from);
		return;
	}
	if ((slash = strchr(from, '/')) != NULL)
		*slash = 0;
	if (type && !strcasecmp(type, "headline")) {
		char *subject = xml_get_child(sess.curr, "subject");
		dvprintf("headline from %s:", from);
		if (subject) dvprintf("subject: %s", xml_get_data(subject));
		dvprintf("%s", msg);
	} else if (strcasecmp(from, keepalive_user) || strcmp(msg, KEEPALIVE)) {
		got_im(from, msg, 0);
	}
}

static void
jabber_process()
{
	if (!strcasecmp(xml_name(sess.curr), "iq")) {
		jabber_process_iq();
	} else if (!strcasecmp(xml_name(sess.curr), "presence")) {
		jabber_process_presence();
	} else if (!strcasecmp(xml_name(sess.curr), "message")) {
		jabber_process_message();
	} else
		dvprintf("unhandled xml parent %s", xml_name(sess.curr));

	xml_free(sess.curr);
	sess.curr = NULL;
}

static void
jabber_roster_cb()
{
	void *query;
        char presence[1024];

	dvprintf("online");

	query = xml_get_child(sess.curr, "query");
	add_group("Buddies", 1);
	if (query) {
		list *children = xml_get_children(query);
		while (children) {
			void *item = children->data;
			char *jid, *sub;
			char *slash;
			children = children->next;
			sub = xml_get_attrib(item, "subscription");
			if (!sub) {
				dvprintf("no subscription information");
				continue;
			}
			jid = xml_get_attrib(item, "jid");
			if (!jid) {
				dvprintf("no jid?");
				continue;
			}
			if (!strcasecmp(sub, "none")) {
				dvprintf("no subscription to %s", jid);
				continue;
			}
			if ((slash = strchr(jid, '/')) != NULL)
				*slash = 0;
			add_buddy(jid, 1);
		}
	}

	snprintf(presence, sizeof (presence),
		 "<presence><status>Online</status><priority>%d</priority></presence>",
	         si.priority);
        jabber_send(presence);
}

static void
jabber_auth_cb()
{
	char roster[1024];

	dvprintf("authenticated");

	snprintf(roster, sizeof (roster),
			 "<iq id='%d' type='get'><query xmlns='jabber:iq:roster'/></iq>",
			 sess.id);
	jabber_send_iq(roster, sess.id++, jabber_roster_cb);
}

static void
jabber_auth(int type)
{
	char auth[1024];
	int n;

	n = snprintf(auth, sizeof (auth),
				 "<iq id='%d' type='set'>"
				   "<query xmlns='jabber:iq:auth'>"
				     "<username>%s</username>"
				     "<resource>%s</resource>",
				 sess.id,
				 si.jid, si.resource ? si.resource : "grim");

	if (type == 1) {
		SHA1Context sha1ctxt;
		uint8_t digest[SHA1HashSize];
		int i;

		SHA1Reset(&sha1ctxt);
		SHA1Input(&sha1ctxt, sess.streamid, strlen(sess.streamid));
		SHA1Input(&sha1ctxt, si.key, strlen(si.key));
		SHA1Result(&sha1ctxt, digest);

		n += snprintf(auth + n, sizeof (auth) - n, "<digest>");
		for (i = 0; i < SHA1HashSize; i++)
			n += snprintf(auth + n, sizeof (auth) - n, "%02x", digest[i]);
		n += snprintf(auth + n, sizeof (auth) - n, "</digest>");
	} else if (type == 2) {
		n += snprintf(auth + n, sizeof (auth) - n,
					  "<password>%s</password>", si.key);
	}

	snprintf(auth + n, sizeof (auth) - n, "</query></iq>");

	jabber_send_iq(auth, sess.id++, jabber_auth_cb);
}

static void
jabber_start_cb()
{
	void *query = xml_get_child(sess.curr, "query");
	if (!query) {
		dvprintf("auth with no query");
		return;
	}
	if (xml_get_child(query, "digest"))
		jabber_auth(1);
	else if (xml_get_child(query, "password"))
		jabber_auth(2);
	else
		dvprintf("unknown auth query");
}

static void
jabber_start(void *data, const char *el, const char **attr)
{
	int i;

	if (!strcasecmp(el, "stream:stream")) {
		char iq[1024];

		sess.streamid = NULL;
		for (i = 0; attr[i]; i += 2) {
			if (!strcasecmp(attr[i], "id")) {
				sess.streamid = strdup(attr[i + 1]);
			}
		}

		snprintf(iq, sizeof (iq),
				 "<iq id='%d' type='get'>"
				   "<query xmlns='jabber:iq:auth'>"
				     "<username>%s</username>"
				   "</query>"
				 "</iq>",
				 sess.id, si.jid);
		jabber_send_iq(iq, sess.id++, jabber_start_cb);
		return;
	}

	if (sess.curr)
		sess.curr = xml_child(sess.curr, el);
	else
		sess.curr = xml_new(el);

	for (i = 0; attr[i]; i += 2)
		xml_attrib(sess.curr, attr[i], attr[i + 1]);
}

static void
jabber_end(void *data, const char *el)
{
	void *parent;

	if (!sess.curr)
		return;

	if (!(parent = xml_parent(sess.curr)))
		jabber_process();
	else if (!strcasecmp(xml_name(sess.curr), el))
		sess.curr = parent;
}

static void
jabber_chardata(void *data, const char *s, int len)
{
	xml_data(sess.curr, s, len);
}

static int
jabber_callback(void *nb, int event, nbio_fd_t *fdt)
{
	if (event == NBIO_EVENT_READ) {
		char buf[1024];
		int len;

		if ((len = recv(fdt->fd, buf, sizeof (buf) - 1, 0)) <= 0) {
			if (errno != 0)
				dvprintf("connection error: %s", strerror(errno));
			return (-1);
		}
		buf[len] = '\0';

		log_xml(buf, 0);

		if (!XML_Parse(sess.parser, buf, len, 0)) {
			dvprintf("parser error: %s",
					 XML_ErrorString(XML_GetErrorCode(sess.parser)));
			return (-1);
		}

		return (0);

	} else if (event == NBIO_EVENT_WRITE) {
		unsigned char *buf;
		int offset, len;

		if (!(buf = nbio_remtoptxvector(nb, fdt, &len, &offset))) {
			dvprintf("EVENT_WRITE, but no finished buffer!");
			return (-1);
		}

		free(buf);

		return (0);

	} else if ((event == NBIO_EVENT_ERROR) || (event == NBIO_EVENT_EOF)) {
		dvprintf("connection error! (EVENT_%s)",
				 (event == NBIO_EVENT_ERROR) ? "ERROR" : "EOF");

		nbio_closefdt(nb, fdt);

		return (-1);
	}

	dvprintf("jabber_callback: unknown event %d", event);

	return (-1);
}

static int
jabber_connected(void *nb, int event, nbio_fd_t *fdt)
{
	if (event == NBIO_EVENT_CONNECTED) {
		char stream[1024];

		dvprintf("connected to server");

		if (!(fdt = nbio_addfd(nb, NBIO_FDTYPE_STREAM, fdt->fd, 0,
							   jabber_callback, NULL, 0, 128)))
			return (-1);

		sess.fdt = fdt;
		nbio_setraw(nb, fdt, 2);

		snprintf(stream, sizeof (stream),
				 "<stream:stream "
				   "to='%s' "
				   "xmlns='jabber:client' "
				   "xmlns:stream='http://etherx.jabber.org/streams'"
				 ">", si.jserver);
		jabber_send(stream);
	} else if (event == NBIO_EVENT_CONNECTFAILED) {
		dvprintf("unable to connect to %s", si.jserver);
		nbio_closefdt(nb, fdt);
		return (-1);
	}

	return (0);
}

int
init_server()
{
	struct sockaddr_in sa;
	struct hostent *hp;

	si.displayname = si.jid;

	if (!(sess.parser = XML_ParserCreate(NULL)))
		return (-1);

	XML_SetElementHandler(sess.parser, jabber_start, jabber_end);
	XML_SetCharacterDataHandler(sess.parser, jabber_chardata);

	if (!(hp = gethostbyname(si.jserver)))
		return (-1);

	memset(&sa, 0, sizeof (struct sockaddr_in));
	sa.sin_port = htons(si.jport);
	memcpy(&sa.sin_addr, hp->h_addr, hp->h_length);
	sa.sin_family = hp->h_addrtype;

	if (nbio_connect(&gnb, (struct sockaddr *)&sa, sizeof (sa),
					 jabber_connected, NULL))
		return (-1);

	return (0);
}

void
getinfo(char *name)
{
}

void
usersearch(char *email)
{
}

void
send_im(char *to, char *msg)
{
	int len = (strlen(msg) * 5) + strlen(to) + 1024;
	char *fmt = malloc(len);
	char *send = malloc(len);
	int i, j;
	if (!send || !fmt) {
		if (fmt)
			free(fmt);
		if (send)
			free(send);
		return;
	}
	for (i = 0, j = 0; msg[i]; i++) {
		if (msg[i] == '&') {
			fmt[j++] = '&';
			fmt[j++] = 'a';
			fmt[j++] = 'm';
			fmt[j++] = 'p';
			fmt[j++] = ';';
		} else if (msg[i] == '<') {
			fmt[j++] = '&';
			fmt[j++] = 'l';
			fmt[j++] = 't';
			fmt[j++] = ';';
		} else if (msg[i] == '>') {
			fmt[j++] = '&';
			fmt[j++] = 'g';
			fmt[j++] = 't';
			fmt[j++] = ';';
		} else {
			fmt[j++] = msg[i];
		}
	}
	fmt[j++] = 0;
	snprintf(send, len,
			 "<message to='%s' type='chat'>"
			   "<body>%s</body>"
			 "</message>",
			 to, fmt);
	jabber_send(send);
	free(send);
	free(fmt);
}

void
keepalive()
{
	jabber_send("\t");
	if (keepalive_user[0]) {
		send_im(keepalive_user, KEEPALIVE);
	}
}

void
presence(char *to, int avail)
{
	int len = strlen(to) + 1024;
	char *send = malloc(len);
	if (!send)
		return;
	if (avail)
		snprintf(send, len,
				 "<presence to='%s'>"
				   "<status>Available</status>"
				 "</presence>",
				 to);
	else
		snprintf(send, len, "<presence to='%s' type='unavailable' />", to);
	jabber_send(send);
	free(send);
}

void away(char *msg)
{
}

/* vim:set sw=4 ts=4 ai noet cindent tw=80: */
