#include <stdlib.h>
#include <stdio.h>

#include "../hash.h"
#include "keys.h"

#define KHN(n) handle_ ## n
#define KHF(n) {#n , KHN(n) }

/* Key handlers */

int handle_exit(){
	return KH_RET_EXIT;
}

/* Defaults and Infrastructure */

struct keylist{
	int key;
	handler_t handler;
};

static struct keylist default_keys[]={
	{'q',KHN(exit)},
	{'\0',NULL}
};

struct handlerlist{
	char *name;
	handler_t handler;
};

static struct handlerlist handler_names[]={
	KHF(exit),
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

int process_key(int c){
	handler_t kh;
	char s[17];

	sprintf(s,"%x",c);

	if((kh=get_uikey(s)))
		return kh();

	return KH_RET_NO_HANDLER;
}

void bind_defaults(){
	int i;

	init_uikey_table();

	for(i=0;default_keys[i].key!='\0';i++)
		bind_key(default_keys[i].key,default_keys[i].handler);
}
