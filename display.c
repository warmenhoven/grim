#define _GNU_SOURCE
#include "main.h"
#include <curses.h>
#include <ctype.h>
#include <string.h>

#define BLWID 20
#define ENHEI 8

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

static unsigned int cursor_pos = 0;
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

static void draw_blist()
{
	/* XXX */
}

static list *wrap(char *text, int cols)
{
	list *l = NULL;
	char *m = text;

	if (!m) return l;

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
	int i = cursor_pos;

	int lines = b - t, cols = r - l;

	list *last = lst;
	if (last) while (last->next) last = last->next;

	while (last && lines) {
		list *newlines = wrap(last->data, cols);
		int len = list_length(newlines);
		list *n = newlines;
		while (len-- && lines--) {
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

static void draw_tab()
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
		mvaddch(1, pos, ACS_BTEE);
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
	draw_tab();

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

static int stdin_ready(void *nbv, int event, nbio_fd_t *fdt)
{
	int c = getch();
	switch (c) {
	case 1:		/* ^A */
		cursor_pos = 0;
		//move(LINES - ENHEI + 1, BLWID + 1);
		draw_entry();
		refresh();
		break;
	case 3:		/* ^C */
		if (cur_tab) {
			list *l = tabs;
			struct tab *t;
			int i = 0;
			while (i++ < cur_tab)
				l = l->next;
			t = l->data;
			tabs = list_remove(tabs, t);
			free(t->title);
			list_free(t->text);
			free(t);
		}
		break;
	case 4:		/* ^D */
		end_window();
		exit(0);
		break;
	case 5:		/* ^E */
		cursor_pos = strlen(entry_text);
		draw_entry();
		refresh();
		break;
	case 9:		/* ^I, tab */
		break;
	case 11:	/* ^K */
		entry_text[cursor_pos] = 0;
		draw_entry();
		refresh();
		break;
	case 12:	/* ^L */
		redraw_screen();
		break;
	case 13:	/* ^M, enter */
		break;
	case 21:	/* ^U */
		memmove(entry_text, entry_text + cursor_pos, strlen(entry_text) - cursor_pos + 1);
		cursor_pos = 0;
		draw_entry();
		refresh();
		break;
	case 23:	/* ^W */
		break;
	case 127:	/* backspace */
		if (*entry_text) {
			entry_text[strlen(entry_text)-1] = 0;
			cursor_pos--;
			draw_entry();
			refresh();
		}
		break;
	default:
		if (isprint(c)) {
			int l = strlen(entry_text);
			entry_text = realloc(entry_text, l + 2);
			if (cursor_pos == l) {
				entry_text[l] = c;
				entry_text[l + 1] = 0;
				cursor_pos++;
			} else {
				char *p = entry_text + cursor_pos;
				memmove(p + 1, p, l - cursor_pos + 1);
				entry_text[cursor_pos] = c;
				cursor_pos++;
			}
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

	va_start(ap, f);
	vsprintf(s, f, ap);
	va_end(ap);

	t->text = list_append(t->text, strdup(s));
	if (cur_tab)
		t->unseen = 1;
	draw_tab();
	refresh();
}
