#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "urlparse.h"
#include "httpfetch.h"
#include "ui/view.h"
#include "ui/ui_common.h"

struct io_data{
	struct urllist *ul;
	struct mainwindow *mw;
};

static void handle_ipc(struct io_data *iod, ipcinfo ii){
	if(ii==IPCVAL_UPDATE_REQUEST){
		xmlproc_gen_lines(iod->ul,iod->mw);
	}
	else if(ii==IPCVAL_WRITE_ENTRY){
		int fd;
		int id;
		read(iod->mw->outfd[0],&id,sizeof(id));
		read(iod->mw->outfd[0],&fd,sizeof(fd));
		xmlproc_write_entry(iod->ul,iod->mw,id,fd);
		close(fd);
	}
	ii=IPCVAL_DONE;
	write(iod->mw->infd[1],&ii,sizeof(ii));
}

void* iothread(void *data){
	struct io_data *iod=(struct io_data*)data;
	struct urllist *ul;
	ipcinfo ii;
	struct timeval wait = { 0, 0 };
	fd_set fdread;
	int sret;
	int wait_time=30*60;

	ul=urlparse();
	iod->ul=ul;
	while(1){
		fetch_urls(ul,5);

		wait.tv_sec=wait_time;
		wait.tv_usec=0;

		xmlproc_gen_lines(ul,iod->mw);
		update_view(iod->mw);

		do{
			FD_ZERO(&fdread);
			FD_SET(iod->mw->outfd[0],&fdread);
			sret=select(iod->mw->outfd[0]+1,&fdread,NULL,NULL,&wait);
			if(sret>0){
				if(FD_ISSET(iod->mw->outfd[0],&fdread)){
					int rr=read(iod->mw->outfd[0],&ii,sizeof(ii));
					if(rr>=0){
						handle_ipc(iod,ii);
					}
				}
			}
		}while(sret>0 && wait.tv_sec>0);
		break; /* TODO: XXX: DELETE ME */
	}

	return (void*)0;
}

int main(int argc, char **argv){
	struct mainwindow *mw;
	pthread_t ioth;
	struct io_data iod;
	char hs[]="Feed List";
	char fs[]="press q to exit";

	mw=setup_ui();
	iod.mw=mw;
	pthread_create(&ioth,NULL,iothread,&iod);
	chars_to_widechars(mw->header,hs,mw->width);
	chars_to_widechars(mw->footer,fs,mw->width);
	update_view(mw);
	run_ui(mw);
	end_ui();

	return 0;
}
