#define _GNU_SOURCE
#include "main.h"
#include <curses.h>
#include <ctype.h>
#include <string.h>

#define BLWID 20
#define ENHEI 4

/** LIST UTIL **/

typedef struct _list {
	struct _list *prev;
	struct _list *next;
	void *data;
} list;

static list *list_new(void *data)
{
	list *l = malloc(sizeof(list));
	l->prev = NULL;
	l->next = NULL;
	l->data = data;
	return l;
}

static unsigned int list_length(list *l)
{
	unsigned int c = 0;

	while (l) {
		l = l->next;
		c++;
	}

	return c;
}

static void *list_nth(list *l, int n)
{
	while (l && n) {
		l = l->next;
		n--;
	}
	if (l) return l->data;
	return NULL;
}

static list *list_append(list *l, void *data)
{
	list *s = l;

	if (!s) return list_new(data);

	while (s->next) s = s->next;
	s->next = list_new(data);
	s->next->prev = s;

	return l;
}

static list *list_prepend(list *l, void *data)
{
	list *s = list_new(data);
	s->next = l;
	if (l)
		l->prev = s;
	return s;
}

static list *list_remove(list *l, void *data)
{
	list *s = l, *p = NULL;

	if (!s) return NULL;
	if (s->data == data) {
		p = s->next;
		if (p)
			p->prev = NULL;
		free(s);
		return p;
	}
	while (s->next) {
		p = s;
		s = s->next;
		if (s->data == data) {
			p->next = s->next;
			if (p->next)
				p->next->prev = p;
			free(s);
			return l;
		}
	} 
	return l;
}

static void list_free(list *l)
{
	while (l) {
		list *s = l;
		l = l->next;
		free(s);
	}
}

/** LIST UTIL (END) **/

static unsigned int cursor_x = 0;
static unsigned int cursor_y = 0;
static char *entry_text = NULL;

struct tab {
	char *title;
	char unseen;
	list *text;
};

static list *tabs = NULL;
static unsigned int cur_tab = 0;

static int sound = 1;

static void draw_blist()
{
	/* XXX */
}

static list *wrap(char *text, int cols)
{
	list *l = NULL;
	char *m = text;

	if (!m || !*m) return list_new(calloc(1, 1));

	while (strlen(m) > cols) {
		char *x = m + cols;
		while (isspace(*x))
			x--;
		while (!isspace(*x))
			x--;
		x++;
		if (x == m) x = m + cols;
		l = list_prepend(l, strndup(m, x - m));
		m = x;
	}

	if (*m) l = list_prepend(l, strdup(m));

	return l;
}

static void draw_list(list *lst, int t, int l, int b, int r, bool cursor)
{
	list *draw = NULL;
	int i = strlen(entry_text);

	int lines = b - t, cols = r - l;

	list *last = lst;
	if (last) while (last->next) last = last->next;

	while (last && lines > 0) {
		list *newlines = wrap(last->data, cols);
		int len = list_length(newlines);
		list *n = newlines;
		while (len-- && lines-- > 0) {
			draw = list_prepend(draw, n->data);
			n = n->next;
		}
		list_free(newlines);
		last = last->prev;
	}

	while (draw) {
		char *d = draw->data;
		mvaddstr(t++, l, draw->data);
		draw = list_remove(draw, draw->data);
		if (cursor) {
			if (strlen(d) > i)
				i -= strlen(d);
			else {
				cursor_x = BLWID + 1 + i;
				cursor_y = t - 1;
				cursor = FALSE;
			}
		}
		free(d);
	}

	move(cursor_y, cursor_x);
}

static void draw_tabs()
{
	int i = 0, pos = BLWID + 2;
	list *t = tabs;
	struct tab *ct = NULL;

	/* draw the horizontal line separating the tabs from the view */
	mvhline(1, BLWID + 1, ACS_HLINE, 100);
	mvaddch(1, BLWID, ACS_LTEE);

	/* clear the old titles */
	move(0, BLWID + 1);
	clrtoeol();

	/* draw the tabs, but only titles except for the cur_tab */
	while (t) {
		struct tab *tab = t->data;
		if (i++ == cur_tab) {
			ct = tab;
			mvaddch(0, pos++, '*');
			pos++;
		}
		if (tab->unseen) {
			mvaddch(0, pos++, '+');
			pos++;
		}
		mvaddstr(0, pos, tab->title);
		pos += strlen(tab->title);
		pos++;
		mvaddch(0, pos, ACS_VLINE);
		mvaddch(1, pos++, ACS_BTEE);
		pos++;
		t = t->next;
	}

	/* clear out the old tab */
	for (i = 2; i < LINES - ENHEI; i++) {
		move(i, BLWID + 1);
		clrtoeol();
	}

	/* now draw the new one */
	draw_list(ct->text, 2, BLWID + 1, LINES - ENHEI, COLS, FALSE);
}

static void draw_entry()
{
	int i;
	list *l;

	/* clear all the old text */
	for (i = LINES - ENHEI + 1; i < LINES; i++) {
		move(i, BLWID + 1);
		clrtoeol();
	}

	/* draw the text. draw_list also places the cursor */
	if (entry_text && *entry_text) {
		l = list_new(entry_text);
		draw_list(l, LINES - ENHEI + 1, BLWID + 1, LINES, COLS - 1, TRUE);
		list_free(l);
	} else {
		cursor_x = BLWID + 1;
		cursor_y = LINES - ENHEI + 1;
		move(cursor_y, cursor_x);
	}
}

void redraw_screen()
{
	/* draw the vertical line */
	mvvline(0, BLWID, ACS_VLINE, 100);

	/* now the horizontal one separating the view from the entry */
	mvhline(LINES - ENHEI, BLWID + 1, ACS_HLINE, 100);
	mvaddch(LINES - ENHEI, BLWID, ACS_LTEE);

	draw_blist();

	/* draw the current tab */
	draw_tabs();

	/* draw the text */
	draw_entry();

	refresh();
}

int init_window()
{
	struct tab *tab;

	initscr();
	keypad(stdscr, TRUE);
	nonl();
	raw();
	noecho();

	tab = calloc(1, sizeof(struct tab));
	tab->title = strdup(PROG);
	tab->unseen = 0;
	tab->text = NULL;

	tabs = list_append(tabs, tab);
	entry_text = calloc(1, 1);

	redraw_screen();

	return watch_stdin();
}

void end_window()
{
	endwin();
}

static char *nospaces(char *x)
{
	static char m[17];
	int i = 0, j = 0;
	while (x[i]) {
		if (x[i] != ' ')
			m[j++] = x[i];
		i++;
	}
	m[j] = 0;
	return m;
}

static struct tab *find_tab(char *who)
{
	char *m = nospaces(who);
	list *l = tabs;
	struct tab *t;

	l = l->next;	/* skip system message tab */
	while (l) {
		t = l->data;
		if (!strcasecmp(t->title, m))
			return t;
		l = l->next;
	}

	t = calloc(1, sizeof(struct tab));
	t->title = strdup(m);
	t->unseen = 0;
	t->text = NULL;
	tabs = list_append(tabs, t);
	return t;
}

static void append_text(struct tab *t, char *x)
{
	char *m;

	while ((m = strchr(x, '\n'))) {
		*m = 0;
		t->text = list_append(t->text, strdup(x));
		*m = '\n';
		x = m + 1;
	}

	t->text = list_append(t->text, strdup(x));
}

#define VALID_TAG(x)		if (!strncasecmp(string, x ">", strlen(x ">")))		\
					return strlen(x) + 1;

#define VALID_OPT_TAG(x)	if (!strncasecmp(string, x " ", strlen(x " "))) {	\
					char *c = string + strlen(x " ");		\
					char e = '"';					\
					char quote = 0;					\
					while (*c) {					\
						if (*c == '"' || *c == '\'') {		\
							if (quote && (*c == e))		\
								quote = !quote;		\
							else if (!quote) {		\
								quote = !quote;		\
								e = *c;			\
							}				\
						} else if (!quote && (*c == '>'))	\
							break;				\
						c++;					\
					}						\
					if (*c)						\
						return c - string + 1;			\
				}

static int is_tag(char *string)
{
	if (!strchr(string, '>'))
		return 0;

	VALID_TAG("B");
	VALID_TAG("BOLD");
	VALID_TAG("/B");
	VALID_TAG("/BOLD");
	VALID_TAG("I");
	VALID_TAG("ITALIC");
	VALID_TAG("/I");
	VALID_TAG("/ITALIC");
	VALID_TAG("U");
	VALID_TAG("UNDERLINE");
	VALID_TAG("/U");
	VALID_TAG("/UNDERLINE");
	VALID_TAG("S");
	VALID_TAG("STRIKE");
	VALID_TAG("/S");
	VALID_TAG("/STRIKE");
	VALID_TAG("SUB");
	VALID_TAG("/SUB");
	VALID_TAG("SUP");
	VALID_TAG("/SUP");
	VALID_TAG("PRE");
	VALID_TAG("/PRE");
	VALID_TAG("TITLE");
	VALID_TAG("/TITLE");
	VALID_TAG("BR");
	VALID_TAG("HR");
	VALID_TAG("/FONT");
	VALID_TAG("/A");
	VALID_TAG("P");
	VALID_TAG("/P");
	VALID_TAG("H3");
	VALID_TAG("/H3");
	VALID_TAG("HTML");
	VALID_TAG("/HTML");
	VALID_TAG("BODY");
	VALID_TAG("/BODY");
	VALID_TAG("FONT");
	VALID_TAG("HEAD");
	VALID_TAG("HEAD");

	VALID_OPT_TAG("HR");
	VALID_OPT_TAG("FONT");
	VALID_OPT_TAG("BODY");
	VALID_OPT_TAG("A");
	VALID_OPT_TAG("IMG");
	VALID_OPT_TAG("P");
	VALID_OPT_TAG("H3");

	if (!strncasecmp(string, "!--", strlen("!--"))) {
		char *e = strstr(string + strlen("!--"), "-->");
		if (e)
			return e - string + strlen("-->");
	}

	return 0;
}

static int is_amp(char *string, char *replace, int *length)
{
	if (!strncasecmp(string, "&amp;", 5)) {
		*replace = '&';
		*length = 5;
	} else if (!strncasecmp(string, "&lt;", 4)) {
		*replace = '<';
		*length = 4;
	} else if (!strncasecmp(string, "&gt;", 4)) {
		*replace = '>';
		*length = 4;
	} else if (!strncasecmp(string, "&nbsp;", 6)) {
		*replace = ' ';
		*length = 6;
	} else if (!strncasecmp(string, "&copy;", 6)) {
		*replace = '�';
		*length = 6;
	} else if (!strncasecmp(string, "&quot;", 6)) {
		*replace = '\"';
		*length = 6;
	} else if (!strncasecmp(string, "&reg;", 5)) {
		*replace = '�';
		*length = 5;
	} else if (*(string + 1) == '#') {
		uint pound = 0;
		if (sscanf(string, "&#%u;", &pound) == 1) {
			int l10 = 0;
			while (pound /= 10) l10++;
			if (*(string + 3 + l10) != ';')
				return 0;
			*replace = (char)pound;
			*length = 2;
			while (isdigit((int)string[*length])) (*length)++;
			if (string[*length] == ';') (*length)++;
		} else {
			return 0;
		}
	} else {
		return 0;
	}

	return 1;
}

static char *strip_html(char *x)
{
	static char *y = NULL;
	int pos = 0, len;
	char amp;

	free(y);
	y = malloc(strlen(x) + 1);

	while (*x) {
		if (*x == '<' && (len = is_tag(x + 1))) {
			x++;
			if (!strncasecmp(x, "BR>", 3))
				y[pos++] = '\n';
			x += len;
		} else if (*x == '&' && is_amp(x, &amp, &len)) {
			y[pos++] = amp;
			x += len;
		} else {
			y[pos++] = *x++;
		}
	}

	y[pos] = 0;
	return y;
}

void got_im(char *from, char *msg, int away)
{
	struct tab *t = find_tab(from);

	time_t tm = time(NULL);
	struct tm *stm = localtime(&tm);

	char *h = strip_html(msg);
	char *x = malloc(strlen(from) + strlen(h) + 100);

	if (!strncasecmp(h, "/me ", 4))
		sprintf(x, "%02d:%02d:%02d %s*** %s %s", stm->tm_hour, stm->tm_min, stm->tm_sec,
				away ? "<AUTO> " : "", from, h + 4);
	else
		sprintf(x, "%02d:%02d:%02d %s%s: %s", stm->tm_hour, stm->tm_min, stm->tm_sec,
				away ? "<AUTO> " : "", from, h);

	append_text(t, x);
	free(x);

	if (list_nth(tabs, cur_tab) != t)
		t->unseen = 1;
	draw_tabs();
	refresh();

	if (sound) play();
}

static void process_command()
{
	char *x = entry_text;
	while (*x == '/') x++;

	if (!strncasecmp(x, "tab ", 4) && x[4]) {
		find_tab(x + 4);
		draw_tabs();
		refresh();
	} else if (!strncasecmp(x, "info ", 5) && x[5]) {
		aim_getinfo(&si.sess, aim_getconn_type(&si.sess, AIM_CONN_TYPE_BOS), x + 5, AIM_GETINFO_GENERALINFO);
	} else if (!strcasecmp(x, "sound off")) {
		sound = 0;
	} else if (!strcasecmp(x, "sound on")) {
		sound = 1;
	} else if (!strcasecmp(x, "sound")) {
		dvprintf("sound is %s", sound ? "on" : "off");
	}
}

static void send_message()
{
	struct tab *t;

	time_t tm;
	struct tm *stm;

	char *h, *x;

	if (!entry_text || !*entry_text)
		return;

	t = list_nth(tabs, cur_tab);

	tm = time(NULL);
	stm = localtime(&tm);

	h = strip_html(entry_text);
	x = malloc(strlen(si.screenname) + strlen(h) + 100);

	aim_send_im(&si.sess, t->title, 0, entry_text);

	if (!strncasecmp(h, "/me ", 4))
		sprintf(x, "%02d:%02d:%02d *** %s %s", stm->tm_hour, stm->tm_min, stm->tm_sec,
				si.screenname, h + 4);
	else
		sprintf(x, "%02d:%02d:%02d %s: %s", stm->tm_hour, stm->tm_min, stm->tm_sec,
				si.screenname, h);

	append_text(t, x);

	draw_tabs();
	refresh();

	free(x);
}

static int stdin_ready(void *nbv, int event, nbio_fd_t *fdt)
{
	struct tab *t;
	int c = getch();
	switch (c) {
	case 3:		/* ^C */
		if (cur_tab) {
			t = list_nth(tabs, cur_tab);
			tabs = list_remove(tabs, t);
			cur_tab--;
			free(t->title);
			while (t->text) {
				char *s = t->text->data;
				t->text = list_remove(t->text, s);
				free(s);
			}
			free(t);
			draw_tabs();
			refresh();
		}
		break;
	case 4:		/* ^D */
		end_window();
		exit(0);
		break;
	case 9:		/* ^I, tab */
		cur_tab++;
		if (cur_tab >= list_length(tabs))
			cur_tab = 0;
		t = list_nth(tabs, cur_tab);
		t->unseen = 0;
		draw_tabs();
		refresh();
		break;
	case 12:	/* ^L */
		redraw_screen();
		break;
	case 13:	/* ^M, enter */
		if (cur_tab)
			send_message();
		else
			process_command();
		*entry_text = 0;
		draw_entry();
		refresh();
		break;
	case 21:	/* ^U */
		*entry_text = 0;
		draw_entry();
		refresh();
		break;
	case 23:	/* ^W */
		c = strlen(entry_text) - 1;
		while (c > 0 && isspace(entry_text[c])) c--;
		while (c + 1 && !isspace(entry_text[c])) c--;
		c++;
		entry_text[c] = 0;
		draw_entry();
		refresh();
		break;
	case 127:	/* backspace */
		if (*entry_text) {
			entry_text[strlen(entry_text)-1] = 0;
			draw_entry();
			refresh();
		}
		break;
	default:
		if (isprint(c)) {
			int l = strlen(entry_text);
			entry_text = realloc(entry_text, l + 2);
			entry_text[l] = c;
			entry_text[l + 1] = 0;
			draw_entry();
			refresh();
		}
		break;
	}
	refresh();
	return 0;
}

int watch_stdin()
{
	nbio_fd_t *fdt;

	if (!(fdt = nbio_addfd(&gnb, NBIO_FDTYPE_STREAM, 0, 0, stdin_ready, NULL, 0, 0))) {
		fprintf(stderr, "Couldn't read stdin\n");
		return 1;
	}
	if (nbio_setraw(&gnb, fdt, 2)) {
		fprintf(stderr, "Couldn't read raw stdin\n");
		return 1;
	}

	return 0;
}

void dvprintf(char *f, ...)
{
	va_list ap;
	char s[8192];
	struct tab *t = tabs->data;
	char *m;

	va_start(ap, f);
	vsprintf(s, f, ap);
	va_end(ap);

	m = strip_html(s);
	append_text(t, m);
	if (cur_tab)
		t->unseen = 1;
	draw_tabs();
	refresh();
}
