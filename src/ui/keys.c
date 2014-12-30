#include <stdlib.h>
#include <stdio.h>
#include <ncurses.h>

#include "../hash.h"
#include "keys.h"
#include "view.h"

#define KHN(n) handle_ ## n
#define KHF(n) {#n , KHN(n) }

/* Key handlers */

int handle_exit(struct mainwindow *mw){
	return context_exit(mw);
}

int handle_next(struct mainwindow *mw){
	return next_item(mw);
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
	{'k',KHN(prev)},
	{KEY_ENTER,KHN(select)},
	{'o',KHN(select)},
	{'R',KHN(refresh_all)},
	{'A',KHN(catchup_feed)},
	{'C',KHN(catchup_all)},
	{'N',KHN(toggle_read)},
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
	{NULL,NULL}
};

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

void bind_defaults(){
	int i;

	init_uikey_table();

	for(i=0;default_keys[i].key!='\0';i++)
		bind_key(default_keys[i].key,default_keys[i].handler);
}
