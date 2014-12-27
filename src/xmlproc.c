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

void write_feeds_line(tchar_t *line, int len, xmlNode *root){
	xmlNode *cur_node = NULL;

	for(cur_node=root->children;cur_node;cur_node=cur_node->next){
		if(cur_node->type==XML_ELEMENT_NODE && strcmp((const char*)cur_node->name,"title")==0){
			chars_to_widechars(line,(const char*)cur_node->children->content,len);
			return;
		}
	}
}

void write_entries_line(tchar_t *line, int len, xmlNode *root, int id){
	xmlNode *cur_node = NULL;
	int off=0;
	int entid=0;
	const size_t tsize=256;
	char tmp[tsize];

	for(cur_node=root->children;cur_node;cur_node=cur_node->next){
		if(cur_node->type==XML_ELEMENT_NODE && strcmp((const char*)cur_node->name,"entry")==0){
			if(entid==id){
				break;
			}
			entid++;
		}
	}

	if(cur_node==NULL)
		return;

	root=cur_node;

	for(cur_node=root->children;cur_node;cur_node=cur_node->next){
		if(cur_node->type==XML_ELEMENT_NODE && strcmp((const char*)cur_node->name,"publishDateTime")==0){
			struct tm tp;
			strptime((const char*)cur_node->children->content,"%a, %d %b %y %T",&tp);
			strftime(tmp,tsize,"%d %b  ",&tp);
			off+=chars_to_widechars(line+off,tmp,len-off);
			break;
		}
	}

	for(cur_node=root->children;cur_node;cur_node=cur_node->next){
		if(cur_node->type==XML_ELEMENT_NODE && strcmp((const char*)cur_node->name,"title")==0){
			off+=chars_to_widechars(line+off,(const char *)cur_node->children->content,len-off);
			break;
		}
	}
}

void write_line(tchar_t *line, int len, xmlDocPtr xd, int type, int id, int index){
	xmlNode *root = xmlDocGetRootElement(xd);
	int off=0;
	const size_t tsize=256;
	char tmp[tsize];

	line[0]=0;

	if(root==NULL){
		return;
	}

	sprintf(tmp,"%d  ",index+1);
	off=chars_to_widechars(line,tmp,len);

	if(type==CTX_ENTRIES){
		write_entries_line(line+off,len-off,root,id);
	}
	else{
		write_feeds_line(line+off,len-off,root);
	}
}

void xmlproc_gen_lines(void *uld, struct mainwindow *mw){
	int i;
	int sn=mw->page*mw->body_len;
	struct urllist *ul = (struct urllist*)uld;

	/* skip down to the current page */
	while(ul && sn){
		sn--;
		ul=ul->next;
	}

	for(i=0;i<mw->body_len && ul;i++){
		write_line(mw->data.lv[i].line,mw->width,ul->data.doc,mw->ctx_type,mw->ctx_id,i);
		ul=ul->next;
	}

	for(;i<mw->body_len;i++)
		mw->data.lv[i].line[0]=0;
}
