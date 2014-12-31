#include <curl/curl.h>

#include "common_defs.h"
#include "httpfetch.h"
#include "xmlproc.h"
#include "urlparse.h"
#include "config.h"

static size_t write_memory_cb(void *contents, size_t size, size_t nmemb, void *userp){
	size_t n=size*nmemb;

	xmlproc_parse_block(userp,contents,n);

	return n;
}

static void run_multi_group(CURLM *cm){
	struct timeval timeout;
	int rc=0; /* select() return code */
	CURLMcode mc; /* curl_multi_fdset() return code */
	int rnp=0;

	fd_set fdread;
	fd_set fdwrite;
	fd_set fdexcep;
	int maxfd = -1;

	long curl_timeo = -1;
	do{
		if(rc!=-1)
			mc=curl_multi_perform(cm,&rnp);

		if(rnp){
			FD_ZERO(&fdread);
			FD_ZERO(&fdwrite);
			FD_ZERO(&fdexcep);

			timeout.tv_sec = 1;
			timeout.tv_usec = 0;

			curl_multi_timeout(cm, &curl_timeo);
			if(curl_timeo >= 0) {
				timeout.tv_sec = curl_timeo / 1000;
				if(timeout.tv_sec > 1)
					timeout.tv_sec = 1;
				else
					timeout.tv_usec = (curl_timeo % 1000) * 1000;
			}

			mc = curl_multi_fdset(cm, &fdread, &fdwrite, &fdexcep, &maxfd);

			if(mc != CURLM_OK){
				fprintf(stderr,"fdset error\n");
				return;
			}

			if(maxfd == -1) {
				struct timeval wait = { 0, 100 * 1000 }; /* 100ms */
				rc = select(0, NULL, NULL, NULL, &wait);
			}
			else{
				rc = select(maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);
			}
		}

	}while(rnp);
}

void http_init(){
	curl_global_init(CURL_GLOBAL_ALL);
}

void fetch_urls(struct urllist *ul, const int npara, void *db){
	CURL **ce;
	CURLM *cm=curl_multi_init();
	struct xmlproc_data *xh;
	struct urllist *tul;
	struct urllist **ula;
	int i;
	int anp;
	CURLMsg *msg; /* for picking up messages with the transfer status */
	int msgs_left; /* how many messages are left */

	INIT_MEM(xh,npara);
	INIT_MEM(ce,npara);
	INIT_MEM(ula,npara);

	for(tul=ul;tul;tul=tul->next)
		tul->data.n_httperr=-1;

	/* TODO: this is awful. clean up at some point. */
	while(1){
		tul=ul;
		anp=0;
		for(i=0;i<npara;i++){
			if(tul==NULL)
				break;

			if(tul->data.n_httperr<0){
				tul->data.n_httperr=0;
			}
			else if(tul->data.n_httperr==0 || tul->data.n_httperr>=global_config.dl_retries){
				tul=tul->next;
				while(tul && (tul->data.n_httperr>=global_config.dl_retries || tul->data.n_httperr==0))
					tul=tul->next;
				if(tul==NULL)
					break;
				if(tul->data.n_httperr<0)
					tul->data.n_httperr=0;
			}

			ula[i]=tul;
			if(tul->data.info.doc){
				xmlproc_free_doc(tul->data.info.doc);
				tul->data.info.doc=NULL;
			}

			xmlproc_init(xh+i,tul);

			ce[i]=curl_easy_init();

			/* send all data to this function  */
			curl_easy_setopt(ce[i],CURLOPT_WRITEFUNCTION,write_memory_cb);
			curl_easy_setopt(ce[i],CURLOPT_WRITEDATA,(void *)(xh+i));
			curl_easy_setopt(ce[i],CURLOPT_USERAGENT,"newsbaoter/0.1");
			curl_easy_setopt(ce[i],CURLOPT_FOLLOWLOCATION,1);
			curl_easy_setopt(ce[i],CURLOPT_URL,tul->data.url);
			if(global_config.proxy)
				curl_easy_setopt(ce[i],CURLOPT_PROXY,global_config.proxy);

			curl_multi_add_handle(cm,ce[i]);

			tul=tul->next;
			anp++;
		}

		if(anp==0)
			break;

		run_multi_group(cm);

		while ((msg = curl_multi_info_read(cm, &msgs_left))) {
			if (msg->msg == CURLMSG_DONE) {
				int idx, found = 0;
				//char na[2]={0,0};

				/* Find out which handle this message is about */
				for (idx=0; idx<anp; idx++) {
					found = (msg->easy_handle == ce[idx]);
					if(found)
						break;
				}

				if(msg->data.result!=CURLE_OK){
					if(msg->data.result==CURLE_OPERATION_TIMEDOUT){
						if(ula[idx]->data.n_httperr<0)
							ula[idx]->data.n_httperr=1;
						else
							ula[idx]->data.n_httperr++;
					}
					else{
						fprintf(stderr,"libcurl error (%d) on url [try %d/%d]: %s\n",msg->data.result,ula[idx]->data.n_httperr,global_config.dl_retries,ula[idx]->data.url);
						ula[idx]->data.n_httperr=global_config.dl_retries;
					}
					continue;
				}

				xmlproc_finish(xh+idx,db);
				/*
				na[0]=idx+'0';
				xp_outf(xh[idx].doc,na);
				*/
				//ula[idx]->data.doc=xh[idx].doc;
				ula[idx]->data.info.feedid=xh[idx].feedid;
			}
		}

		for(i=0;i<anp;i++){
			curl_multi_remove_handle(cm,ce[i]);
			curl_easy_cleanup(ce[i]);
		}
	}

	curl_multi_cleanup(cm);

	free(ula);
	free(ce);
	free(xh);
}
