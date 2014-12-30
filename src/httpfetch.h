#ifndef NBHTTPFETCH_H
#define NBHTTPFETCH_H

#include "urlparse.h"

void http_init();
void fetch_urls(struct urllist *ul, const int npara, void *db);

#endif
