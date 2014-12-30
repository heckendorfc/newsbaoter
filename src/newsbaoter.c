#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sqlite3.h>

#include "urlparse.h"
#include "httpfetch.h"
#include "config.h"
#include "ui/view.h"
#include "ui/ui_common.h"
#include "sql/cache.h"

struct io_data{
	struct urllist *ul;
	struct mainwindow *mw;
	sqlite3 *db;
};

static int handle_ipc(struct io_data *iod, ipcinfo ii){
	int ret=IPCVAL_DONE;
	if(ii==IPCVAL_UPDATE_REQUEST){
		if(cache_gen_lines(iod->ul,iod->mw,iod->db))
			ret=IPCVAL_EOL;
	}
	else if(ii==IPCVAL_WRITE_ENTRY){
		int fd;
		int id;
		read(iod->mw->outfd[0],&id,sizeof(id));
		read(iod->mw->outfd[0],&fd,sizeof(fd));
		cache_write_entry(iod->ul,iod->mw,id,fd,iod->db);
		close(fd);
	}
	else if(ii==IPCVAL_REFRESH_ALL){
		return 1;
	}
	ii=ret;
	write(iod->mw->infd[1],&ii,sizeof(ii));

	return 0;
}

void* iothread(void *data){
	struct io_data *iod=(struct io_data*)data;
	struct urllist *ul;
	sqlite3 *db;
	ipcinfo ii;
	struct timeval wait = { 0, 0 };
	fd_set fdread;
	int sret;
	int iret=0;
	int wait_time=30*60;
	int parallel=5;

	if(global_config.reload_time>0)
		wait_time=global_config.reload_time*60;

	if(global_config.parallel_reload>0)
		parallel=global_config.parallel_reload;

	db=init_db();
	iod->db=db;
	ul=urlparse();
	iod->ul=ul;
	http_init();

	while(1){
		if(iret || global_config.auto_reload){
			fetch_urls(ul,parallel,db);

			cache_gen_lines(ul,iod->mw,db);
			update_view(iod->mw);
			iret=0;
		}

		wait.tv_sec=wait_time;
		wait.tv_usec=0;

		do{
			FD_ZERO(&fdread);
			FD_SET(iod->mw->outfd[0],&fdread);
			sret=select(iod->mw->outfd[0]+1,&fdread,NULL,NULL,&wait);
			if(sret>0){
				if(FD_ISSET(iod->mw->outfd[0],&fdread)){
					int rr=read(iod->mw->outfd[0],&ii,sizeof(ii));
					if(rr>=0){
						iret=handle_ipc(iod,ii);
					}
				}
			}
			else if(sret==-1 && errno==EINTR){
				sret=1;
				continue;
			}
		}while(sret>0 && !iret && wait.tv_sec>0);
		if(sret<0){
			fprintf(stderr,"Error in I/O select\n");
			exit(3);
		}
	}

	sqlite3_close(db);

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
	read_config_options_file();
	pthread_create(&ioth,NULL,iothread,&iod);
	chars_to_widechars(mw->header,hs,mw->width);
	chars_to_widechars(mw->footer,fs,mw->width);
	update_view(mw);
	run_ui(mw);
	end_ui();

	return 0;
}
