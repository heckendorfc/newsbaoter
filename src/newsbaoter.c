#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "urlparse.h"
#include "httpfetch.h"
#include "ui/view.h"

struct io_data{
	struct urllist *ul;
	struct mainwindow *mw;
};

void* iothread(void *data){
	struct io_data *iod=(struct io_data*)data;
	struct urllist *ul;

	ul=urlparse();
	iod->ul=ul;
	while(1){
		fetch_urls(ul,5);

		xmlproc_gen_lines(ul,iod->mw);
		update_view(iod->mw);

		sleep(60*30);
		break;
	}

	return (void*)0;
}

int main(int argc, char **argv){
	struct mainwindow *mw;
	pthread_t ioth;
	struct io_data iod;
	char hs[]="newsbaoter v0.1 | Feed List";
	char fs[]="press q to exit";

	mw=setup_ui();
	iod.mw=mw;
	pthread_create(&ioth,NULL,iothread,&iod);
	chars_to_widechars(mw->header,hs,mw->width);
	chars_to_widechars(mw->footer,fs,mw->width);
	update_view(mw);
	run_ui(&iod);
	end_ui();

	return 0;
}
