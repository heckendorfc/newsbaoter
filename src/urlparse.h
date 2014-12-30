#ifndef NB_URLPARSE_H
#define NB_URLPARSE_H

#include "xmlproc.h"

struct urlitem{
	char *url;
	xmlDocPtr doc;
	int n_httperr;
};

struct urllist{
	struct urllist *next;
	struct urlitem data;
};

struct taglist{
	struct taglist *next;
	struct urlitem *data;
};

struct urllist* urlparse();

#endif
