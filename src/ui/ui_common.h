#ifndef NBUI_COMMON_H
#define NBUI_COMMON_H

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
	CTX_ENTRY,
};

struct listview{
	tchar_t *line;
	int unread;
	int id;
};

struct pagerview{

};

typedef int ipcinfo;
enum ipcinfo_val{
	IPCVAL_UPDATE_REQUEST,
	IPCVAL_WRITE_ENTRY,
	IPCVAL_REFRESH_ALL,
	IPCVAL_READ_FEED,
	IPCVAL_TOGGLE_READ,
	IPCVAL_DONE,
	IPCVAL_EOL,
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
	int outfd[2];
	int infd[2];
	int beep_request;
	union{
		struct listview *lv;
		struct pagerview *pv;
	}data;
};

enum hk_ret{
	KH_RET_NO_HANDLER,
	KH_RET_OK,
	KH_RET_EXIT,
	KH_RET_UPDATE,
};

typedef int(*handler_t)(struct mainwindow *mw);

#endif
