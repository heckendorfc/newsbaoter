/* Copyright 2015 Christian Heckendorf.  All rights reserved.
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file.
 */

#ifndef NBCONFIG_H
#define NBCONFIG_H

struct conf_read{
	char *buf;
	int b_alloc;
	int b_stored;
	int l_ret;
	int eof;
};

typedef struct nbcolor_t{
	unsigned int color;
	unsigned int attr;
	char set;
}nbcolor_t;

enum nbcolor_cp{
	CP_NOTACOLOR=0,
	CP_LISTNORMAL=1,
	CP_LISTFOCUS,
	CP_LISTNORMAL_UR,
	CP_LISTFOCUS_UR,
	CP_INFO,
	CP_ARTICLE,
	CP_BACKGROUND,
	CP_NULL,
	CP_ALERT,
};
#define FII_STYLE(base,i) (base+((mw->data.lv && mw->data.lv[i].unread)?2:0))
#define FI_STYLE(base) FII_STYLE(base,cursor_i)
#define FB_STYLE FII_STYLE((i!=cursor_i?CP_LISTNORMAL:CP_LISTFOCUS),i)

struct nb_config{
	char *proxy; /* "socks5://localhost:1234" */
	int dl_retries;
	int auto_reload;
	int reload_time;
	int show_read_feeds;
	int show_read_entries;
	int parallel_reload;
	int confirm_exit;
	int notify_beep;
	int entry_retention;
	char **pager;
	char **downloader;
	int html_pager;
	nbcolor_t colors[CP_NULL];
};

extern struct nb_config global_config;

int open_config(const char *file);
int read_config_line(int fd, struct conf_read *c);
void clean_conf_read(struct conf_read *c);
char* get_line_word(char *buf, char **next);
char* get_config_file(const char *file);
void read_config_options_file();
void set_nbcolor(nbcolor_t *cd, unsigned int cfi, unsigned int cbi, unsigned int attr);
void get_nbcolor(nbcolor_t *cd, unsigned int *cfi, unsigned int *cbi, unsigned int *attr);

#endif
