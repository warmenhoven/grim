#include <stdlib.h>
#include "list.h"

list *list_new(void *data)
{
	list *l = malloc(sizeof(list));
	l->prev = NULL;
	l->next = NULL;
	l->data = data;
	return l;
}

unsigned int list_length(list *l)
{
	unsigned int c = 0;

	while (l) {
		l = l->next;
		c++;
	}

	return c;
}

void *list_nth(list *l, int n)
{
	while (l && n) {
		l = l->next;
		n--;
	}
	if (l) return l->data;
	return NULL;
}

list *list_append(list *l, void *data)
{
	list *s = l;

	if (!s) return list_new(data);

	while (s->next) s = s->next;
	s->next = list_new(data);
	s->next->prev = s;

	return l;
}

list *list_prepend(list *l, void *data)
{
	list *s = list_new(data);
	s->next = l;
	if (l)
		l->prev = s;
	return s;
}

list *list_remove(list *l, void *data)
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

void list_free(list *l)
{
	while (l) {
		list *s = l;
		l = l->next;
		free(s);
	}
}
