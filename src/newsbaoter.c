/* Copyright 2015 Christian Heckendorf.  All rights reserved.
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sqlite3.h>

#include "urlparse.h"
#include "httpfetch.h"
#include "config.h"
#include "ui/view.h"
#include "ui/ui_common.h"
#include "ui/keys.h"
#include "sql/cache.h"
#include "common_defs.h"

FILE *debugfd=NULL;

void debug_print(const char *fmt, ...){
	va_list args;

	if(!debugfd)
		return;

	va_start(args, fmt);
	vfprintf(debugfd, fmt, args);
	va_end(args);
}

int main(int argc, char **argv){
	struct mainwindow *mw;
	char hs[]="Feed List";
	const int fsl=50;
	char fs[fsl];
	void *httpdata=http_init();
	struct urllist *ul;

	if(argc>1){
		if(strcmp(argv[1],"-d")==0){
			char *file=get_config_file(NB_DEBUG_FILE);
			if(file){
				debugfd=fopen(file,"w");
				free(file);
			}
			else
				debugfd=stderr;
		}
		else if(strcmp(argv[1],"-D")==0){
			debugfd=stderr;
		}
	}

	read_config_options_file();
	ul=urlparse();
	mw=setup_ui();
	//iod.mw=mw;
	set_foot_help_string(fs,fsl);
	chars_to_widechars(mw->header,hs,mw->width);
	chars_to_widechars(mw->footer,fs,mw->width);

	mw->db=init_db();
	mw->ul=ul;
	mw->httpdata=httpdata;
	cache_gen_lines(mw,mw->db);

	run_ui(mw);
	end_ui();

	sqlite3_close(mw->db);
	http_destroy(httpdata);

	return 0;
}
