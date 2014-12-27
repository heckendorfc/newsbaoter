#include <stdlib.h>
#include <stdio.h>

#include "urlparse.h"
#include "httpfetch.h"
#include "ui/view.h"

int main(int argc, char **argv){
	struct urllist *ul;
	struct mainwindow *mw;
	char hs[]="newsbaoter v0.1 | Feed List";
	char fs[]="press q to exit";

	//ul=urlparse();
	//fetch_urls(ul,5);

	mw=setup_ui();
	chars_to_widechars(mw->header,hs,mw->width);
	chars_to_widechars(mw->footer,fs,mw->width);
	update_view(mw);
	run_ui();
	end_ui();

	return 0;
}
