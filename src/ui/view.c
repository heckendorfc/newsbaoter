//#include <sqlite3.h>
#include <sys/ioctl.h>
#include <signal.h>
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
#include "../config.h"

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

static void restyle_window(WINDOW *w, int style){
	wattrset(w,global_config.colors[style].attr);
	wcolor_set(w,style,NULL);
}

static void restyle_focus_item(int style){
	restyle_window(body_w[cursor_i],style);
}

int next_item(struct mainwindow *mw){
	restyle_focus_item(CP_LISTNORMAL);
	if(cursor_i<content_bw_max)
		cursor_i++;
	else if(content_bw_max==mw->body_len-1){
		cursor_i=0;
		mw->page++;
		if(request_list_update(mw)){ /* end of list; undo */
			cursor_i=content_bw_max;
			mw->page--;
		}
	}
	restyle_focus_item(CP_LISTFOCUS);
	return KH_RET_UPDATE;
}

int prev_item(struct mainwindow *mw){
	restyle_focus_item(CP_LISTNORMAL);
	if(cursor_i>0)
		cursor_i--;
	else if(mw->page>0){
		mw->page--;
		cursor_i=mw->body_len-1;
		request_list_update(mw);
	}
	restyle_focus_item(CP_LISTFOCUS);
	return KH_RET_UPDATE;
}

int select_item(struct mainwindow *mw){
	if(mw->ctx_type==CTX_FEEDS){
		mw->ctx_type=CTX_ENTRIES;
		mw->ctx_id=cursor_i;
		mw->page=0;
		request_list_update(mw);
		restyle_focus_item(CP_LISTNORMAL);
		cursor_i=0;
		restyle_focus_item(CP_LISTFOCUS);
	}
	else if(mw->ctx_type==CTX_ENTRIES){
		int tmp_id;
		tmp_id=(mw->page*mw->body_len)+cursor_i;
		pipe_entry(mw,tmp_id);
	}
	return KH_RET_UPDATE;
}

int refresh_all(struct mainwindow *mw){
	ipcinfo ii=IPCVAL_REFRESH_ALL;

	write(mw->outfd[1],&ii,sizeof(ii));
	mvwaddstr(foot_w,0,FOOT_WIDTH-21,"Refreshing Feeds");

	return KH_RET_OK;
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
	//wrefresh(titl_w);
	restyle_window(titl_w,CP_INFO);

	head_w=newwin(HEAD_HEIGHT,HEAD_WIDTH,HEAD_TGAP,HEAD_LGAP);
	//wrefresh(head_w);
	restyle_window(head_w,CP_INFO);

	foot_w=newwin(FOOT_HEIGHT,FOOT_WIDTH,FOOT_TGAP,FOOT_LGAP);
	//wrefresh(foot_w);
	restyle_window(foot_w,CP_INFO);

	for(i=0;i<bw_len;i++){
		body_w[i]=newwin(BODI_HEIGHT,BODY_WIDTH,(i*BODI_HEIGHT)+BODY_TGAP,BODY_LGAP);
		restyle_window(body_w[i],CP_LISTNORMAL);
	}

	restyle_focus_item(CP_LISTFOCUS);

	clear_display();
}

void resize_mainwindow(struct mainwindow *mw){
	int i;
	const int wd=BODY_WIDTH+1;
	const int tl=BODY_HEIGHT*wd;

	if(mw->width!=BODY_WIDTH || mw->body_len!=BODY_HEIGHT){
		regen_windows();
	}

	mw->body_len=BODY_HEIGHT;
	mw->width=BODY_WIDTH;
	redo_allocation(mw->header,wd);
	redo_allocation(mw->footer,wd);
	redo_allocation(mw->data.lv,mw->body_len);
	redo_allocation(mw->body,tl);

	for(i=0;i<mw->body_len;i++){
		mw->data.lv[i].line=mw->body+(i*wd);
		mw->data.lv[i].line[0]=0;
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
	int i;

	for(i=0;i<max && str[i];i++)
		;
	for(;i<max;i++)
		str[i]=' ';
	str[max]=0;
	mvwprintw(w,0,0,"%ls",str);
	wrefresh(w);
}

void update_view(struct mainwindow *mw){
	int i;

	if(newsize){
		struct winsize ws;
		ioctl(0, TIOCGWINSZ, &ws);
		resizeterm(ws.ws_row,ws.ws_col);
		resize_mainwindow(mw);
		newsize=0;
		request_list_update(mw);
	}

	clear_display();

	mvwaddstr(titl_w,0,0,TITL_STRING);
	wrefresh(titl_w);
	print_crop(head_w,mw->header,HEAD_WIDTH);
	//print_crop(head_w,mw->header,mw->width);
	print_crop(foot_w,mw->footer,FOOT_WIDTH);
	//print_crop(foot_w,mw->footer,mw->width);
	for(i=0;i<mw->body_len;i++){
		print_crop(body_w[i],mw->data.lv[i].line,BODY_WIDTH);
		if(mw->data.lv[i].line[0])
			content_bw_max=i;
	}
}

void catchresize(int sig){
	newsize=1;
}

void set_color_pair(int n, int f, int b){
	init_pair(n,f,b);
}

static void set_default_colors(){
	memset(&global_config.colors,0,sizeof(global_config.colors));

	init_pair(CP_LISTNORMAL,COLOR_WHITE,0);
	init_pair(CP_LISTNORMAL_UR,COLOR_WHITE,0);
	init_pair(CP_LISTFOCUS,COLOR_WHITE,0);
	init_pair(CP_LISTFOCUS_UR,COLOR_WHITE,0);
	init_pair(CP_INFO,COLOR_WHITE,COLOR_BLUE);
	init_pair(CP_ARTICLE,COLOR_WHITE,0);
	init_pair(CP_BACKGROUND,COLOR_WHITE,0);

	global_config.colors[CP_LISTFOCUS].attr=A_BOLD;
	global_config.colors[CP_LISTFOCUS_UR].attr=A_BOLD;
}

struct mainwindow* setup_ui(){
	struct mainwindow *mw;

	setlocale(LC_ALL,getenv("LC_ALL"));
	signal(SIGWINCH,catchresize);

	(void) initscr();      /* initialize the curses library */
	keypad(stdscr, TRUE);  /* enable keyboard mapping */
	(void) nonl();         /* tell curses not to do NL->CR/NL on output */
	(void) cbreak();       /* take input chars one at a time, no wait for \n */
	noecho();
	//(void) echo();         /* echo input - in color */

	start_color();
	set_default_colors();

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
