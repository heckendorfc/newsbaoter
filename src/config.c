#include <stdlib.h>
#include <stddef.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ncurses.h>

#include "common_defs.h"
#include "config.h"
#include "ui/view.h"
#include "ui/keys.h"

#define CONF_BUF_INC 256

struct nb_config global_config={
	.proxy=NULL,
	.dl_retries=1,
	.auto_reload=1,
	.reload_time=30,
	.show_read_feeds=1,
	.show_read_entries=1,
	.parallel_reload=5,
	.confirm_exit=0,
	.notify_beep=0,
	.entry_retention=3*24*60*60,
};

int copy_str(void *d, char *next){
	char **sd=(char**)d;
	char *v;

	while((v=get_line_word(next,&next))){
		*sd=strdup(v);
		return 0;
	}

	return 1;
}

int copy_int(void *d, char *next){
	int *id=(int*)d;
	char *v;

	while((v=get_line_word(next,&next))){
		switch(v[0]){
			case 'y':
			case 'Y':
				*id=1;
				break;
			case 'n':
			case 'N':
				*id=0;
				break;
			default:
				*id=strtol(v,NULL,10); /* TODO: test for number? */
		}
		return 0;
	}

	return 1;
}

static const char *color_type_strings[]={
	"listnormal",
	"listfocus",
	"listnormal_unread",
	"listfocus_unread",
	"info",
	"article",
	"background",
	NULL
};

int get_color_type_index(char *s){
	int i;

	for(i=0;color_type_strings[i];i++)
		if(strcmp(color_type_strings[i],s)==0)
			return i;

	return -1;
}

static const char *color_name_strings[]={
	"black",
	"red",
	"green",
	"yellow",
	"blue",
	"magenta",
	"cyan",
	"white",
	NULL
};

int get_color_name_index(char *s){
	int i;
	const char *ebc="color";

	/* 256 color */
	for(i=0;ebc[i] && s[i] && ebc[i]==s[i];i++);
	if(ebc[i]==0)
		return strtol(s+i,NULL,10);

	for(i=0;color_name_strings[i];i++)
		if(strcmp(color_name_strings[i],s)==0)
			return i;

	return -1;
}

struct attrmap{
	const char *name;
	const int attr;
}curses_attr_map[]={
	{"standout",A_STANDOUT},
	{"underline",A_UNDERLINE},
	{"reverse",A_REVERSE},
	{"blink",A_BLINK},
	{"dim",A_DIM},
	{"bold",A_BOLD},
	{"protect",A_PROTECT},
	{"invis",A_INVIS},
	{NULL,0}
};

int get_color_attr(char *s){
	int i;

	for(i=0;curses_attr_map[i].name;i++)
		if(strcmp(curses_attr_map[i].name,s)==0)
			return curses_attr_map[i].attr;

	return 0;
}

void set_nbcolor(nbcolor_t *cd, unsigned int cfi, unsigned int cbi, unsigned int attr){
	cd->color=cfi|(cbi<<16);
	cd->attr=attr;
}

void get_nbcolor(nbcolor_t *cd, unsigned int *cfi, unsigned int *cbi, unsigned int *attr){
	*cbi=cd->color>>16;
	*cfi=cd->color&0xFFFF;
	*attr=cd->attr;
}

int copy_color(void *d, char *next){
	nbcolor_t *cd=(nbcolor_t*)d;
	char *v;
	int cti;
	int cfi,cbi;
	int cat=0;

	if(!(v=get_line_word(next,&next)))
		return 1;

	if((cti=get_color_type_index(v))<0)
		return 1;

	if(!(v=get_line_word(next,&next)))
		return 1;

	if((cfi=get_color_name_index(v))<0)
		return 1;

	if(!(v=get_line_word(next,&next)))
		return 1;

	if((cbi=get_color_name_index(v))<0)
		return 1;

	while((v=get_line_word(next,&next))){
		cat|=get_color_attr(v);
	}

	set_nbcolor(&cd[cti],cfi,cbi,cat);

	return 0;
}

int config_bind_key(void *d, char *next){
	char *v;
	tchar_t key;
	handler_t h;
	int f=d-((void*)&global_config);

	if(!(v=get_line_word(next,&next)))
		return 1;

	if(mbtowc(&key,v,strlen(v))<0)
		return 1;

	if(f==0){
		unbind_key(key);
	}
	else{
		if(!(v=get_line_word(next,&next)))
			return 1;

		if(!(h=find_handler(v)))
			return 1;

		bind_key(key,h);
	}

	return 0;
}

#define NBCO(n) offsetof(struct nb_config, n)
struct config_args{
	const char *str;
	long doff;
	int (*process)(void*,char *);
}config_options[]={
	{"proxy",NBCO(proxy),copy_str},
	{"download-retries",NBCO(dl_retries),copy_int},
	{"auto-reload",NBCO(auto_reload),copy_int},
	{"reload-time",NBCO(reload_time),copy_int},
	{"show-read-feeds",NBCO(show_read_feeds),copy_int},
	{"show-read-articles",NBCO(show_read_entries),copy_int},
	{"reload-threads",NBCO(parallel_reload),copy_int},
	{"confirm-exit",NBCO(confirm_exit),copy_int},
	{"notify-beep",NBCO(notify_beep),copy_int},
	{"color",NBCO(colors),copy_color},
	{"bind-key",0x1,config_bind_key},
	{"unbind-key",0x0,config_bind_key},
	{"cache-retention",NBCO(entry_retention),copy_int},
	{NULL,0,NULL}
};

int open_config(const char *file){
	return open(file,O_RDONLY);
}

static void move_buf(int off, struct conf_read *c){
	int n=(off-c->l_ret)+1;
	c->b_stored-=c->l_ret;
	memmove(c->buf,c->buf+c->l_ret,c->b_stored);
	c->l_ret=n;
}

static int check_for_line(struct conf_read *c){
	int i;

	if(c->l_ret>=c->b_stored)
		return 0;

	for(i=c->l_ret;i<c->b_stored && c->buf[i]=='\n';i++);
	c->l_ret=i;

	for(i=c->l_ret;i<c->b_stored;i++){
		if(c->buf[i]=='\n'){
			c->buf[i]=0;
			while(i<c->b_stored && (c->buf[i]==' ' || c->buf[i]=='\t'))i++;
			move_buf(i,c);
			return 1;
		}
	}

	return 0;
}

/*TODO: ring buffer? */
int read_config_line(int fd, struct conf_read *c){
	int nr;

	if(c->b_alloc<1){
		INIT_MEM(c->buf,CONF_BUF_INC);
		c->b_alloc=CONF_BUF_INC;
	}

	/* Check if there was a second line in the buffer */
	if(check_for_line(c))
		return 1;

	if(c->eof)
		return 0;

	/* Move new data to the front */
	c->b_stored-=c->l_ret;
	memmove(c->buf,c->buf+c->l_ret,c->b_stored);
	c->l_ret=0;

	do{
		nr=read(fd,c->buf+c->b_stored,c->b_alloc-c->b_stored);
		if(nr<0){
			perror("Read URL file failed");
			exit(1);
		}
		else if(nr==0 && c->b_stored>0 && c->buf[c->b_stored-1]!='\n')
			c->buf[c->b_stored]='\n';

		c->b_stored+=nr;
		if(c->b_stored==c->b_alloc){
			c->b_alloc+=CONF_BUF_INC;
			REINIT_MEM(c->buf,c->b_alloc);
		}
	}while(!check_for_line(c) && nr>0);

	if(nr==0)
		c->eof=1;

	if(c->eof && c->b_stored<=0)
		return 0;

	return 1;
}

char* get_line_word(char *buf, char **next){
	char *p=buf;
	char *np;

	if(buf==NULL || *buf==0)
		return NULL;

	while(*p && *p!=' ' && *p!='\t')
		p++;

	np=p;
	if(*np){
		np++;
		while(*np && (*np==' ' || *np=='\t'))
			np++;
	}

	*p=0;

	*next=np;

	return buf;
}

void clean_conf_read(struct conf_read *c){
	if(c->buf)
		free(c->buf);
}

char* get_config_file(const char *file){
	char *home=getenv("HOME");
	char *ret;
	const int hn = strlen(home);
	const int cn = strlen(file);
	int n=hn+cn+2;

	INIT_MEM(ret,n);

	snprintf(ret,n,"%s/%s",home,file);

	return ret;
}

static int find_config_index(char *s){
	int i;

	for(i=0;config_options[i].str;i++){
		if(strcmp(config_options[i].str,s)==0)
			return i;
	}

	return -1;
}

void read_config_options_file(){
	char *file=get_config_file(NB_CONF_FILE);
	int fd;
	char *bp,*next;
	struct conf_read c;
	int coff;

	if((fd=open_config(file))<0){
		fprintf(stderr,"No config file found. Please consider configuring ~/%s\n",NB_CONF_FILE);
		goto done;
	}

	memset(&c,0,sizeof(c));

	while(read_config_line(fd,&c)){
		if(c.buf[0]=='#')
			continue;

		bp=get_line_word(c.buf,&next);
		if(bp==NULL)
			break;

		if((coff=find_config_index(bp))<0)
			continue;

		/* TODO: error check */
		config_options[coff].process(
				((void*)(&global_config))+config_options[coff].doff,
				next);
	}

	set_config_colors();

	clean_conf_read(&c);

done:
	free(file);
}

