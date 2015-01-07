/* Copyright 2015 Christian Heckendorf.  All rights reserved.
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file.
 */

#ifndef NBCOMMON_H
#define NBCOMMON_H

#include <stdlib.h>
#include <stdio.h>

#define NB_CONF_DIR ".newsbaoter/"
#define NB_CONF_FILE NB_CONF_DIR "config"
#define NB_URL_FILE NB_CONF_DIR "urls"
#define NB_CACHE_DB NB_CONF_DIR "cache.db"
#define NB_DEBUG_FILE NB_CONF_DIR "debug"

#define ALLOC_FAIL(n) {fprintf(stderr,"Malloc of %ld failed at %s:%d\n",(size_t)(n),__FILE__,__LINE__);exit(22);}
#define INIT_MEM(x,n) {if((x=malloc((n)*sizeof(*x)))==NULL){ALLOC_FAIL(n);}}
#define REINIT_MEM(x,n) {if((x=realloc(x,(n)*sizeof(*x)))==NULL){ALLOC_FAIL(n);}}

void debug_print(const char *fmt, ...);
extern FILE *debugfd;

#endif
