/* i hate you, adam. */
#define FAIM_INTERNAL
#define FAIM_INTERNAL_INSANE

#include "main.h"

#define FLAP_LEN 6
#define MAXSNAC 8192

static int aim_nbio_addflapvec(nbio_fd_t *fdt, unsigned char *oldbuf)
{
	unsigned char *buf;

	if (!(buf = oldbuf)) {
		if (!(buf = (unsigned char *)malloc(FLAP_LEN+MAXSNAC)))
			return -1;
	}

	if (nbio_addrxvector(&gnb, fdt, buf, FLAP_LEN, 0) == -1) {
		free(buf);
		return -1;
	}

	return 0;
}

/* libfaim would normally do this internally */
static aim_frame_t *mkrxframe(aim_session_t *sess, aim_conn_t *conn, unsigned char *buf)
{
	fu16_t payloadlen;
	fu8_t *payload = NULL;
	aim_frame_t *newrx;

	if (buf[0] != 0x2a) {
		dvprintf("mkrxframe: invalid FLAP start byte (0x%02x)", buf[0]);
		return NULL;
	}

	payloadlen = aimutil_get16(buf+4);

	if (!(newrx = (aim_frame_t *)malloc(sizeof(aim_frame_t))))
		return NULL;
	memset(newrx, 0, sizeof(aim_frame_t));

	newrx->hdrtype = AIM_FRAMETYPE_FLAP;

	newrx->hdr.flap.type = aimutil_get8(buf+1);
	newrx->hdr.flap.seqnum = aimutil_get16(buf+2);

	newrx->nofree = 0;

	if (payloadlen) {

		if (!(payload = (fu8_t *)malloc(payloadlen))) {
			aim_frame_destroy(newrx);
			return NULL;
		}
	}

	aim_bstream_init(&newrx->data, payload, payloadlen);
	aimbs_putraw(&newrx->data, buf + FLAP_LEN, payloadlen);

	/* position at beginning of stream */
	aim_bstream_rewind(&newrx->data);

	newrx->conn = conn;
	newrx->next = NULL;

	if (!sess->queue_incoming)
		sess->queue_incoming = newrx;
	else {
		aim_frame_t *cur;

		for (cur = sess->queue_incoming; cur->next; cur = cur->next)
			;
		cur->next = newrx;
	}

	return newrx;
}

static int aim_callback(void *nbv, int event, nbio_fd_t *fdt)
{
	nbio_t *nb = (nbio_t *)nbv;
	aim_conn_t *conn = (aim_conn_t *)fdt->priv;
	aim_session_t *sess = (aim_session_t *)conn->sessv;

	if (event == NBIO_EVENT_READ) {
		int len, offset;
		unsigned char *buf;
		fu16_t payloadlen;

		if (!(buf = nbio_remtoprxvector(nb, fdt, &len, &offset))) {
			dvprintf("aim_callback: no data buffer waiting");
			return -1;
		}

		if (buf[0] != 0x2a) {
			dvprintf("aim_callback: invalid FLAP start byte (0x%02x)", buf[0]);
			free(buf);
			return -1;
		}

		payloadlen = aimutil_get16(buf+4);

		if (payloadlen && (len == FLAP_LEN)) {

			if (nbio_addrxvector(nb, fdt, buf, FLAP_LEN + payloadlen, FLAP_LEN) == -1) {
				free(buf);
				return -1;
			}

		} else if (!payloadlen || (len > FLAP_LEN)) {
			aim_frame_t *newrx;

			conn->lastactivity = time(NULL);

			if ((newrx = mkrxframe(sess, conn, buf))) {
				aim_rxdispatch(sess);
				newrx = NULL;
			}

			if (!sess->connlist) {
				dvprintf("no connections left (EVENT_READ)");
				return -1;
			}

			aim_nbio_addflapvec(fdt, buf);
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

		aim_conn_kill(sess, &conn);
		nbio_closefdt(nb, fdt);

		if (!sess->connlist || !aim_getconn_type(sess, AIM_CONN_TYPE_BOS)) {
			dvprintf("no connections left (EVENT_ERROR)");
			return -1;
		}

		return 0;
	}

	dvprintf("aim_callback: unknown event %d", event);

	return -1;
}

static int aim_tx_sendframe_nbio(aim_session_t *sess, aim_frame_t *fr)
{
	int buflen;
	unsigned char *buf;
	nbio_fd_t *fdt;
	aim_bstream_t bufbs;

	if (!(fdt = nbio_getfdt(&gnb, fr->conn->fd))) {
		dvprintf("sendframe: getfdt failed");
		return -1;
	}

	if (!nbio_txavail(&gnb, fdt)) {
		dvprintf("no tx buffers available!");
		return -1;
	}

	if (fr->hdrtype != AIM_FRAMETYPE_FLAP) {
		dvprintf("sendframe: unknown hdrtype");
		return -1;
	}

	buflen = FLAP_LEN + aim_bstream_curpos(&fr->data);
	if (!(buf = (unsigned char *)malloc(buflen)))
		return -1;
	aim_bstream_init(&bufbs, buf, buflen);

	fr->hdr.flap.seqnum = aim_get_next_txseqnum(fr->conn);

	/* FLAP header */
	aimbs_put8(&bufbs, 0x2a);
	aimbs_put8(&bufbs, fr->hdr.flap.type);
	aimbs_put16(&bufbs, fr->hdr.flap.seqnum);
	aimbs_put16(&bufbs, aim_bstream_curpos(&fr->data));

	/* payload */
	aim_bstream_rewind(&fr->data);
	aimbs_putbs(&bufbs, &fr->data, buflen - FLAP_LEN);

	fr->handled = 1;

	if (nbio_addtxvector(&gnb, fdt, bufbs.data, bufbs.offset) == -1) {
		dvprintf("nbio_addtxvector: %s", strerror(errno));
		free(buf);
		fr->conn->seqnum--;
		return 0; /* not fatal */
	}

	fr->conn->lastactivity = time(NULL);

	return 0;
}

static int aim_tx_enqueue__nbio(aim_session_t *sess, aim_frame_t *fr)
{

	if (!fr->conn) {
		dvprintf("aim_tx_enqueue__nbio: ERROR: packet has no connection");
		aim_frame_destroy(fr);
		return -1;
	}

	aim_tx_sendframe_nbio(sess, fr);

	aim_frame_destroy(fr);

	return 0;
}

static void debugcb(aim_session_t *sess, int level, const char *format, va_list va)
{
	return;
}

static nbio_fd_t *addaimconn(aim_conn_t *conn)
{
	nbio_fd_t *fdt;

	if (!(fdt = nbio_addfd(&gnb, NBIO_FDTYPE_STREAM, conn->fd, 0, aim_callback, conn, 1, 128)))
		return NULL;

	aim_nbio_addflapvec(fdt, NULL);

	return fdt;
}

static int cb_conninitdone_bos(aim_session_t *sess, aim_frame_t *fr, ...)
{
	aim_reqpersonalinfo(sess, fr->conn);
	aim_bos_reqlocaterights(sess, fr->conn);
	aim_bos_setprofile(sess, fr->conn, NULL, NULL, 0);
	aim_bos_reqbuddyrights(sess, fr->conn);
	aim_reqicbmparams(sess);
	aim_bos_reqrights(sess, fr->conn);
	aim_bos_setgroupperm(sess, fr->conn, AIM_FLAG_ALLUSERS);
	aim_bos_setprivacyflags(sess, fr->conn, 0);

	return 1;
}

static int cb_bosrights(aim_session_t *sess, aim_frame_t *fr, ...)
{
	aim_clientready(sess, fr->conn);

	dvprintf("aim: officially connected to BOS.");

	return 1;
}

static int cb_icbmparaminfo(aim_session_t *sess, aim_frame_t *fr, ...)
{
	struct aim_icbmparameters *params;
	va_list ap;

	va_start(ap, fr);
	params = va_arg(ap, struct aim_icbmparameters *);
	va_end(ap);

	params->maxmsglen = 8000;
	params->minmsginterval = 0; /* in milliseconds */

	aim_seticbmparam(sess, params);

	return 1;
}

static int cb_connerr(aim_session_t *sess, aim_frame_t *fr, ...)
{
	va_list ap;
	unsigned short code;
	char *msg = NULL;

	va_start(ap, fr);
	code = va_arg(ap, int);
	msg = va_arg(ap, char *);
	va_end(ap);

	si.killme = 1;

	return 1;
}

static int cb_oncoming(aim_session_t *sess, aim_frame_t *fr, ...)
{
	va_list ap;
	aim_userinfo_t *info;

	va_start(ap, fr);
	info = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	/* XXX */

	return 1;
}

static int cb_offgoing(aim_session_t *sess, aim_frame_t *fr, ...)
{
	va_list ap;
	aim_userinfo_t *info;

	va_start(ap, fr);
	info = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	/* XXX */

	return 1;
}

static int cb_incomingim(aim_session_t *sess, aim_frame_t *fr, ...)
{
	fu16_t channel;
	aim_userinfo_t *userinfo;
	struct aim_incomingim_ch1_args *args;
	va_list ap;

	va_start(ap, fr);
	channel = (fu16_t)va_arg(ap, unsigned int);
	userinfo = va_arg(ap, aim_userinfo_t *);

	if (channel != 1) {
		va_end(ap);
		dvprintf("unsupported channel 0x%04x", channel);
		return 1;
	}

	args = va_arg(ap, struct aim_incomingim_ch1_args *);
	va_end(ap);

	got_im(userinfo->sn, args->msg, args->icbmflags & AIM_IMFLAGS_AWAY);

	return 1;
}

static int cb_parse_err(aim_session_t *sess, aim_frame_t *fr, ...)
{
	static char *errs[] = {
		"Invalid error", 
		"Invalid SNAC",
		"Rate to host",
		"Rate to client",
		"Not logged on",
		"Service unavailable",
		"Service not defined",
		"Obsolete SNAC",
		"Not supported by host",
		"Not supported by client",
		"Refused by client",
		"Reply too big",
		"Responses lost",
		"Request denied",
		"Busted SNAC payload",
		"Insufficient rights",
		"In local permit/deny",
		"Too evil (sender)",
		"Too evil (receiver)",
		"User temporarily unavailable",
		"No match", 
		"List overflow",
		"Request ambiguous",
		"Queue full",
		"Not while on AOL",
	};
	static int errlen = 25;

	va_list ap;
	char *destn; 
	fu16_t reason; 

	va_start(ap, fr);
	reason = (fu16_t)va_arg(ap, unsigned int);
	destn = va_arg(ap, char *);
	va_end(ap);

	if (destn)
		dvprintf("%s: %s", destn, reason < errlen ? errs[reason] : "unknown reason");

	return 1;
}

static int cb_parse_evilnotify(aim_session_t *sess, aim_frame_t *fr, ...)
{
	va_list ap;
	int newevil;
	aim_userinfo_t *userinfo;

	va_start(ap, fr);
	newevil = va_arg(ap, int);
	userinfo = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	dvprintf("warned by %s, now at %f", userinfo->sn && *userinfo->sn ? userinfo->sn : "anon",
		 aim_userinfo_warnlevel(userinfo));

	return 1;
}

static int cb_parse_userinfo(aim_session_t *sess, aim_frame_t *fr, ...)
{
	aim_userinfo_t *userinfo;
	char *prof_encoding = NULL;
	char *prof = NULL;
	fu16_t inforeq = 0;

	va_list ap;
	va_start(ap, fr);
	userinfo = va_arg(ap, aim_userinfo_t *);
	inforeq = (fu16_t)va_arg(ap, unsigned int);
	prof_encoding = va_arg(ap, char *);
	prof = va_arg(ap, char *);
	va_end(ap);

	if (inforeq == AIM_GETINFO_GENERALINFO) {
		char buf[26]; ctime_r(&userinfo->onlinesince, buf); buf[strlen(buf)-1] = 0;
		if (userinfo->idletime)
			dvprintf("%s: idletime: %d", userinfo->sn, userinfo->idletime);
		if (aim_userinfo_warnlevel(userinfo))
			dvprintf("%s: warnlevel: %f", userinfo->sn, aim_userinfo_warnlevel(userinfo));
		dvprintf("%s: onlinesince: %s", userinfo->sn, buf);
		if (prof && *prof) dvprintf("%s:\n%s", userinfo->sn, prof);

		if (userinfo->flags & AIM_FLAG_AWAY)
			aim_getinfo(sess, fr->conn, userinfo->sn, AIM_GETINFO_AWAYMESSAGE);
	} else
		dvprintf("%s:\n%s", userinfo->sn, prof);

	return 1;
}

static int cb_parse_authresp(aim_session_t *sess, aim_frame_t *fr, ...)
{
	va_list ap;
	struct aim_authresp_info *info;
	aim_conn_t *bosconn;
	nbio_fd_t *authfdt, *bosfdt;

	va_start(ap, fr);
	info = va_arg(ap, struct aim_authresp_info *);
	va_end(ap);

	dvprintf("aim: screen name: %s", info->sn);
	
	if (info->errorcode || !info->bosip || !info->cookie) {
		si.killme = 1;
		return 1;
	}

	dvprintf("aim: Associated email: %s / BOS IP: %s", info->email, info->bosip);

	authfdt = nbio_getfdt(&gnb, fr->conn->fd);
	aim_conn_kill(sess, &fr->conn);
	nbio_closefdt(&gnb, authfdt);

	if (!(bosconn = aim_newconn(sess, AIM_CONN_TYPE_BOS, info->bosip))) {
		si.killme = 1;
		return 1;
	} else if ((bosconn->fd == -1) || (bosconn->status & AIM_CONN_STATUS_CONNERR)) {
		si.killme = 1;
		return 1;
	}

	if (!(bosfdt = addaimconn(bosconn))) {
		aim_conn_kill(sess, &bosconn);
		si.killme = 1;
		return 1;
	}

	aim_conn_addhandler(sess, bosconn, 0x0004, 0x0005, cb_icbmparaminfo, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNINITDONE, cb_conninitdone_bos, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_BOS, AIM_CB_BOS_RIGHTS, cb_bosrights, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_BUD, AIM_CB_BUD_ONCOMING, cb_oncoming, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_BUD, AIM_CB_BUD_OFFGOING, cb_offgoing, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_INCOMING, cb_incomingim, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_ERROR, cb_parse_err, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_LOC, AIM_CB_LOC_ERROR, cb_parse_err, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR, cb_connerr, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_ERROR, cb_parse_evilnotify, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_LOC, AIM_CB_LOC_USERINFO, cb_parse_userinfo, 0);

	aim_sendcookie(sess, bosconn, info->cookie);

	return 1;
}

static int cb_parse_login(aim_session_t *sess, aim_frame_t *fr, ...)
{
	struct client_info_s info = AIM_CLIENTINFO_KNOWNGOOD;
	char *key;
	va_list ap;

	va_start(ap, fr);
	key = va_arg(ap, char *);
	va_end(ap);

	aim_send_login(sess, fr->conn, si.screenname, si.password, &info, key);

	return 1;
}

int init_faim()
{
	aim_conn_t *authconn;

	aim_session_init(&si.sess, 0, 0);
	aim_setdebuggingcb(&si.sess, debugcb); /* still needed even if debuglevel = 0 ! */

	aim_tx_setenqueue(&si.sess, AIM_TX_USER, &aim_tx_enqueue__nbio);

	dvprintf("aim: connecting to %s", si.authorizer);

	if (!(authconn = aim_newconn(&si.sess, AIM_CONN_TYPE_AUTH, si.authorizer))) {
		dvprintf("faim: internal connection error");
		aim_session_kill(&si.sess);
		return -1;
	} else if (authconn->fd == -1) {
		if (authconn->status & AIM_CONN_STATUS_RESOLVERR) {
			dvprintf("faim: could not resolve authorizer name");
		} else if (authconn->status & AIM_CONN_STATUS_CONNERR) {
			dvprintf("could not connect to authorizer");
		}

		aim_conn_kill(&si.sess, &authconn);
		aim_session_kill(&si.sess);

		return -1;
	} 

	if (!addaimconn(authconn)) {
		aim_conn_kill(&si.sess, &authconn);
		si.killme = 1;
		return 1;
	}

	aim_conn_addhandler(&si.sess, authconn, 0x0017, 0x0007, cb_parse_login, 0);
	aim_conn_addhandler(&si.sess, authconn, 0x0017, 0x0003, cb_parse_authresp, 0);

	aim_request_login(&si.sess, authconn, si.screenname);

	return 0;
}
