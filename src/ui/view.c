//#include <sqlite3.h>
#include <locale.h>
#include <pthread.h>
#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "view.h"
#include "keys.h"
#include "util.h"
#include "../common_defs.h"

#ifndef NB_VERS_S
#define NB_VERS_S "A.B"
#endif

#define TITL_HEIGHT 1
#define TITL_WIDTH 18
#define TITL_STRING "newsbaoter v" NB_VERS_S " | "
#define TITL_TGAP 0
#define TITL_LGAP 0

#define HEAD_HEIGHT 1
#define HEAD_WIDTH (COLS-TITL_WIDTH)
#define HEAD_TGAP 0
#define HEAD_LGAP TITL_WIDTH

#define FOOT_HEIGHT 1
#define FOOT_WIDTH COLS
#define FOOT_TGAP (LINES-1)
#define FOOT_LGAP 0

#define BODY_HEIGHT (LINES-FOOT_HEIGHT-HEAD_HEIGHT)
#define BODI_HEIGHT 1
#define BODY_WIDTH COLS
#define BODY_TGAP 1
#define BODY_LGAP 0

static WINDOW *titl_w=NULL,*head_w=NULL,*foot_w=NULL,**body_w=NULL;
static int bw_len=0;
static int content_bw_max=0;
static int cursor_i=0;
static int newsize=0;

#define redo_allocation(x,n) {if(x){REINIT_MEM(x,n);}else{INIT_MEM(x,n);}}

int next_item(struct mainwindow *mw){
	wattroff(body_w[cursor_i],A_BOLD);
	if(cursor_i<content_bw_max)
		cursor_i++;
	/* TODO: pages */
	wattron(body_w[cursor_i],A_BOLD);
	return KH_RET_UPDATE;
}

int prev_item(struct mainwindow *mw){
	wattroff(body_w[cursor_i],A_BOLD);
	if(cursor_i>0)
		cursor_i--;
	wattron(body_w[cursor_i],A_BOLD);
	return KH_RET_UPDATE;
}

int select_item(struct mainwindow *mw){
	if(mw->ctx_type==CTX_FEEDS){
		mw->ctx_type=CTX_ENTRIES;
		mw->ctx_id=cursor_i;
		mw->page=0;
		request_list_update(mw);
		wattroff(body_w[cursor_i],A_BOLD);
		cursor_i=0;
		wattron(body_w[cursor_i],A_BOLD);
	}
	else if(mw->ctx_type==CTX_ENTRIES){
		int tmp_id;
		tmp_id=(mw->page*mw->body_len)+cursor_i;
		pipe_entry(mw,tmp_id);
	}
	return KH_RET_UPDATE;
}

int context_exit(struct mainwindow *mw){
	if(mw->ctx_type==CTX_FEEDS)
		return KH_RET_EXIT;
	else{
		mw->ctx_type=CTX_FEEDS;
		mw->ctx_id=0;
		mw->page=0;
		request_list_update(mw);
		return KH_RET_UPDATE;
	}
}

void clear_display(){
	int i;

	erase();
	refresh();
	return;

	wrefresh(titl_w);
	wrefresh(head_w);
	wrefresh(foot_w);
	for(i=0;i<bw_len;i++)
		wrefresh(body_w[i]);
}

void regen_windows(){
	int i;

	if(titl_w)
		delwin(titl_w);
	if(head_w)
		delwin(head_w);
	if(foot_w)
		delwin(foot_w);
	if(body_w){
		for(i=0;i<bw_len;i++)
			delwin(body_w[i]);
	}

	bw_len=BODY_HEIGHT;
	REINIT_MEM(body_w,bw_len);

	titl_w=newwin(TITL_HEIGHT,TITL_WIDTH,TITL_TGAP,TITL_LGAP);
	wrefresh(titl_w);

	head_w=newwin(HEAD_HEIGHT,HEAD_WIDTH,HEAD_TGAP,HEAD_LGAP);
	wrefresh(head_w);

	foot_w=newwin(FOOT_HEIGHT,FOOT_WIDTH,FOOT_TGAP,FOOT_LGAP);
	wrefresh(foot_w);

	for(i=0;i<bw_len;i++){
		body_w[i]=newwin(BODI_HEIGHT,BODY_WIDTH,(i*BODI_HEIGHT)+BODY_TGAP,BODY_LGAP);
	}

	wattron(body_w[cursor_i],A_BOLD);

	clear_display();
}

void resize_mainwindow(struct mainwindow *mw){
	int i;
	const int tl=BODY_HEIGHT*BODY_WIDTH;

	if(mw->width!=BODY_WIDTH || mw->body_len!=BODY_HEIGHT){
		regen_windows();
	}

	mw->body_len=BODY_HEIGHT;
	mw->width=BODY_WIDTH;
	redo_allocation(mw->header,mw->width);
	redo_allocation(mw->footer,mw->width);
	redo_allocation(mw->data.lv,mw->body_len);
	redo_allocation(mw->body,tl);

	for(i=0;i<mw->body_len;i++){
		mw->data.lv[i].line=mw->body+(i*mw->width);
	}
}

int chars_to_widechars(tchar_t *dst, const char *src, int max){
	int bn;

	if(max<1)
		return 0;

	//int n=mbstowcs(NULL,src,0); // find wide length
	bn=mbstowcs(dst,src,max);

	return bn;
}

static void print_crop(WINDOW *w, tchar_t *str, int max){
	str[max-1]=0;
	mvwprintw(w,0,0,"%ls",str);
	wrefresh(w);
}

void update_view(struct mainwindow *mw){
	int i;
	if(newsize){
		resize_mainwindow(mw);
		newsize=0;
	}
	else{
		clear_display();
	}

	mvwaddstr(titl_w,0,0,TITL_STRING);
	wrefresh(titl_w);
	print_crop(head_w,mw->header,mw->width);
	print_crop(foot_w,mw->footer,mw->width);
	for(i=0;i<mw->body_len;i++){
		print_crop(body_w[i],mw->data.lv[i].line,mw->width);
		if(mw->data.lv[i].line[0])
			content_bw_max=i;
	}
}

struct mainwindow* setup_ui(){
	struct mainwindow *mw;

	setlocale(LC_ALL,getenv("LC_ALL"));

	(void) initscr();      /* initialize the curses library */
	keypad(stdscr, TRUE);  /* enable keyboard mapping */
	(void) nonl();         /* tell curses not to do NL->CR/NL on output */
	(void) cbreak();       /* take input chars one at a time, no wait for \n */
	noecho();
	//(void) echo();         /* echo input - in color */

	move(LINES-1,0);

	//pthread_mutex_init(&view_m,NULL);

	INIT_MEM(mw,1);
	memset(mw,0,sizeof(*mw));

	resize_mainwindow(mw);

	bind_defaults();

	mw->ctx_type=CTX_FEEDS;
	mw->ctx_id=0;
	if(pipe(mw->outfd)!=0)
		; /* TODO: error */
	if(pipe(mw->infd)!=0)
		; /* TODO: error */
	return mw;
}

void run_ui(struct mainwindow *mw){
	int ch;
	int ret;

	while((ch = getch())){
		ret=process_key(ch,mw);
		if(ret==KH_RET_EXIT)
			break;
		else if(ret==KH_RET_UPDATE)
			update_view(mw);
	}
}

void end_ui(){
	//pthread_mutex_destroy(&view_m);

	endwin();
}

