#ifndef NBCONFIG_H
#define NBCONFIG_H

struct conf_read{
	char *buf;
	int b_alloc;
	int b_stored;
	int l_ret;
	int eof;
};

int open_config(const char *file);
int read_config_line(int fd, struct conf_read *c);
void clean_conf_read(struct conf_read *c);
char* get_line_word(char *buf, char **next);

#endif
