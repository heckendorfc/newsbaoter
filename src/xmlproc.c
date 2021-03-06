/* Copyright 2015 Christian Heckendorf.  All rights reserved.
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file.
 */

#ifdef __linux__
#define _XOPEN_SOURCE 500
#endif
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <libxml/xmlmemory.h>
#include <libxml/debugXML.h>
#include <libxml/HTMLtree.h>
#include <libxml/xmlIO.h>
#include <libxml/parser.h>
#include <libxslt/xslt.h>
#include <libxml/xinclude.h>
#include <libxml/catalog.h>
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

#include "common_defs.h"
#include "xmlproc.h"
#include "ui/view.h"
#include "urlparse.h"
#include "sql/cache.h"

static const char *entry_fields[]={
	"title",
	"publishDateTime",
	"lastUpdatedDateTime",
	"url",
	"authorName",
	"authorEmail",
	"description",
	"content",
	"entryUniqueId",
	"",
};
#define NENTRYFIELD EFI_PUBLIC_DONE

static char *feed_fields[]={
	"title",
	"url",
	"description",
	"authorName",
	"authorEmail",
	"imageUrl",
	"imageText",
	"",
};
#define NFEEDFIELD FFI_PUBLIC_DONE

static int xmlproc_update_cache(struct xmlproc_data *h, sqlite3 *db);

/* Parsing XML text into data structure */

void xmlproc_parse_block(struct xmlproc_data *h, char *buf, size_t n){
	if(h->ctx==NULL){
		h->ctx=xmlCreatePushParserCtxt(NULL,NULL,buf,n,NULL);
		return;
	}
	xmlParseChunk(h->ctx,buf,n,0);
}

void xp_outf(xmlDocPtr doc,char *fn){
	FILE *fp=fopen(fn,"w");
	xmlDocDump(fp,doc);
}

void xmlproc_free_doc(xmlDocPtr doc){
	xmlFreeDoc(doc);

}

#ifndef SHARE_PATH
#define SHARE_PATH "/tmp"
#endif

#define RSS_20_XSL "RSS_20_xsl.xml"
#define RSS_09_XSL "RSS_09_xsl.xml"
#define ATOM03_XSL "Atom_03_xsl.xml"
#define ATOM10_XSL "Atom_10_xsl.xml"
#define RDF_XSL "RDF_xsl.xml"
#define XSS(x) SHARE_PATH "/xsl/" x

char* get_spec_sheet(xmlDocPtr doc){
	xmlNode *root = xmlDocGetRootElement(doc);

	if(root && root->name && strcmp((const char*)root->name,"feed")==0){
		char *ret=XSS(ATOM10_XSL);
		xmlChar *a=xmlGetProp(root,(xmlChar*)"version");
		if(a){
			if(strcmp((const char*)a,"0.3")==0)
				ret=XSS(ATOM03_XSL);
			xmlFree(a);
		}
		return ret;
	}
	else if(root && root->name && strcmp((const char*)root->name,"RDF")==0)
		return XSS(RDF_XSL);
	else if(root && root->name && strcmp((const char*)root->name,"rss")==0){
		char *ret=XSS(RSS_09_XSL);
		xmlChar *a=xmlGetProp(root,(xmlChar*)"version");
		if(a){
			if(strcmp((const char*)a,"2.0")==0)
				ret=XSS(RSS_20_XSL);
			xmlFree(a);
		}
		return ret;
	}

	return XSS(RSS_09_XSL);
}

static void xmlproc_transform(struct xmlproc_data *h){
	xmlDocPtr res;
	h->ss = xsltParseStylesheetFile((const xmlChar*)get_spec_sheet(h->doc));
	res = xsltApplyStylesheet(h->ss,h->doc,NULL);
	xmlFreeDoc(h->doc);
	h->doc=res;
	xsltFreeStylesheet(h->ss);
}

void xmlproc_cleanup_global(){
	xmlCleanupParser();
}

void xmlproc_cleanup(struct xmlproc_data *h){
	if(h->ctx){
		xmlFreeParserCtxt(h->ctx);
		h->ctx=NULL;
	}
	if(h->doc){
		xmlFreeDoc(h->doc);
		h->doc=NULL;
	}
}

int xmlproc_finish(struct xmlproc_data *h, sqlite3 *db, int *newentry){
	int res;
	int ret=0;

	*newentry=0;

	if(!h->ctx)
		return 1;

	if(xmlParseChunk(h->ctx,NULL,0,1)){
		ret=1;
		goto done;
	}

	h->doc=h->ctx->myDoc;
	res = h->ctx->wellFormed;

	if(!res){
		debug_print("Failed to parse\n");
		ret=1;
		goto done;
	}

	xmlproc_transform(h);

	*newentry=xmlproc_update_cache(h,db);

done:
	xmlFreeParserCtxt(h->ctx);
	h->ctx=NULL;
	xmlFreeDoc(h->doc);
	h->doc=NULL;

	return ret;
}

void xmlproc_err_handler(void *ctx, const char *msg, ...){
	va_list args;

	if(!debugfd)
		return;

	va_start(args, msg);
	vfprintf(debugfd, msg, args);
	va_end(args);
}

void xmlproc_global_init(){
	xmlGenericErrorFunc handler=xmlproc_err_handler;
	initGenericErrorDefaultFunc(&handler);
}

void xmlproc_init(struct xmlproc_data *h, void *ul){
	struct urllist *tul=(struct urllist*)ul;
	memset(h,0,sizeof(*h));
	h->url=tul->data.url;
}

/* TODO: Probably delete this next section */

#if 0
static struct urllist* get_urllist_start(struct urllist *ul, struct mainwindow *mw){
	int sn=mw->page*mw->body_len;

	/* skip down to the current page */
	while(ul && sn){
		sn--;
		ul=ul->next;
	}

	if(mw->ctx_type==CTX_ENTRIES){
		sn=mw->ctx_id;
		/* skip down to the current id */
		while(ul && sn){
			sn--;
			ul=ul->next;
		}
	}

	return ul;
}

xmlNode* get_nth_entry(xmlNode *cur_node, int id){
	int entid=0;

	for(;cur_node;cur_node=cur_node->next){
		if(cur_node->type==XML_ELEMENT_NODE && strcmp((const char*)cur_node->name,"entry")==0){
			if(entid==id){
				return cur_node;
			}
			entid++;
		}
	}

	return NULL;
}

static void write_elem_if_avail(char *name, xmlNode *node, int fd){
	node=find_xml_node(node,name,XML_ELEMENT_NODE);
	if(node && node->children && node->children->content && node->children->content[0]){
		const char *s = (const char*)node->children->content;
		write(fd,name,strlen(name));
		write(fd,": ",2);
		write(fd,s,strlen(s));
		write(fd,"\n",1);
	}
}


int xmlproc_write_entry(void *uld, struct mainwindow *mw, int id, int fd){
	xmlNode *n;
	struct urllist *ul=get_urllist_start(uld,mw);
	xmlNode *r;
	int i;

	r=xmlDocGetRootElement(ul->data.doc);
	n=r->children;

	n=get_nth_entry(n,id);
	if(n)
		n=n->children;
	else
		return 1;

	for(i=0;entry_fields[i][0];i++){
		write_elem_if_avail(entry_fields[i],n,fd);
	}

	return 0;
}

static int write_feeds_line(tchar_t *line, int len, xmlNode *root){
	xmlNode *cur_node = NULL;

	cur_node=find_xml_node(root->children,"title",XML_ELEMENT_NODE);
	if(cur_node && cur_node->children){
		chars_to_widechars(line,(const char*)cur_node->children->content,len);
		return 0;
	}

	return 1;
}

static int set_standard_time(tchar_t *line, int len, xmlChar *tstr){
	const size_t tsize=256;
	char tmp[tsize];
	struct tm tp;

	if(tstr==NULL){
		return chars_to_widechars(line,"        ",len);
	}

	strptime((const char*)tstr,"%F %T",&tp);
	strftime(tmp,tsize,"%d %b  ",&tp);

	return chars_to_widechars(line,tmp,len);
}

static int write_entries_line(tchar_t *line, int len, xmlNode *root, int id){
	xmlNode *cur_node = NULL;
	int off=0;

	cur_node=get_nth_entry(root->children,id);

	if(cur_node==NULL)
		return 1;

	root=cur_node;

	cur_node=find_xml_node(root->children,"publishDateTime",XML_ELEMENT_NODE);
	if(cur_node && cur_node->children){
		off+=set_standard_time(line+off,len-off,cur_node->children->content);
	}
	else{
		cur_node=find_xml_node(root->children,"lastUpdatedDateTime",XML_ELEMENT_NODE);
		if(cur_node && cur_node->children)
			off+=set_standard_time(line+off,len-off,cur_node->children->content);
		else
			off+=set_standard_time(line+off,len-off,NULL);
	}

	cur_node=find_xml_node(root->children,"title",XML_ELEMENT_NODE);
	if(cur_node && cur_node->children){
		off+=chars_to_widechars(line+off,(const char *)cur_node->children->content,len-off);
	}

	return 0;
}

static int write_line(tchar_t *line, int len, xmlDocPtr xd, int type, int id, int index){
	xmlNode *root = xmlDocGetRootElement(xd);
	int off=0;
	const size_t tsize=256;
	char tmp[tsize];
	int r;

	line[0]=0;

	if(root==NULL || root->children==NULL){
		return 1;
	}

	snprintf(tmp,tsize,"%3d  ",index+1);
	off=chars_to_widechars(line,tmp,len);

	if(type==CTX_ENTRIES){
		r=write_entries_line(line+off,len-off,root,index);
	}
	else{
		r=write_feeds_line(line+off,len-off,root);
	}

	if(r)
		line[0]=0;

	return r;
}

static void set_title_string(struct urllist *ul, struct mainwindow *mw, char *tail){
	const int tlen=256;
	char tmp[tlen];
	const char *head="";
	xmlNode *root = xmlDocGetRootElement(ul->data.doc);
	xmlNode *node;

	if(root==NULL || root->children==NULL)
		return;

	node=find_xml_node(root->children,"title",XML_ELEMENT_NODE);

	if(mw->ctx_type==CTX_FEEDS)
		head="Your Feeds";
	else if(node && node->children && node->children->content && node->children->content[0])
		head=(const char*)node->children->content;

	snprintf(tmp,tlen,"%s %s",head,tail);

	chars_to_widechars(mw->header,tmp,mw->width);
}

int xmlproc_gen_lines(void *uld, struct mainwindow *mw){
	int i,r;
	const int tlen=256;
	char tmp[tlen];
	struct urllist *ul = get_urllist_start(uld,mw);

	if(ul==NULL)
		return 1;

	snprintf(tmp,tlen,"(Page %d)",mw->page+1);
	set_title_string(ul,mw,tmp);

	for(r=i=0;i<mw->body_len && ul && !r;i++){
		r=write_line(mw->data.lv[i].line,mw->width,ul->data.doc,mw->ctx_type,mw->ctx_id,i);
		if(mw->ctx_type==CTX_FEEDS){
			if(r){
				snprintf(tmp,tlen,"%3d  (F) %s",i+1,ul->data.url);
				chars_to_widechars(mw->data.lv[i].line,tmp,mw->width);
			}
			ul=ul->next;
			r=0;
		}
	}

	for(;i<mw->body_len;i++)
		mw->data.lv[i].line[0]=0;

	return 0;
}
#endif

/* Translating XML data structure to SQL */

static xmlNode* find_xml_node(xmlNode *cur_node, const char *name, int type){
	for(;cur_node;cur_node=cur_node->next){
		if(cur_node->type==type && strcmp((const char*)cur_node->name,name)==0)
			return cur_node;
	}
	return NULL;
}

static const char* get_content_if_avail(xmlNode *node,const char *str){
	node=find_xml_node(node,str,XML_ELEMENT_NODE);
	if(node && node->children){
		return (const char*)node->children->content;
	}
	return NULL;
}

static int update_feed(xmlNode *root, struct xmlproc_data *h, sqlite3 *db){
	const char *fi[NFEEDFIELD];
	int i;

	for(i=0;i<NFEEDFIELD;i++)
		fi[i]=get_content_if_avail(root->children,feed_fields[i]);

	fi[FFI_URL]=h->url;

	h->feedid=cache_update_feed(db,fi);

	return 0;
}

static int update_entry(xmlNode *root, struct xmlproc_data *h, sqlite3 *db){
	const char *ei[NENTRYFIELD];
	int i;
	char pdate[20];
	char udate[20];
	char tmpstr[100];
	char tmptitle[50];
	struct tm tp;

	for(i=0;i<NENTRYFIELD;i++)
		ei[i]=get_content_if_avail(root->children,entry_fields[i]);

	udate[0]=pdate[0]=0;
	if(ei[EFI_PDATE]){
		strptime(ei[EFI_PDATE],"%F %T %z",&tp);
		strftime(pdate,20,"%s",&tp);
		ei[EFI_PDATE]=pdate;
	}
	if(ei[EFI_UDATE]){
		strptime(ei[EFI_UDATE],"%F %T %z",&tp);
		strftime(udate,20,"%s",&tp);
		ei[EFI_UDATE]=udate;
		if(!ei[EFI_PDATE])
			ei[EFI_PDATE]=udate;
	}

	if(!ei[EFI_UID]){
		cache_get_new_uid(h->feedid,tmpstr,100,db);
		ei[EFI_UID]=tmpstr;
	}

	if(!ei[EFI_TITLE]){
		ei[EFI_TITLE]=tmptitle;
		if(ei[EFI_DESC]){
			strncpy(tmptitle,ei[EFI_DESC],49);
		}
		else if(ei[EFI_CONTENT]){
			strncpy(tmptitle,ei[EFI_CONTENT],49);
		}
		else{
			ei[EFI_TITLE]=ei[EFI_UID];
		}
		tmptitle[49]=0;
	}

	cache_update_entry(db,h->feedid,ei);

	return 0;
}

static int update_entries(xmlNode *root, struct xmlproc_data *h, sqlite3 *db){
	xmlNode *cur_node=root->children;

	cache_init_cleanup(db,h->feedid);

	for(;cur_node;cur_node=cur_node->next){
		if(cur_node->type==XML_ELEMENT_NODE && strcmp((const char*)cur_node->name,"entry")==0){
			update_entry(cur_node,h,db);
		}
	}

	cache_cleanup_old(db,h->feedid);

	return 0;
}

static int xmlproc_update_cache(struct xmlproc_data *h, sqlite3 *db){
	int n_unr[2];
	xmlNode *root = xmlDocGetRootElement(h->doc);

	if(root==NULL)
		return 0;

	n_unr[0]=get_num_unread(db);
	update_feed(root,h,db);
	update_entries(root,h,db);
	n_unr[1]=get_num_unread(db);

	xmlproc_free_doc(h->doc);
	h->doc=NULL;

	if(n_unr[0]<n_unr[1]){
		return 1;
	}
	return 0;
}
