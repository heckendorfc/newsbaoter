/* Copyright 2015 Christian Heckendorf.  All rights reserved.
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file.
 */

#include <curl/curl.h>
#include <string.h>

#include "common_defs.h"
#include "httpfetch.h"
#include "xmlproc.h"
#include "urlparse.h"
#include "config.h"

struct http_data{
	CURLM *cm;
	int npara;
	CURL **ce;
	struct xmlproc_data *xh;
	struct urllist **ula;
	int anp;
	int rnp;
	struct timeval timeout;
};

static size_t write_memory_cb(void *contents, size_t size, size_t nmemb, void *userp){
	size_t n=size*nmemb;

	xmlproc_parse_block(userp,contents,n);

	return n;
}

void cleanup_handles(struct http_data *data){
	int i;

	for(i=0;i<data->anp;i++){
		curl_multi_remove_handle(data->cm,data->ce[i]);
		curl_easy_cleanup(data->ce[i]);
		if(data->xh[i].doc){
			xmlproc_free_doc(data->xh[i].doc);
			data->xh[i].doc=NULL;
		}
	}

	curl_multi_cleanup(data->cm);
	data->cm=NULL;
	data->anp=0;

	free(data->ula);
	free(data->ce);
	free(data->xh);

	/* globals */
	xmlproc_cleanup_global();
	curl_global_cleanup();
	curl_global_init(CURL_GLOBAL_ALL);
}

struct timeval* http_get_timeout(void *d){
	struct http_data *data=(struct http_data*)d;
	long curl_timeo = -1;

	if(data->cm==NULL)
		return NULL;

	curl_multi_timeout(data->cm, &curl_timeo);

	if(curl_timeo >= 0) {
		data->timeout.tv_sec = curl_timeo / 1000;
		if(data->timeout.tv_sec > 1)
			data->timeout.tv_sec = 1;
		else
			data->timeout.tv_usec = (curl_timeo % 1000) * 1000;
	}

	return &data->timeout;
}

static int process_multi_group(struct http_data *data){
	CURLMcode mc;

	do{
		mc=curl_multi_perform(data->cm,&data->rnp);
	}while(mc==CURLM_CALL_MULTI_PERFORM);

	return data->rnp==0;
}

void* http_init(){
	struct http_data *data;
	curl_global_init(CURL_GLOBAL_ALL);
	INIT_MEM(data,1);
	memset(data,0,sizeof(*data));
	return data;
}

void http_destroy(void *d){
	struct http_data *data=(struct http_data*)d;
	if(data)
		free(data);
	curl_global_cleanup();
}


void http_fetch_init(struct urllist *ul, void *d){
	struct http_data *data=(struct http_data*)d;
	int npara=5;
	struct urllist *tul;

	if(global_config.parallel_reload>0)
		data->npara=npara=global_config.parallel_reload;

	data->cm=curl_multi_init();

	INIT_MEM(data->xh,npara);
	INIT_MEM(data->ce,npara);
	INIT_MEM(data->ula,npara);

	for(tul=ul;tul;tul=tul->next)
		tul->data.n_httperr=-1;

	data->anp=0;
	data->rnp=0;
}

int http_get_fds(fd_set *fdr, fd_set *fdw, fd_set *fde, void *d){
	struct http_data *data=(struct http_data*)d;
	int maxfd;
	int ret;

	if(data->cm==NULL)
		return -2;

	if((ret=curl_multi_fdset(data->cm,fdr,fdw,fde,&maxfd))){
		return -3;
	}

	return maxfd;
}

void finish_batch(struct http_data *data, void *db){
	int i;
	CURLMsg *msg; /* for picking up messages with the transfer status */
	int msgs_left; /* how many messages are left */

	while ((msg = curl_multi_info_read(data->cm, &msgs_left))) {
		if (msg->msg == CURLMSG_DONE) {
			int idx, found = 0;
			//char na[2]={0,0};

			/* Find out which handle this message is about */
			for (idx=0; idx<data->anp; idx++) {
				found = (msg->easy_handle == data->ce[idx]);
				if(found)
					break;
			}

			if(msg->data.result!=CURLE_OK){
				if(msg->data.result==CURLE_OPERATION_TIMEDOUT){
					if(data->ula[idx]->data.n_httperr<0)
						data->ula[idx]->data.n_httperr=1;
					else
						data->ula[idx]->data.n_httperr++;
				}
				else{
					debug_print("libcurl error (%d) on url [try %d/%d]: %s\n",msg->data.result,data->ula[idx]->data.n_httperr,global_config.dl_retries,data->ula[idx]->data.url);
					data->ula[idx]->data.n_httperr=global_config.dl_retries;
				}
				xmlproc_cleanup(data->xh+idx);
				continue;
			}

			if(xmlproc_finish(data->xh+idx,db)){
				if(data->ula[idx]->data.n_httperr<0)
					data->ula[idx]->data.n_httperr=1;
				else
					data->ula[idx]->data.n_httperr++;
			}
			//ula[idx]->data.doc=xh[idx].doc;
			//ula[idx]->data.info.feedid=xh[idx].feedid;
		}
	}

	for(i=0;i<data->anp;i++){
		curl_multi_remove_handle(data->cm,data->ce[i]);
		curl_easy_cleanup(data->ce[i]);
	}

}

int process_handles(struct http_data *data){
	return process_multi_group(data);
}

void load_new_handles(struct urllist *ul, struct http_data *data){
	struct urllist *tul;
	int i;

	tul=ul;
	data->anp=0;
	for(i=0;i<data->npara;i++){
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

		data->ula[i]=tul;
		if(tul->data.info.doc){
			xmlproc_free_doc(tul->data.info.doc);
			tul->data.info.doc=NULL;
		}

		xmlproc_init(data->xh+i,tul);

		data->ce[i]=curl_easy_init();

		/* send all data to this function  */
		curl_easy_setopt(data->ce[i],CURLOPT_WRITEFUNCTION,write_memory_cb);
		curl_easy_setopt(data->ce[i],CURLOPT_WRITEDATA,(void *)(data->xh+i));
		curl_easy_setopt(data->ce[i],CURLOPT_USERAGENT,"newsbaoter/0.1");
		curl_easy_setopt(data->ce[i],CURLOPT_FOLLOWLOCATION,1);
		curl_easy_setopt(data->ce[i],CURLOPT_URL,tul->data.url);
		if(global_config.proxy)
			curl_easy_setopt(data->ce[i],CURLOPT_PROXY,global_config.proxy);

		curl_multi_add_handle(data->cm,data->ce[i]);

		tul=tul->next;
		data->anp++;
	}

	if(data->anp==0)
		goto empty;

	return;

empty:
	cleanup_handles(data);
}

int handle_urls(struct urllist *ul, void *d, void *db){
	struct http_data *data=(struct http_data*)d;

	if(data->cm==NULL){
		http_fetch_init(ul,d);
		load_new_handles(ul,data);
	}

	if(process_handles(data)){
		finish_batch(data,db);
		load_new_handles(ul,data);
	}

	return data->cm==NULL;
}
