#ifndef NBHTTPFETCH_H
#define NBHTTPFETCH_H

#include "urlparse.h"

void* http_init();
void http_destroy(void *d);
int http_get_fds(fd_set *fdr, fd_set *fdw, fd_set *fde, void *d);
struct timeval* http_get_timeout(void *d);
int handle_urls(struct urllist *ul, void *d, void *db);
void http_fetch_init(struct urllist *ul, void *d);
void fetch_urls(struct urllist *ul, const int npara, void *db);

#endif
