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
#include "../common_defs.h"

#define HEAD_HEIGHT 1
#define HEAD_WIDTH COLS
#define HEAD_TGAP 0
#define HEAD_LGAP 0

#define FOOT_HEIGHT 1
#define FOOT_WIDTH COLS
#define FOOT_TGAP (LINES-1)
#define FOOT_LGAP 0

#define BODY_HEIGHT (LINES-FOOT_HEIGHT-HEAD_HEIGHT)
#define BODI_HEIGHT 1
#define BODY_WIDTH COLS
#define BODY_TGAP 1
#define BODY_LGAP 0

static WINDOW *head_w=NULL,*foot_w=NULL,**body_w=NULL;
static int bw_len=0;
static int cursor_i=0;
static int newsize=0;

#define redo_allocation(x,n) {if(x){REINIT_MEM(x,n);}else{INIT_MEM(x,n);}}

void clear_display(){
	int i;

	erase();
	refresh();
	return;

	wrefresh(head_w);
	wrefresh(foot_w);
	for(i=0;i<bw_len;i++)
		wrefresh(body_w[i]);
}

void regen_windows(){
	int i;

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

	head_w=newwin(HEAD_HEIGHT,HEAD_WIDTH,HEAD_TGAP,HEAD_LGAP);
	wrefresh(head_w);

	foot_w=newwin(FOOT_HEIGHT,FOOT_WIDTH,FOOT_TGAP,FOOT_LGAP);
	wrefresh(foot_w);

	for(i=0;i<bw_len;i++){
		body_w[i]=newwin(BODI_HEIGHT,BODY_WIDTH,(i*BODI_HEIGHT)+BODY_TGAP,BODY_LGAP);
	}

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

int chars_to_widechars(tchar_t *dst, char *src, int max){
	int bn;

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

	print_crop(head_w,mw->header,mw->width);
	print_crop(foot_w,mw->footer,mw->width);
	for(i=0;i<mw->body_len;i++)
		print_crop(body_w[i],mw->data.lv[i].line,mw->width);
}

struct mainwindow* setup_ui(){
	int num;
	struct mainwindow *mw;

	setlocale(LC_ALL,getenv("LC_ALL"));

	(void) initscr();      /* initialize the curses library */
	keypad(stdscr, TRUE);  /* enable keyboard mapping */
	//(void) nonl();         /* tell curses not to do NL->CR/NL on output */
	(void) cbreak();       /* take input chars one at a time, no wait for \n */
	noecho();
	//(void) echo();         /* echo input - in color */

	move(LINES-1,0);

	//pthread_mutex_init(&view_m,NULL);

	INIT_MEM(mw,1);
	memset(mw,0,sizeof(*mw));

	resize_mainwindow(mw);

	bind_defaults();

	move(1,2);
	printw("egerge");
	refresh();

	return mw;
}

void run_ui(){
	int ch;
	int ret;

	while((ch = getch())){
		ret=process_key(ch);
		if(ret==KH_RET_EXIT)
			break;
	}
}

void end_ui(){
	//pthread_mutex_destroy(&view_m);

	endwin();
}

