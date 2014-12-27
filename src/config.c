#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "common_defs.h"
#include "config.h"

#define CONF_BUF_INC 256

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
