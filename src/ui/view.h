#ifndef NBUIVIEW_H
#define NBUIVIEW_H

#include <wchar.h>

typedef wchar_t tchar_t;
//typedef unsigned int tchar_t;

enum viewtype{
	NBVT_LIST,
	NBVT_PAGER,
};

enum listcontext{
	CTX_NULL,
	CTX_FEEDS,
	CTX_ENTRIES,
};

struct listview{
	tchar_t *line;
};

struct pagerview{

};

struct mainwindow{
	tchar_t *header;
	tchar_t *footer;
	tchar_t *body;
	int body_type;
	int body_len;
	int width;
	int page;
	int ctx_type;
	int ctx_id;
	union{
		struct listview *lv;
		struct pagerview *pv;
	}data;
};

int chars_to_widechars(tchar_t *dst, const char *src, int max);
void update_view(struct mainwindow *mw);
struct mainwindow* setup_ui();
void run_ui();
void end_ui();

#endif
