/* Copyright 2015 Christian Heckendorf.  All rights reserved.
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file.
 */

#ifndef NB_URLPARSE_H
#define NB_URLPARSE_H

#include "xmlproc.h"
#include "sql/cache.h"

struct urlitem{
	char *url;
	union{
		xmlDocPtr doc;
	}info;
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
