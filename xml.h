void *xml_new(const char *);
void *xml_child(void *, const char *);
void xml_attrib(void *, const char *, const char *);
void xml_data(void *, const char *, int);

void *xml_parent(void *);
const char *xml_name(void *);
