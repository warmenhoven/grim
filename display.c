#define _GNU_SOURCE
#include <ctype.h>
#include <curses.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "list.h"
#include "main.h"

static int max_blwid = -1;
static int BLWID = -1;
#define ENHEI 4

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

static int isaway = 0;
static char *awaymsg;
static int sound = 1;
static int newlines = 1;
static int print_anyway = 0;
static int sp[2];

struct group {
	char *name;
	short gid;
	list *members;
};

struct buddy {
	char *name;
	int   state;
	int   stalk;
};

static list *pals = NULL;
static list *notfound = NULL;

static char *nospaces(char *x)
{
	static char m[256];
	int i = 0, j = 0;
	while (x[i]) {
		if (x[i] != ' ')
			m[j++] = x[i];
		i++;
	}
	m[j] = 0;
	return m;
}

static void draw_blist()
{
	int pos = 0, i, j;
	list *p = pals;
	if (BLWID < 0)
		return;
	for (i = 0; i < COLS; i++)
		for (j = 0; j < BLWID; j++)
			mvaddch(i, j, ' ');
	while (p) {
		struct group *g = p->data;
		list *m = g->members;
		while (m) {
			struct buddy *b = m->data;
			if (b->state) {
				mvaddstr(pos++, 0, b->name);
			}
			m = m->next;
		}
		p = p->next;
	}
	pos++;
	p = pals;
	while (p) {
		struct group *g = p->data;
		list *m = g->members;
		while (m) {
			struct buddy *b = m->data;
			if (!b->state) {
				mvaddstr(pos++, 0, b->name);
			}
			m = m->next;
		}
		p = p->next;
	}

	move(cursor_y, cursor_x);
}

static struct buddy *find_buddy(char *name)
{
	list *l = pals;
	while (l) {
		struct group *g = l->data;
		list *m = g->members;
		while (m) {
			struct buddy *b = m->data;
			if (!strcasecmp(b->name, nospaces(name))) {
				return (b);
			}
			m = m->next;
		}
		l = l->next;
	}
	return (NULL);
}

void add_group(char *name, short gid)
{
	list *l = pals;
	struct group *g;
	while (l) {
		g = l->data;
		if (g->gid == gid)
			return;
		l = l->next;
	}
	g = malloc(sizeof (struct group));
	g->name = strdup(name);
	g->gid = gid;
	g->members = NULL;
	pals = list_append(pals, g);
}

void add_buddy(char *name, short gid)
{
	list *l = pals;
	struct group *g;
	struct buddy *b;
	int redraw = 0;
	while (l) {
		g = l->data;
		if (g->gid == gid)
			break;
		l = l->next;
	}
	if (!l)
		return;

	if (find_buddy(name))
		return;

	b = malloc(sizeof (struct buddy));
	b->name = strdup(nospaces(name));
	if ((int)strlen(b->name) >= max_blwid)
		max_blwid = strlen(b->name) + 1;
	b->state = 0;
	b->stalk = 0;
	if (notfound) {
		l = notfound;
		while (l) {
			struct buddy *t = l->data;
			if (!strcasecmp(b->name, t->name)) {
				free(b->name);
				b->name = t->name;
				b->state = t->state;
				notfound = list_remove(notfound, t);
				free(t);
				l = notfound;
				redraw = 1;
				continue;
			}
			l = l->next;
		}
	}
	g->members = list_append(g->members, b);
	draw_blist();
	refresh();
}

void buddy_state(char *name, int state)
{
	struct buddy *b = find_buddy(name);
	if (b) {
		if (strcmp(b->name, nospaces(name)))
			strcpy(b->name, nospaces(name));
		if (b->state != state) {
			if (b->stalk) {
				time_t tm = time(NULL);
				struct tm *stm = localtime(&tm);

				dvprintf("%s %s at %d/%d %d:%d:%d", b->name,
						 state ? "online" : "offline",
						 stm->tm_mon, stm->tm_mday,
						 stm->tm_hour, stm->tm_min, stm->tm_sec);
			}
			b->state = state;
		}
		draw_blist();
		refresh();
	} else {
		b = malloc(sizeof (struct buddy));
		b->name = strdup(nospaces(name));
		b->state = state;
		notfound = list_append(notfound, b);
	}
}

static list *wrap(char *text, int cols)
{
	list *l = NULL;
	char *m = text;

	if (!m || !*m) return list_new(calloc(1, 1));

	while (strlen(m) > cols) {
		char *x = m + cols;
		while (isspace(*x) && x >= m)
			x--;
		while (!isspace(*x) && x >= m)
			x--;
		x++;
		if (x == m) x = m + cols;
		l = list_prepend(l, strndup(m, x - m));
		m = x;
	}

	if (*m) l = list_prepend(l, strdup(m));

	return l;
}

static list *append_text(list *l, char *x)
{
	char *m, *t = x;

	while (*t) {
		if (*t == '\r')
			*t = ' ';
		t++;
	}

	while ((m = strchr(x, '\n'))) {
		*m = 0;
		l = list_prepend(l, strdup(x));
		*m = '\n';
		x = m + 1;
	}

	l = list_prepend(l, strdup(x));
	return l;
}

static void draw_list(list *lst, int t, int l, int b, int r, bool cursor)
{
	list *draw = NULL;

	int lines = b - t, cols = r - l;

	while (lst && lines > 0) {
		list *newlines = wrap(lst->data, cols);
		int len = list_length(newlines);
		list *n = newlines;
		while (len-- > 0 && lines-- > 0) {
			draw = list_prepend(draw, n->data);
			n = n->next;
		}
		while (len-- > 0) {
			free(n->data);
			n = n->next;
		}
		list_free(newlines);
		lst = lst->next;
	}

	while (draw) {
		char *d = draw->data;
		int i = strlen(d);
		mvaddstr(t++, l, d);
		draw = list_remove(draw, d);
		if (cursor) {
			cursor_x = BLWID + 1 + i;
			cursor_y = t - 1;
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
	mvhline(1, BLWID + 1, ACS_HLINE, COLS - BLWID - 1);
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
			attron(A_BOLD);
			mvaddch(0, pos++, '+');
			pos++;
		}
		mvaddstr(0, pos, tab->title);
		pos += strlen(tab->title);
		pos++;
		attroff(A_BOLD);
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
	if (*entry_text) {
		l = append_text(NULL, entry_text);
		draw_list(l, LINES - ENHEI + 1, BLWID + 1, LINES, COLS - 1, TRUE);
		list_free(l);
	} else {
		cursor_x = BLWID + 1;
		cursor_y = LINES - ENHEI + 1;
		move(cursor_y, cursor_x);
	}
}

static void redraw_screen()
{
	/* draw the vertical line */
	mvvline(0, BLWID, ' ' /* ACS_VLINE */, LINES);

	/* now the horizontal one separating the view from the entry */
	if (isaway) {
		int i = COLS - BLWID - 1;
		int len = 0;
		char *m = malloc(i + 1);
		m[len] = 0;
		while (len < i) {
			strncat(m, "away ", i - len);
			len += 5;
		}
		mvaddstr(LINES - ENHEI, BLWID + 1, m);
		free(m);
	} else {
		mvhline(LINES - ENHEI, BLWID + 1, ACS_HLINE, COLS - BLWID - 1);
	}
	mvaddch(LINES - ENHEI, BLWID, ACS_LTEE);

	draw_blist();

	/* draw the current tab */
	draw_tabs();

	/* draw the text */
	draw_entry();

	refresh();
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
		*replace = '?';
		*length = 6;
	} else if (!strncasecmp(string, "&quot;", 6)) {
		*replace = '\"';
		*length = 6;
	} else if (!strncasecmp(string, "&reg;", 5)) {
		*replace = '?';
		*length = 5;
	} else if (*(string + 1) == '#') {
		uint pound = 0;
		if (sscanf(string, "&#%u;", &pound) == 1) {
			int l10 = 0;
			*replace = (char)pound;
			while (pound /= 10) l10++;
			if (*(string + 3 + l10) != ';')
				return 0;
			*length = 4 + l10;
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
		if (*x == 0x14) {
			y[pos++] = '?';
			x++;
		} else if (*x == '<' && (len = is_tag(x + 1))) {
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

static char *myctime(void)
{
	static char retbuf[64];
	struct tm *lt;
	struct timeval tv;

	gettimeofday(&tv, NULL);
	lt = localtime((time_t *)&tv.tv_sec);
	strftime(retbuf, 64, "%a %b %e %H:%M:%S %Z %Y", lt);

	return retbuf;
}

static void log_msg(char *from, char *to, char *msg)
{
	char path[8192];
	FILE *f;

	sprintf(path, "%s/log", mydir());
	if (!(f = fopen(path, "a"))) {
		fprintf(stderr, "Can't write %s\n", path);
		return;
	}
	fprintf(f, "%s | %s | %s | %s\n", myctime(), from, to, msg);
	fclose(f);
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

	t->text = append_text(t->text, x);
	log_msg(from, si.displayname, h);
	free(x);

	if (list_nth(tabs, cur_tab) != t)
		t->unseen = 1;
	draw_tabs();
	refresh();

	if (sound) beep();
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
	t->text = append_text(t->text, m);
	if (cur_tab)
		t->unseen = 1;
	draw_tabs();
	refresh();
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
		getinfo(x + 5);
	} else if (!strncasecmp(x, "keepalive ", 10) && x[10]) {
		strcpy(keepalive_user, x + 10);
	} else if (!strncasecmp(x, "stalk ", 6) && x[6]) {
		list *l = pals;
		while (l) {
			struct group *g = l->data;
			list *m = g->members;
			while (m) {
				struct buddy *b = m->data;
				if (!strcasecmp(b->name, nospaces(x + 6))) {
					b->stalk = !b->stalk;
					dvprintf("%sstalking %s", b->stalk ? "" : "not ", b->name);
					return;
				}
				m = m->next;
			}
			l = l->next;
		}
		dvprintf("stalk who?");
	} else if (!strcasecmp(x, "sound off")) {
		sound = 0;
	} else if (!strcasecmp(x, "sound on")) {
		sound = 1;
	} else if (!strcasecmp(x, "sound")) {
		dvprintf("sound is %s", sound ? "on" : "off");
	} else if (!strncasecmp(x, "search ", 7) && x[7]) {
		usersearch(x + 7);
	} else if (!strncasecmp(x, "avail ", 6) && x[6]) {
		presence(x + 6, 1);
	} else if (!strncasecmp(x, "unavail ", 8) && x[8]) {
		presence(x + 8, 0);
	} else if (!strcasecmp(x, "debug")) {
		print_anyway = !print_anyway;
	} else if (!strncasecmp(x, "priority ", 9 && x[9])) {
		priority(atoi(x + 9));
	} else if (!strncasecmp(x, "away ", 5) && x[5]) {
		free(awaymsg);
		awaymsg = strdup(&x[5]);
		if (isaway) {
			away(awaymsg);
		}
	} else if (!strcasecmp(x, "help")) {
		dvprintf("No help for you! Read the source! NEXT!");
	}
}

static char *fix_amp(char *str)
{
	static char *rv = NULL;
	int i, j;

	if (strchr(str, '&') == NULL)
		return str;

	rv = realloc(rv, strlen(str) * 5 + 1);
	for (i = 0, j = 0; str[i] != 0; i++) {
		if (str[i] != '&') {
			rv[j++] = str[i];
			continue;
		}
		rv[j++] = '&';
		rv[j++] = 'a';
		rv[j++] = 'm';
		rv[j++] = 'p';
		rv[j++] = ';';
	}

	rv[j] = '\0';
	return rv;
}

static void send_message()
{
	struct tab *t;

	time_t tm;
	struct tm *stm;

	char *h, *x;

	if (!*entry_text)
		return;

	t = list_nth(tabs, cur_tab);

	tm = time(NULL);
	stm = localtime(&tm);

	h = strip_html(entry_text);
	x = malloc(strlen(si.displayname) + strlen(h) + 100);

	if (!strncasecmp(h, "/me ", 4))
		sprintf(x, "%02d:%02d:%02d *** %s %s", stm->tm_hour, stm->tm_min, stm->tm_sec,
				si.displayname, h + 4);
	else
		sprintf(x, "%02d:%02d:%02d %s: %s", stm->tm_hour, stm->tm_min, stm->tm_sec,
				si.displayname, h);

	t->text = append_text(t->text, x);
	log_msg(si.displayname, t->title, h);

	send_im(t->title, fix_amp(entry_text));

	draw_tabs();
	refresh();

	free(x);
}

static void move_tab(int forward)
{
	struct tab *t;

	if (!cur_tab)
		return;

	t = list_nth(tabs, cur_tab);
	tabs = list_remove(tabs, t);
	if (forward) {
		if (list_length(tabs) == cur_tab)
			cur_tab = 1;
		else
			cur_tab++;
	} else {
		if (cur_tab == 1)
			cur_tab = list_length(tabs);
		else
			cur_tab--;
	}
	tabs = list_insert(tabs, t, cur_tab - 1);
	draw_tabs();
	refresh();
}

static int stdin_ready(void *nbv, int event, nbio_fd_t *fdt)
{
	struct tab *t;
	int l = strlen(entry_text);
	int c = getch();
	switch (c) {
	case 2:		/* ^B */
		if (BLWID < 0)
			BLWID = max_blwid;
		else
			BLWID = -1;
		redraw_screen();
		break;
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
			t = list_nth(tabs, cur_tab);
			t->unseen = 0;
			draw_tabs();
			refresh();
		}
		break;
	case 4:		/* ^D */
		end_window();
		exit(0);
		break;
	case 7:		/* ^G */
		if (isaway) {
			away(NULL);
			dvprintf("you are back!");
			isaway = 0;
		} else {
			away(awaymsg);
			dvprintf("you are away: %s", awaymsg);
			isaway = 1;
		}
		redraw_screen();
		break;
	case 8:		/* ^H */
	case 127:	/* backspace */
	case 263:	/* backspace */
		if (*entry_text) {
			entry_text[l - 1] = 0;
			draw_entry();
			refresh();
		}
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
		clear();
		redraw_screen();
		break;
	case 10:	/* ^J */
	case 13:	/* ^M, enter */
		if (newlines) {
			if (cur_tab)
				send_message();
			else
				process_command();
			*entry_text = 0;
		} else {
			entry_text = realloc(entry_text, l + 2);
			entry_text[l] = '\n';
			entry_text[l + 1] = 0;
		}
		draw_entry();
		refresh();
		break;
	case 14:	/* ^N */
		newlines = !newlines;
		break;
	case 18:	/* ^R */
		t = list_nth(tabs, cur_tab);
		while (t->text) {
			char *s = t->text->data;
			t->text = list_remove(t->text, s);
			free(s);
		}
		draw_tabs();
		refresh();
		break;
	case 19:	/* ^S */
		sound = !sound;
		break;
	case 20:	/* ^T */
		if (l > 1) {
			c = entry_text[l - 1];
			entry_text[l - 1] = entry_text[l - 2];
			entry_text[l - 2] = c;
			draw_entry();
			refresh();
		}
		break;
	case 21:	/* ^U */
		*entry_text = 0;
		draw_entry();
		refresh();
		break;
	case 23:	/* ^W */
	case 255:	/* M-Backspace */
		c = l - 1;
		while (c > 0 && isspace(entry_text[c])) c--;
		while (c + 1 && !isspace(entry_text[c])) c--;
		c++;
		entry_text[c] = 0;
		draw_entry();
		refresh();
		break;
	case 27:	/* ESC */
		c = getch();
		switch (c) {
		case 'n':
			move_tab(1);
			break;
		case 'p':
			move_tab(0);
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			if (c - '0' < list_length(tabs)) {
				cur_tab = c - '0';
				t = list_nth(tabs, cur_tab);
				t->unseen = 0;
				draw_tabs();
				refresh();
			}
			break;
		}
		break;
	case 176:	/* M-0 */
	case 177:	/* M-1 */
	case 178:	/* M-2 */
	case 179:	/* M-3 */
	case 180:	/* M-4 */
	case 181:	/* M-5 */
	case 182:	/* M-6 */
	case 183:	/* M-7 */
	case 184:	/* M-8 */
	case 185:	/* M-9 */
		if (c - 176 < list_length(tabs)) {
			cur_tab = c - 176;
			t = list_nth(tabs, cur_tab);
			t->unseen = 0;
			draw_tabs();
			refresh();
		}
		break;

#define ADD_CHAR(e)	do { \
				entry_text = realloc(entry_text, l + 2); \
				entry_text[l] = e; \
				entry_text[l + 1] = 0; \
				draw_entry(); \
				refresh(); \
			} while (0)

	case 195:
		c = getch();
		c += 64;
		ADD_CHAR(c);
		break;
	case 238:	/* M-N */
		move_tab(1);
		break;
	case 240:	/* M-P */
		move_tab(0);
		break;
	case 432:
	case 433:
	case 434:
	case 435:
	case 436:
	case 437:
	case 438:
	case 439:
	case 440:
	case 441:
		if (c - 432 < list_length(tabs)) {
			cur_tab = c - 432;
			t = list_nth(tabs, cur_tab);
			t->unseen = 0;
			draw_tabs();
			refresh();
		}
		break;
	default:
		if (isprint(c)) {
			ADD_CHAR(c);
		} else if (print_anyway) {
			char x[256];
			snprintf(x, 256, "\\x%02x", c);
			entry_text = realloc(entry_text, l + strlen(x) + 1);
			strcat(entry_text, x);
			draw_entry();
			refresh();
		}
		break;
	}
	refresh();
	return 0;
}

static int watch_fd(int fd, nbio_handler_t handler)
{
	nbio_fd_t *fdt;

	if (!(fdt = nbio_addfd(&gnb, NBIO_FDTYPE_STREAM, fd, 0, handler, NULL, 0, 0))) {
		fprintf(stderr, "Couldn't read stdin\n");
		return 1;
	}
	if (nbio_setraw(&gnb, fdt, 2)) {
		fprintf(stderr, "Couldn't read raw stdin\n");
		return 1;
	}

	return 0;
}

static int sigwinch_redraw(void *nbv, int event, nbio_fd_t *fdt)
{
	char c;
	read(sp[0], &c, 1);
	endwin();
	initscr();
	clear();
	redraw_screen();
	return 0;
}

static void sigwinch(int sig)
{
	char c = 0;
	write(sp[1], &c, 1);
	endwin();
	initscr();
	clear();
	redraw_screen();
}

int init_window()
{
	struct tab *tab;
	int fl;

	awaymsg = strdup("I am away from my computer.");

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

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp))
		return 1;
	if (watch_fd(sp[0], sigwinch_redraw))
		return 1;
	if (watch_fd(0, stdin_ready))
		return 1;

	/* if we set stdin to nonblock then we need to set stdout to block */
	fl = fcntl(1, F_GETFL);
	fl ^= O_NONBLOCK;
	fcntl(1, F_SETFL, fl);

	signal(SIGWINCH, sigwinch);

	return 0;
}

void end_window()
{
	endwin();
}
