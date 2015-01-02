#ifndef NBXMLPROC_H
#define NBXMLPROC_H

#include <libxml/parser.h>
#include <libxslt/xslt.h>
#include <libxslt/transform.h>

#include "ui/view.h"
#include "sql/cache.h"
#include "urlparse.h"

struct xmlproc_data{
	xmlParserCtxtPtr ctx;
	xmlDocPtr doc;
	xsltStylesheetPtr ss;
	rowid_t feedid;
	char *url;
};

enum feed_field_ind{
	FFI_TITLE=0,
	FFI_URL,
	FFI_DESC,
	FFI_AUTHNAME,
	FFI_AUTHEMAIL,
	FFI_IURL,
	FFI_ITEXT,
	FFI_PUBLIC_DONE
};

enum entry_field_ind{
	EFI_TITLE=0,
	EFI_PDATE,
	EFI_UDATE,
	EFI_URL,
	EFI_AUTHNAME,
	EFI_AUTHEMAIL,
	EFI_DESC,
	EFI_CONTENT,
	EFI_UID,
	EFI_PUBLIC_DONE,
};

void xmlproc_cleanup(struct xmlproc_data *h);
void xp_outf(xmlDocPtr doc,char *fn);
void xmlproc_free_doc(xmlDocPtr doc);
void xmlproc_parse_block(struct xmlproc_data *h, char *buf, size_t n);
int xmlproc_finish(struct xmlproc_data *h, sqlite3 *db);
void xmlproc_init(struct xmlproc_data *h, void *ul);
int xmlproc_write_entry(void *uld, struct mainwindow *mw, int id, int fd);
int xmlproc_gen_lines(void *uld, struct mainwindow *mw);

#endif
