//#include <sqlite3.h>
#include <sys/select.h>
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

static int print_crop(WINDOW *w, tchar_t *str, int max);

static void restyle_window(WINDOW *w, int style){
	wattrset(w,global_config.colors[style].attr);
	wcolor_set(w,style,NULL);
}

static void restyle_focus_item(struct mainwindow *mw, int style){
	style=FI_STYLE(style);
	restyle_window(body_w[cursor_i],style);
	print_crop(body_w[cursor_i],mw->data.lv[cursor_i].line,BODY_WIDTH);
}

int low_item(struct mainwindow *mw){
	restyle_focus_item(mw,CP_LISTNORMAL);
	cursor_i=content_bw_max;
	restyle_focus_item(mw,CP_LISTFOCUS);
	return KH_RET_OK;
}

int mid_item(struct mainwindow *mw){
	restyle_focus_item(mw,CP_LISTNORMAL);
	cursor_i=content_bw_max/2;
	restyle_focus_item(mw,CP_LISTFOCUS);
	return KH_RET_OK;
}

int high_item(struct mainwindow *mw){
	restyle_focus_item(mw,CP_LISTNORMAL);
	cursor_i=0;
	restyle_focus_item(mw,CP_LISTFOCUS);
	return KH_RET_OK;
}

int next_item(struct mainwindow *mw){
	restyle_focus_item(mw,CP_LISTNORMAL);
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
	restyle_focus_item(mw,CP_LISTFOCUS);
	return KH_RET_OK;
}

int next_unread_item(struct mainwindow *mw){
	int i;
	int found=-1;
	restyle_focus_item(mw,CP_LISTNORMAL);
	for(i=cursor_i;i<=content_bw_max;i++){
		if(mw->data.lv[i].unread){
			found=cursor_i=i;
		}
	}
	if(found<0)
		next_unread_ipc(mw,cursor_i);
	restyle_focus_item(mw,CP_LISTFOCUS);
	return KH_RET_UPDATE;
}

int prev_item(struct mainwindow *mw){
	restyle_focus_item(mw,CP_LISTNORMAL);
	if(cursor_i>0)
		cursor_i--;
	else if(mw->page>0){
		mw->page--;
		cursor_i=mw->body_len-1;
		request_list_update(mw);
	}
	restyle_focus_item(mw,CP_LISTFOCUS);
	return KH_RET_OK;
}

int select_item(struct mainwindow *mw){
	if(mw->ctx_type==CTX_FEEDS){
		mw->ctx_type=CTX_ENTRIES;
		mw->ctx_id=mw->data.lv[cursor_i].id;
		mw->page=0;
		request_list_update(mw);
		restyle_focus_item(mw,CP_LISTNORMAL);
		cursor_i=0;
		restyle_focus_item(mw,CP_LISTFOCUS);
	}
	else if(mw->ctx_type==CTX_ENTRIES){
		int tmp_id;
		//tmp_id=(mw->page*mw->body_len)+cursor_i;
		tmp_id=mw->data.lv[cursor_i].id;
		pipe_entry(mw,tmp_id);
	}
	return KH_RET_UPDATE;
}

void notify_refresh_footer(struct mainwindow *mw){
	mvwaddstr(foot_w,0,FOOT_WIDTH-21,"Refreshing Feeds");
	wrefresh(foot_w);
}

int refresh_all(struct mainwindow *mw){
	ipcinfo ii=IPCVAL_REFRESH_ALL;

	write(mw->outfd[1],&ii,sizeof(ii));
	notify_refresh_footer(mw);

	return KH_RET_OK;
}

int catchup_feed(struct mainwindow *mw){
	if(mw->ctx_type==CTX_FEEDS){
		catchup_entries(mw,mw->data.lv[cursor_i].id);
		restyle_focus_item(mw,CP_LISTFOCUS);
		return KH_RET_OK;
	}
	else{
		catchup_entries(mw,mw->ctx_id);
		return KH_RET_UPDATE;
	}
}

int toggle_read(struct mainwindow *mw){
	toggle_read_ipc(mw,mw->data.lv[cursor_i].id);
	restyle_focus_item(mw,CP_LISTFOCUS);
	if(mw->ctx_id==0 || !global_config.show_read_entries)
		return KH_RET_UPDATE;
	return KH_RET_OK;
}

int catchup_all(struct mainwindow *mw){
	catchup_entries(mw,0);
	return KH_RET_UPDATE;
}

int context_exit(struct mainwindow *mw){
	if(mw->ctx_type==CTX_FEEDS)
		return KH_RET_EXIT;
	else{
		cursor_i=0;
		mw->ctx_type=CTX_FEEDS;
		mw->ctx_id=0;
		mw->page=0;
		request_list_update(mw);
		return KH_RET_UPDATE;
	}
}

void clear_display(){
	int i;

	clear();
	//erase();
	refresh();
	return;

	wrefresh(titl_w);
	wrefresh(head_w);
	wrefresh(foot_w);
	for(i=0;i<bw_len;i++)
		wrefresh(body_w[i]);
}

void regen_windows(struct mainwindow *mw){
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

	if(bw_len>BODY_HEIGHT)
		cursor_i=0;

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

	restyle_window(body_w[cursor_i],FI_STYLE(CP_LISTFOCUS));

	clear_display();
}

void resize_mainwindow(struct mainwindow *mw){
	int i;
	const int wd=BODY_WIDTH+1;
	const int tl=BODY_HEIGHT*wd;

	if(mw->width!=BODY_WIDTH || mw->body_len!=BODY_HEIGHT){
		regen_windows(mw);
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

static int print_crop(WINDOW *w, tchar_t *str, int max){
	int i;
	int ret;

	if(str[0])
		ret=1;
	else
		ret=0;

	for(i=0;i<max && str[i] && str[i]==' ';i++)
		;
	if(i==max)
		ret=0;
	for(;i<max && str[i];i++)
		;
	for(;i<max;i++)
		str[i]=' ';
	str[max]=0;
	mvwprintw(w,0,0,"%ls",str);
	wrefresh(w);

	return ret;
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

	if(mw->beep_request){
		beep();
		mw->beep_request=0;
	}

	clear_display();

	mvwaddstr(titl_w,0,0,TITL_STRING);
	wrefresh(titl_w);
	print_crop(head_w,mw->header,HEAD_WIDTH);
	//print_crop(head_w,mw->header,mw->width);
	print_crop(foot_w,mw->footer,FOOT_WIDTH);
	//print_crop(foot_w,mw->footer,mw->width);
	for(i=0;i<mw->body_len;i++){
		restyle_window(body_w[i],FB_STYLE);
		if(print_crop(body_w[i],mw->data.lv[i].line,BODY_WIDTH))
			content_bw_max=i;
	}
}

void catchresize(int sig){
	newsize=1;
}

void set_color_pair(int n, int f, int b, int a){
	set_nbcolor(global_config.colors+n,f,b,a);
	init_pair(n,f,b);
}

static void set_default_colors(){
	memset(&global_config.colors,0,sizeof(global_config.colors));

	set_color_pair(CP_LISTNORMAL,COLOR_WHITE,0,0);
	set_color_pair(CP_LISTNORMAL_UR,COLOR_WHITE,0,0);
	set_color_pair(CP_LISTFOCUS,COLOR_WHITE,0,A_BOLD);
	set_color_pair(CP_LISTFOCUS_UR,COLOR_WHITE,0,A_BOLD);
	set_color_pair(CP_INFO,COLOR_WHITE,COLOR_BLUE,0);
	set_color_pair(CP_ARTICLE,COLOR_WHITE,0,0);
	set_color_pair(CP_BACKGROUND,COLOR_WHITE,0,0);

	init_pair(CP_ALERT,COLOR_WHITE,COLOR_RED);
}

void set_config_colors(){
	int i;
	unsigned int cfi,cbi,attr;

	for(i=0;i<CP_NULL;i++){
		get_nbcolor(global_config.colors+i,&cfi,&cbi,&attr);
		init_pair(i,cfi,cbi);
	}
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
	timeout(0);

	start_color();
	set_default_colors();

	move(LINES-1,0);

	//pthread_mutex_init(&view_m,NULL);

	INIT_MEM(mw,1);
	memset(mw,0,sizeof(*mw));

	pthread_mutex_init(&mw->viewtex,NULL);

	resize_mainwindow(mw);

	bind_defaults();

	mw->ctx_type=CTX_FEEDS;
	mw->ctx_id=0;
	if(pipe(mw->outfd)!=0)
		; /* TODO: error */
	if(pipe(mw->infd)!=0)
		; /* TODO: error */
	if(pipe(mw->sidefd)!=0)
		; /* TODO: error */
	return mw;
}

void run_ui(struct mainwindow *mw){
	int ch;
	int ret;
	fd_set fdread;
	int sret;
	ipcinfo ii;

	while(1){
		FD_ZERO(&fdread);
		FD_SET(mw->sidefd[0],&fdread);
		FD_SET(0,&fdread);
		sret=select(mw->sidefd[0]+1,&fdread,NULL,NULL,NULL);

		if(FD_ISSET(0,&fdread)){
			ch = getch();

			ret=process_key(ch,mw);
			if(ret==KH_RET_EXIT){
				if(global_config.confirm_exit){
					wcolor_set(foot_w,CP_ALERT,NULL);
					mvwaddstr(foot_w,0,0,"Confirm exit? (y/[n])");
					wrefresh(foot_w);
					timeout(-1);
					if(getch()=='y'){
						break;
					}
					else{
						wcolor_set(foot_w,CP_INFO,NULL);
						update_view(mw);
					}
					timeout(0);
				}
				else
					break;
			}
			else if(ret==KH_RET_UPDATE)
				update_view(mw);
		}
		if(FD_ISSET(mw->sidefd[0],&fdread)){
			int rr=read(mw->sidefd[0],&ii,sizeof(ii));
			if(rr>0 && ii==IPCVAL_UPDATE_REQUEST)
				update_view(mw);
		}
	}
}

void end_ui(){
	//pthread_mutex_destroy(&view_m);

	endwin();
}
