#ifndef NBXMLPROC_H
#define NBXMLPROC_H

#include <libxml/parser.h>
#include <libxslt/xslt.h>
#include <libxslt/transform.h>

struct xmlproc_data{
	xmlParserCtxtPtr ctx;
	xmlDocPtr doc;
	xsltStylesheetPtr ss;
};

void xp_outf(xmlDocPtr doc,char *fn);
void xmlproc_free_doc(xmlDocPtr doc);
void xmlproc_parse_block(struct xmlproc_data *h, char *buf, size_t n);
void xmlproc_finish(struct xmlproc_data *h);
void xmlproc_init(struct xmlproc_data *h);

#endif
