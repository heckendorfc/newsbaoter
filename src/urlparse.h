#ifndef NB_URLPARSE_H
#define NB_URLPARSE_H

struct urlitem{
	char *url;
};

struct urllist{
	struct urllist *next;
	struct urlitem data;
};
/*
struct tagitem{
	struct tagitem *next;
	struct urlitem *data;
};
*/
struct taglist{
	struct taglist *next;
	struct urlitem *data;
};

struct urllist* urlparse();

#endif
