#include <stdlib.h>
#include <ncurses.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>

#include "util.h"
#include "../common_defs.h"

void wait_for_pager(struct mainwindow *mw, int pid){
	int status;
	pid_t ret;

	do{
		ret=waitpid(pid,&status,0);
		if(ret==-1 && errno!=EINTR)
			break;
	}while(!(ret==pid && (WIFEXITED(status) || WIFSIGNALED(status))));

	pthread_mutex_unlock(&mw->viewtex);
}

int pipe_to_pager(struct mainwindow *mw, int *pid, int *fd){
	int pipefd[2];

	pthread_mutex_lock(&mw->viewtex);

	pipe(pipefd);

	*pid=fork();

	if(*pid==0){ /* child */
		char *pager="/usr/bin/less"; /* TODO: find path via cmake? */
		char *args[2]={pager,(char*)0};
		dup2(pipefd[0],0);
		close(pipefd[0]);
		close(pipefd[1]);
		if(execvp(args[0],args))
			fprintf(stderr,"Error spawning %s\n",args[0]);
		exit(1);
	}
	else if(*pid>0){ /* parent */
		close(pipefd[0]); /* we don't need the read end here */

		*fd=pipefd[1];
	}
	else{ /* error */
		return 1;
	}
	return 0;
}

int pipe_entry(struct mainwindow *mw, int id){
	int pid,fd;
	ipcinfo ii=IPCVAL_WRITE_ENTRY;
	def_prog_mode();
	endwin();

	write(mw->outfd[1],&ii,sizeof(ii));

	//int fd=2;
	//write(mw->outfd[1],&fd,sizeof(fd));
	pipe_to_pager(mw,&pid,&fd);
	write(mw->outfd[1],&id,sizeof(id));
	write(mw->outfd[1],&fd,sizeof(fd));
	wait_for_pager(mw,pid);

	read(mw->infd[0],&ii,sizeof(ii));
	if(ii!=IPCVAL_DONE){
		/* TODO: error? */
	}

	reset_prog_mode();

	return request_list_update(mw);
}

int request_list_update(struct mainwindow *mw){
	ipcinfo ii=IPCVAL_UPDATE_REQUEST;
	write(mw->outfd[1],&ii,sizeof(ii));
	read(mw->infd[0],&ii,sizeof(ii));
	if(ii!=IPCVAL_DONE){
		/* TODO: error? */
		return 1;
	}
	return 0;
}

int catchup_entries(struct mainwindow *mw, int id){
	ipcinfo ii=IPCVAL_READ_FEED;
	write(mw->outfd[1],&ii,sizeof(ii));
	write(mw->outfd[1],&id,sizeof(id));
	read(mw->infd[0],&ii,sizeof(ii));
	if(ii!=IPCVAL_DONE){
		/* TODO: error? */
		return 1;
	}
	return request_list_update(mw);
}

int toggle_read_ipc(struct mainwindow *mw, int id){
	ipcinfo ii=IPCVAL_TOGGLE_READ;
	write(mw->outfd[1],&ii,sizeof(ii));
	write(mw->outfd[1],&id,sizeof(id));
	read(mw->infd[0],&ii,sizeof(ii));
	if(ii!=IPCVAL_DONE){
		/* TODO: error? */
		return 1;
	}
	return request_list_update(mw);
}

int next_unread_ipc(struct mainwindow *mw, int id){
	ipcinfo ii=IPCVAL_NEXT_UNREAD;
	write(mw->outfd[1],&ii,sizeof(ii));
	write(mw->outfd[1],&id,sizeof(id));
	read(mw->infd[0],&id,sizeof(id));
	read(mw->infd[0],&ii,sizeof(ii));
	if(ii!=IPCVAL_DONE){
		/* TODO: error? */
		return 1;
	}
	return request_list_update(mw);
}
