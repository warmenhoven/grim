#include <stdlib.h>
#include "xml.h"

void *xml_new(const char *el)
{
	return NULL;
}

void *xml_child(void *parent, const char *el)
{
	return NULL;
}

void xml_attrib(void *node, const char *attr, const char *val)
{
}

void xml_data(void *node, const char *data, int len)
{
}

void *xml_parent(void *child)
{
	return NULL;
}

const char *xml_name(void *node)
{
	return NULL;
}
