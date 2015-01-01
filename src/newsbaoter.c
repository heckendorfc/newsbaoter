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
#include "ui/keys.h"
#include "sql/cache.h"

/*
struct io_data{
	struct urllist *ul;
	struct mainwindow *mw;
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
	else if(ii==IPCVAL_READ_FEED){
		int id;
		read(iod->mw->outfd[0],&id,sizeof(id));
		cache_read_feed(iod->db,id);
	}
	else if(ii==IPCVAL_TOGGLE_READ){
		int id;
		read(iod->mw->outfd[0],&id,sizeof(id));
		cache_toggle_read_entry(iod->db,id,-1);
	}
	else if(ii==IPCVAL_NEXT_UNREAD){
		int id;
		read(iod->mw->outfd[0],&id,sizeof(id));
		id=cache_next_unread(iod->mw,id,iod->db);
		write(iod->mw->infd[1],&id,sizeof(id));
	}
	else if(ii==IPCVAL_FETCH_DONE){
			iod->num_ur[1]=get_num_unread(iod->db);
			cache_cleanup_old(iod->db);

			if(iod->num_ur[0]<iod->num_ur[1])
				iod->mw->beep_request=1;

			pthread_mutex_lock(&iod->mw->viewtex);

			cache_gen_lines(iod->ul,iod->mw,iod->db);
			//update_view(iod->mw);
			ii=IPCVAL_UPDATE_REQUEST;
			write(iod->mw->sidefd[1],&ii,sizeof(ii));

			pthread_mutex_unlock(&iod->mw->viewtex);
	}
	ii=ret;
	write(iod->mw->infd[1],&ii,sizeof(ii));

	return 0;
}

void* netiothread(void *data){
	struct io_data *iod=(struct io_data*)data;
	fd_set fdread;
	int sret;
	ipcinfo ii;
	int parallel=5;

	if(global_config.parallel_reload>0)
		parallel=global_config.parallel_reload;

	while(1){
		do{
			FD_ZERO(&fdread);
			FD_SET(iod->pipefd[0],&fdread);
			sret=select(iod->pipefd[0]+1,&fdread,NULL,NULL,NULL);
			if(sret>0){
				if(FD_ISSET(iod->pipefd[0],&fdread)){
					int rr=read(iod->pipefd[0],&ii,sizeof(ii));
					if(rr>=0){
						if(ii==IPCVAL_REFRESH_ALL){
							// FIXME: can sqlite handle this properly from a different thread?
							fetch_urls(iod->ul,parallel,iod->db);
							ii=IPCVAL_FETCH_DONE;
							write(iod->mw->outfd[1],&ii,sizeof(ii));
						}
					}
				}
			}
			else if(sret==-1 && errno==EINTR){
				sret=1;
				continue;
			}
		}while(sret>0);
		if(sret<0){
			fprintf(stderr,"Error in I/O select\n");
			exit(3);
		}
	}
	return (void*)0;
}

void* dbiothread(void *data){
	struct io_data *iod=(struct io_data*)data;
	struct urllist *ul;
	sqlite3 *db;
	ipcinfo ii;
	struct timeval wait = { 0, 0 };
	fd_set fdread;
	int sret;
	int iret=0;
	int wait_time=30*60;

	if(global_config.reload_time>0)
		wait_time=global_config.reload_time*60;

	db=init_db();
	iod->db=db;
	ul=urlparse();
	iod->ul=ul;
	http_init();

	pthread_mutex_lock(&iod->mw->viewtex);

	cache_gen_lines(iod->ul,iod->mw,iod->db);
	//update_view(iod->mw);
	ii=IPCVAL_UPDATE_REQUEST;
	write(iod->mw->sidefd[1],&ii,sizeof(ii));

	pthread_mutex_unlock(&iod->mw->viewtex);

	while(1){
		if(iret || global_config.auto_reload){
			long num[2];

			cache_init_cleanup(db);
			num[0]=get_num_unread(db);
			ii=IPCVAL_REFRESH_ALL;
			write(iod->pipefd[1],&ii,sizeof(ii));
			//notify_refresh_footer(iod->mw);
			//fetch_urls(ul,parallel,db);
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
*/

int main(int argc, char **argv){
	struct mainwindow *mw;
	//struct io_data iod;
	char hs[]="Feed List";
	const int fsl=50;
	char fs[fsl];

	mw=setup_ui();
	//iod.mw=mw;
	read_config_options_file();
	set_foot_help_string(fs,fsl);
	chars_to_widechars(mw->header,hs,mw->width);
	chars_to_widechars(mw->footer,fs,mw->width);

	mw->db=init_db();
	mw->ul=urlparse();
	mw->httpdata=http_init();
	cache_gen_lines(mw,mw->db);

	run_ui(mw);
	end_ui();

	return 0;
}
