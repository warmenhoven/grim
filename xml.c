#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <string.h>
#include "list.h"
#include "xml.h"

typedef struct _xmlnode {
	struct _xmlnode *parent;
	list *children;
	list *attribs;
	char *name;
	char *data;
} xmlnode;

void *xml_new(const char *el)
{
	xmlnode *node = calloc(1, sizeof (xmlnode));
	if (!node)
		return NULL;
	node->name = strdup(el);
	return node;
}

void *xml_child(void *p, const char *el)
{
	xmlnode *parent = p, *node;

	if (!parent)
		return NULL;

	node = calloc(1, sizeof (xmlnode));
	if (!node)
		return NULL;
	node->name = strdup(el);
	node->parent = parent;

	parent->children = list_append(parent->children, node);

	return node;
}

void xml_attrib(void *n, const char *attr, const char *val)
{
	xmlnode *node = n, *attrib;

	if (!node)
		return;

	attrib = calloc(1, sizeof (xmlnode));
	if (!attrib)
		return;
	attrib->name = strdup(attr);
	attrib->data = strdup(val);

	node->attribs = list_append(node->attribs, attrib);
}

void xml_data(void *n, const char *data, int len)
{
	xmlnode *node = n;
	if (!node)
		return;

	node->data = strndup(data, len);
}

void *xml_parent(void *child)
{
	xmlnode *node = child;
	if (!node)
		return NULL;

	return node->parent;
}

char *xml_name(void *n)
{
	xmlnode *node = n;
	if (!node)
		return NULL;

	return node->name;
}

void *xml_get_child(void *n, const char *name)
{
	xmlnode *node = n;
	list *l;

	if (!n || !name)
		return NULL;

	l = node->children;
	while (l) {
		xmlnode *child = l->data;
		if (!strcasecmp(child->name, name))
			return child;
		l = l->next;
	}
	return NULL;
}

list *xml_get_children(void *p)
{
	xmlnode *parent = p;
	if (!parent)
		return NULL;
	return parent->children;
}

char *xml_get_attrib(void *n, const char *name)
{
	xmlnode *node = n;
	list *l;

	if (!n || !name)
		return NULL;

	l = node->attribs;
	while (l) {
		xmlnode *attrib = l->data;
		if (!strcasecmp(attrib->name, name))
			return attrib->data;
		l = l->next;
	}
	return NULL;
}

char *xml_get_data(void *n)
{
	if (!n) return NULL;
	return ((xmlnode *)n)->data;
}

void xml_free(void *n)
{
	xmlnode *node = n;
	if (!node)
		return;

	while (node->attribs) {
		xmlnode *attrib = node->attribs->data;
		node->attribs = list_remove(node->attribs, attrib);
		xml_free(attrib);
	}

	while (node->children) {
		xmlnode *child = node->children->data;
		node->children = list_remove(node->children, child);
		xml_free(child);
	}

	if (node->name)
		free(node->name);
	if (node->data)
		free(node->data);

	free(node);
}
