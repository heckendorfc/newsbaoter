#ifndef NBCACHE_H
#define NBCACHE_H

#include <sqlite3.h>
#include <stdarg.h>

#include "../ui/ui_common.h"

typedef sqlite3_int64 rowid_t;

int nb_qnprintf(char *s, int n, char *fmt, ...);
int nb_sqlite3_exec(sqlite3 *connection, const char *sql, int (*callback)(void*,int,char**,char**), void *arg, char **errmsg);
sqlite3* init_db();
void cache_init_cleanup(sqlite3 *conn);
void cache_cleanup_old(sqlite3 *conn);
rowid_t cache_update_feed(sqlite3 *conn, const char **data);
rowid_t cache_update_entry(sqlite3 *conn, rowid_t feedid, const char **data);
int cache_gen_lines(void *uld, struct mainwindow *mw, sqlite3 *db);
int cache_write_entry(void *uld, struct mainwindow *mw, int id, int fd, sqlite3 *db);
void cache_toggle_read_entry(sqlite3 *conn, rowid_t id, int v);
void cache_read_feed(sqlite3 *conn, rowid_t feedid);
int get_num_unread(sqlite3 *db);
int cache_next_unread(struct mainwindow *mw, int id, sqlite3 *db);

#endif
