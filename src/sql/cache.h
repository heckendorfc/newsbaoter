/* Copyright 2015 Christian Heckendorf.  All rights reserved.
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file.
 */

#ifndef NBCACHE_H
#define NBCACHE_H

#include <sqlite3.h>
#include <stdarg.h>

#include "../ui/ui_common.h"

/* Number of repeated refresh cycles with the entry missing before it gets deleted */
#define CYCLES_UNTIL_DELETE 2

#define valstr(x) ppstr(x)
#define ppstr(x) #x

typedef sqlite3_int64 rowid_t;

enum pubentry_enum{
	PUBENTRY_EID=0,
	PUBENTRY_FID,
	PUBENTRY_TITLE,
	PUBENTRY_DATE,
	PUBENTRY_URL,
	PUBENTRY_ANAME,
	PUBENTRY_AEMAIL,
	PUBENTRY_DESC,
	PUBENTRY_CONTENT,
};

int nb_qnprintf(char *s, int n, char *fmt, ...);
int nb_sqlite3_exec(sqlite3 *connection, const char *sql, int (*callback)(void*,int,char**,char**), void *arg, char **errmsg);
sqlite3* init_db();
void cache_init_cleanup(sqlite3 *conn, rowid_t feedid);
void cache_cleanup_old(sqlite3 *conn, rowid_t feedid);
rowid_t cache_update_feed(sqlite3 *conn, const char **data);
rowid_t cache_update_entry(sqlite3 *conn, rowid_t feedid, const char **data);
int cache_gen_lines(struct mainwindow *mw, sqlite3 *db);
int cache_write_entry(struct mainwindow *mw, int id, int fd, sqlite3 *db);
void cache_toggle_read_entry(sqlite3 *conn, rowid_t id, int v);
void cache_read_feed(sqlite3 *conn, rowid_t feedid);
int get_num_unread(sqlite3 *db);
int cache_next_unread(struct mainwindow *mw, int id, sqlite3 *db);
int cache_get_new_uid(int feedid, char *dst, int size, sqlite3 *db);

#endif
