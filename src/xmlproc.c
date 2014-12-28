#include <unistd.h>
#include <time.h>
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

#include "xmlproc.h"
#include "ui/view.h"
#include "urlparse.h"

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

#define RSS_XSL "RSS_20_xsl.xml"
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

	return XSS(RSS_XSL);
}

static void xmlproc_transform(struct xmlproc_data *h){
	xmlDocPtr res;
	h->ss = xsltParseStylesheetFile((const xmlChar*)get_spec_sheet(h->doc));
	res = xsltApplyStylesheet(h->ss,h->doc,NULL);
	xmlFreeDoc(h->doc);
	h->doc=res;
	xsltFreeStylesheet(h->ss);
}

void xmlproc_finish(struct xmlproc_data *h){
	int res;

	if(!h->ctx)
		return;

	xmlParseChunk(h->ctx,NULL,0,1);

	h->doc=h->ctx->myDoc;
	res = h->ctx->wellFormed;

	xmlFreeParserCtxt(h->ctx);
	h->ctx=NULL;

	if(!res){
		fprintf(stderr, "Failed to parse\n");
		return;
	}

	xmlproc_transform(h);
}

void xmlproc_init(struct xmlproc_data *h){
	memset(h,0,sizeof(*h));
}

static xmlNode* find_xml_node(xmlNode *cur_node, char *name, int type){
	for(;cur_node;cur_node=cur_node->next){
		if(cur_node->type==type && strcmp((const char*)cur_node->name,name)==0)
			return cur_node;
	}
	return NULL;
}

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

static char entry_fields[][30]={
	"title",
	"publishDateTime",
	"lastUpdatedDateTime",
	"url",
	"authorName",
	"authorEmail",
	"description",
	"content",
	"",
};

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

	sprintf(tmp,"%3d  ",index+1);
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
			ul=ul->next;
			r=0;
		}
	}

	for(;i<mw->body_len;i++)
		mw->data.lv[i].line[0]=0;

	return 0;
}
