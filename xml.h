#include "list.h"

void *xml_new(const char *);
void *xml_child(void *, const char *);
void xml_attrib(void *, const char *, const char *);
void xml_data(void *, const char *, int);

void *xml_parent(void *);
char *xml_name(void *);
void *xml_get_child(void *, const char *);
list *xml_get_children(void *);
char *xml_get_attrib(void *, const char *);
char *xml_get_data(void *);

void xml_free(void *);
