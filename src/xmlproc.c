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

/*TODO find feed type */
#define RSS_XSL "../xsl/RSS_20_xsl.xml"
static void xmlproc_transform(struct xmlproc_data *h){
	xmlDocPtr res;
	h->ss = xsltParseStylesheetFile((const xmlChar*)RSS_XSL);
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

/*
	xmlDocPtr doc, res;
	xsltStylesheetPtr cur;
	//const char *params[16 + 1];
	const char *params=NULL;

	xmlSubstituteEntitiesDefault(1);
	xmlLoadExtDtdDefaultValue=1;

	cur = xsltParseStylesheetFile((const xmlChar *)stylesheet);
	doc = xmlParseFile(xmlfile);
	res = xsltApplyStylesheet(cur, doc, &params);

	xsltSaveResultToFile(stdout, res, cur);

	// Cleanup
	xsltFreeStylesheet(cur);
	xmlFreeDoc(res);
	xmlFreeDoc(doc);

	xsltCleanupGlobals();
	xmlCleanupParser();
}
*/
