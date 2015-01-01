#include <stdlib.h>
#include <stdio.h>
#include <ncurses.h>
#include <string.h>
#include <unistd.h>

#include "../hash.h"
#include "keys.h"
#include "view.h"
#include "util.h"

#define KHN(n) handle_ ## n
#define KHF(n) {#n , KHN(n) }

/* Key handlers */

int handle_print_key_pairs(struct mainwindow *mw);

int handle_exit(struct mainwindow *mw){
	return context_exit(mw);
}

int handle_next(struct mainwindow *mw){
	return next_item(mw);
}

int handle_low_item(struct mainwindow *mw){
	return low_item(mw);
}

int handle_mid_item(struct mainwindow *mw){
	return mid_item(mw);
}

int handle_high_item(struct mainwindow *mw){
	return high_item(mw);
}

int handle_next_unread(struct mainwindow *mw){
	return next_unread_item(mw);
}

int handle_prev(struct mainwindow *mw){
	return prev_item(mw);
}

int handle_select(struct mainwindow *mw){
	return select_item(mw);
}

int handle_refresh_all(struct mainwindow *mw){
	return refresh_all(mw);
}

int handle_catchup_feed(struct mainwindow *mw){
	return catchup_feed(mw);
}

int handle_catchup_all(struct mainwindow *mw){
	return catchup_all(mw);
}

int handle_toggle_read(struct mainwindow *mw){
	return toggle_read(mw);
}

/* Defaults and Infrastructure */

struct keylist{
	int key;
	handler_t handler;
};

static struct keylist default_keys[]={
	{'q',KHN(exit)},
	{'j',KHN(next)},
	//{'n',KHN(next_unread)}, /* TODO: implement this */
	{'k',KHN(prev)},
	{'L',KHN(low_item)},
	{'M',KHN(mid_item)},
	{'H',KHN(high_item)},
	//{KEY_ENTER,KHN(select)},
	{'o',KHN(select)},
	{'R',KHN(refresh_all)},
	{'A',KHN(catchup_feed)},
	{'C',KHN(catchup_all)},
	{'N',KHN(toggle_read)},
	{'?',KHN(print_key_pairs)},
	{'\0',NULL}
};

struct handlerlist{
	char *name;
	handler_t handler;
};

static struct handlerlist handler_names[]={
	KHF(exit),
	KHF(next),
	KHF(prev),
	KHF(select),
	{"high-item",KHN(high_item)},
	{"mid-item",KHN(mid_item)},
	{"low-item",KHN(low_item)},
	{"reload-all",KHN(refresh_all)},
	{"mark-feed-read",KHN(catchup_feed)},
	{"mark-all-feeds-read",KHN(catchup_all)},
	{"toggle-article-read",KHN(toggle_read)},
	{"help",KHN(print_key_pairs)},
	{NULL,NULL}
};

handler_t find_handler(char *s){
	int i;

	for(i=0;handler_names[i].name;i++)
		if(strcmp(handler_names[i].name,s)==0)
			break;

	return handler_names[i].handler;
}

void bind_key(int c, handler_t handler){
	char s[17];

	sprintf(s,"%x",c);

	set_uikey(s,handler);
}

void unbind_key(int c){
	char s[17];

	sprintf(s,"%x",c);

	unset_uikey(s);
}

int process_key(int c, struct mainwindow *mw){
	handler_t kh;
	char s[17];

	sprintf(s,"%x",c);

	if((kh=get_uikey(s)))
		return kh(mw);

	return KH_RET_NO_HANDLER;
}

int handle_print_key_pairs(struct mainwindow *mw){
	int off=0;
	void *p;
	char *name;
	int i;
	const int tlen=256;
	char tmp[tlen];
	int fd;
	int pid;
	tchar_t c;

	def_prog_mode();
	endwin();

	pipe_to_pager(mw,&pid,&fd);

	i=snprintf(tmp,tlen,"newsbaoter key bindings\n\n");
	write(fd,tmp,i);

	while((p=next_uikey_pair(&off,&name))){
		for(i=0;handler_names[i].handler;i++){
			if(handler_names[i].handler==p){
				c=strtol(name,NULL,16);
				i=snprintf(tmp,tlen,"%lc\t%s\n",c,handler_names[i].name);
				write(fd,tmp,i);
				break;
			}
		}
	}

	close(fd);

	wait_for_pager(mw,pid);

	reset_prog_mode();


	return KH_RET_UPDATE;
}

void set_foot_help_string(char *s, int len){
	int off=0;
	void *p;
	char *name;
	tchar_t c;
	tchar_t exit_key,help_key;
	tchar_t *d=NULL;

	exit_key=help_key=' ';

	while((p=next_uikey_pair(&off,&name))){
		d=NULL;
		if(p==handle_exit){
			d=&exit_key;
		}
		else if(p==handle_print_key_pairs){
			d=&help_key;
		}
		if(d){
			c=strtol(name,NULL,16);
			*d=c;
		}
	}
	snprintf(s,len,"Press: %lc to exit, %lc for help",exit_key,help_key);
}

void bind_defaults(){
	int i;

	init_uikey_table();

	for(i=0;default_keys[i].key!='\0';i++)
		bind_key(default_keys[i].key,default_keys[i].handler);
}
