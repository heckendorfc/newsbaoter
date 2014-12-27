#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "urlparse.h"
#include "list.h"
#include "config.h"
#include "hash.h"

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

struct urllist* urlparse(){
	struct urllist *urls=NULL;
	struct urllist *lptr;
	char *file=get_config_file(NB_URL_FILE);
	char *bp,*next;
	int fd;
	struct conf_read c;
	struct taglist *tlp;

	if((fd=open_config(file))<0){
		fprintf(stderr,"No URL file found. Please configure ~/%s\n",NB_URL_FILE);
		free(file);
		exit(1);
	}

	memset(&c,0,sizeof(c));

	init_urltag_table();

	while(read_config_line(fd,&c)){
		bp=get_line_word(c.buf,&next);
		if(bp==NULL)
			break;

		LIST_ADD(urls,lptr);
		urls->data.url=strdup(bp);

		while((bp=get_line_word(next,&next))){
			tlp=get_urltag(bp);
			if(tlp==NULL){
				INIT_MEM(tlp,1);
				tlp->next=NULL;
			}
			else{
				struct taglist *tptr;
				LIST_ADD(tlp,tptr);
			}
			tlp->data=&urls->data;
			set_urltag(bp,tlp);
		}
	}

	clean_conf_read(&c);

	free(file);

	return urls;
}
