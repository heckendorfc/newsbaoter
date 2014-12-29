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
	int color;
	int attr;
}nbcolor_t;

enum nbcolor_cp{
	CP_LISTNORMAL=0,
	CP_LISTFOCUS,
	CP_LISTNORMAL_UR,
	CP_LISTFOCUS_UR,
	CP_INFO,
	CP_ARTICLE,
	CP_BACKGROUND,
	CP_NULL,
};

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
	nbcolor_t colors[CP_NULL];
};

extern struct nb_config global_config;

int open_config(const char *file);
int read_config_line(int fd, struct conf_read *c);
void clean_conf_read(struct conf_read *c);
char* get_line_word(char *buf, char **next);
char* get_config_file(const char *file);
void read_config_options_file();

#endif
