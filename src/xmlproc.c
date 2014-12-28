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

#define RSS_XSL "../xsl/RSS_20_xsl.xml"
#define ATOM03_XSL "../xsl/Atom_03_xsl.xml"
#define ATOM10_XSL "../xsl/Atom_10_xsl.xml"
#define RDF_XSL "../xsl/RDF_xsl.xml"

char* get_spec_sheet(xmlDocPtr doc){
	xmlNode *root = xmlDocGetRootElement(doc);

	if(strcmp((const char*)root->name,"feed")==0){
		char *ret=ATOM10_XSL;
		xmlChar *a=xmlGetProp(root,(xmlChar*)"version");
		if(a){
			if(strcmp((const char*)a,"0.3")==0)
				ret=ATOM03_XSL;
			xmlFree(a);
		}
		return ret;
	}
	else if(strcmp((const char*)root->name,"RDF")==0)
		return RDF_XSL;

	return RSS_XSL;
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

int write_feeds_line(tchar_t *line, int len, xmlNode *root){
	xmlNode *cur_node = NULL;

	for(cur_node=root->children;cur_node;cur_node=cur_node->next){
		if(cur_node->type==XML_ELEMENT_NODE && strcmp((const char*)cur_node->name,"title")==0){
			chars_to_widechars(line,(const char*)cur_node->children->content,len);
			return 0;
		}
	}

	return 1;
}

int set_standard_time(tchar_t *line, int len, xmlChar *tstr){
	const size_t tsize=256;
	char tmp[tsize];
	struct tm tp;
	//strptime((const char*)cur_node->children->content,"%a, %d %b %y %T",&tp);
	strptime((const char*)tstr,"%F %T",&tp);
	strftime(tmp,tsize,"%d %b  ",&tp);
	return chars_to_widechars(line,tmp,len);
}

int write_entries_line(tchar_t *line, int len, xmlNode *root, int id){
	xmlNode *cur_node = NULL;
	int off=0;
	int entid=0;

	/*TODO: abstract node finding */

	for(cur_node=root->children;cur_node;cur_node=cur_node->next){
		if(cur_node->type==XML_ELEMENT_NODE && strcmp((const char*)cur_node->name,"entry")==0){
			if(entid==id){
				break;
			}
			entid++;
		}
	}

	if(cur_node==NULL)
		return 1;

	root=cur_node;

	for(cur_node=root->children;cur_node;cur_node=cur_node->next){
		if(cur_node->type==XML_ELEMENT_NODE && strcmp((const char*)cur_node->name,"publishDateTime")==0){
			off+=set_standard_time(line+off,len-off,cur_node->children->content);
			break;
		}
	}

	if(cur_node==NULL){
		for(cur_node=root->children;cur_node;cur_node=cur_node->next){
			if(cur_node->type==XML_ELEMENT_NODE && strcmp((const char*)cur_node->name,"lastUpdatedDateTime")==0){
				off+=set_standard_time(line+off,len-off,cur_node->children->content);
				break;
			}
		}
	}

	for(cur_node=root->children;cur_node;cur_node=cur_node->next){
		if(cur_node->type==XML_ELEMENT_NODE && strcmp((const char*)cur_node->name,"title")==0){
			off+=chars_to_widechars(line+off,(const char *)cur_node->children->content,len-off);
			break;
		}
	}

	return 0;
}

int write_line(tchar_t *line, int len, xmlDocPtr xd, int type, int id, int index){
	xmlNode *root = xmlDocGetRootElement(xd);
	int off=0;
	const size_t tsize=256;
	char tmp[tsize];
	int r;

	line[0]=0;

	if(root==NULL){
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

void xmlproc_gen_lines(void *uld, struct mainwindow *mw){
	int i,r;
	int sn=mw->page*mw->body_len;
	struct urllist *ul = (struct urllist*)uld;

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

	for(r=i=0;i<mw->body_len && ul && !r;i++){
		r=write_line(mw->data.lv[i].line,mw->width,ul->data.doc,mw->ctx_type,mw->ctx_id,i);
		if(mw->ctx_type==CTX_FEEDS){
			ul=ul->next;
			r=0;
		}
	}

	for(;i<mw->body_len;i++)
		mw->data.lv[i].line[0]=0;
}
