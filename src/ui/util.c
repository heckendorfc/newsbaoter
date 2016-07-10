/* Copyright 2015 Christian Heckendorf.  All rights reserved.
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file.
 */

#include <stdlib.h>
#include <ncurses.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>

#include "view.h"
#include "util.h"
#include "../common_defs.h"
#include "../config.h"
#include "../sql/cache.h"

void wait_for_process(struct mainwindow *mw, int pid){
	int status;
	pid_t ret;

	do{
		ret=waitpid(pid,&status,0);
		if(ret==-1 && errno!=EINTR)
			break;
	}while(!(ret==pid && (WIFEXITED(status) || WIFSIGNALED(status))));
}

int pipe_to_process(struct mainwindow *mw, int *pid, int *fd, char **args){
	int pipefd[2];

	pipe(pipefd);

	*pid=fork();

	if(*pid==0){ /* child */
		dup2(pipefd[0],0);
		close(pipefd[0]);
		close(pipefd[1]);
		if(execvp(args[0],args))
			debug_print("Error spawning %s\n",args[0]);
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

int pipe_to_pager(struct mainwindow *mw, int *pid, int *fd){
	return pipe_to_process(mw,pid,fd,global_config.pager);
}

int pipe_fields(struct mainwindow *mw, int id, char **args, char *fields){
	int pid,fd;

	def_prog_mode();
	endwin();

	pipe_to_process(mw,&pid,&fd,args);
	cache_write_entry_field(mw,id,fd,mw->db,fields);
	close(fd);
	wait_for_process(mw,pid);

	reset_prog_mode();

	return request_list_update(mw);
}

int download_entry(struct mainwindow *mw, int id){
	return pipe_fields(mw,id,global_config.downloader,"URL");
}

int pipe_entry(struct mainwindow *mw, int id){
	return pipe_fields(mw,id,global_config.pager,"*");
}

int request_list_update(struct mainwindow *mw){
	return cache_gen_lines(mw,mw->db);
}

int catchup_entries(struct mainwindow *mw, int id){
	cache_read_feed(mw->db,id);
	return request_list_update(mw);
}

int toggle_read_ipc(struct mainwindow *mw, int id){
	cache_toggle_read_entry(mw->db,id,-1);
	return request_list_update(mw);
}

int next_unread_ipc(struct mainwindow *mw, int id){
	id=cache_next_unread(mw,id,mw->db);
	return request_list_update(mw);
}
