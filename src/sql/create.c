/* Copyright 2015 Christian Heckendorf.  All rights reserved.
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file.
 */

#include <sqlite3.h>
#include <sys/stat.h>
#include <errno.h>

#include "cache.h"
#include "../config.h"
#include "../common_defs.h"
#include "../xmlproc.h"

int nb_sqlite3_exec(sqlite3 *connection, const char *sql, int (*callback)(void*,int,char**,char**), void *arg, char **errmsg){
	return sqlite3_exec(connection,sql,callback,arg,errmsg);
}

static int create_db(sqlite3 *conn){
	if(sqlite3_exec(conn,"CREATE TABLE CacheInfo(Version integer primary key)",NULL,NULL,NULL)!=SQLITE_OK ||
	sqlite3_exec(conn,"CREATE TABLE Feed(FeedID integer primary key, "
		"Title text, "
		"URL text, "
		"Description text, "
		"AuthorName text, "
		"AuthorEmail text, "
		"ImageURL text, "
		"ImageText text)",NULL,NULL,NULL)!=SQLITE_OK ||
	sqlite3_exec(conn,"CREATE TABLE Category(CategoryID integer primary key, "
		"Name text not null)",NULL,NULL,NULL)!=SQLITE_OK ||
	sqlite3_exec(conn,"CREATE TABLE FeedCategory(FeedCategoryID integer primary key, "
		"CategoryID integer, "
		"FeedID integer)",NULL,NULL,NULL)!=SQLITE_OK ||
	sqlite3_exec(conn,"CREATE TABLE Entry(EntryID integer primary key, "
		"FeedID integer, "
		"Viewed integer not null default 0, "
		"PubDate integer, "
		"ModDate integer, "
		"UniqueId text, "
		"Title text, "
		"URL text, "
		"AuthorName text, "
		"AuthorEmail text, "
		"Description text, "
		"Content text, "
		"Current integer default 1)",NULL,NULL,NULL)!=SQLITE_OK ||
	sqlite3_exec(conn,"CREATE VIEW PubEntry AS SELECT EntryID,FeedID,Title,datetime(PubDate,'unixepoch') AS Date,URL,AuthorName,AuthorEmail,Description,Content FROM Entry",NULL,NULL,NULL)!=SQLITE_OK)
		return 1;
	return 0;
}
#define ENT_TEXT_START 5

void cache_init_cleanup(sqlite3 *conn, rowid_t feedid){
	const int qlen=256;
	char query[qlen];

	nb_qnprintf(query,qlen,"UPDATE Entry SET Current=Current-1 WHERE FeedID=%ld",feedid);

	nb_sqlite3_exec(conn,query,NULL,NULL,NULL);
}

void cache_cleanup_old(sqlite3 *conn, rowid_t feedid){
	const int qlen=256;
	char query[qlen];

	nb_qnprintf(query,qlen,"DELETE FROM Entry WHERE FeedID=%ld AND Current<=-" valstr(CYCLES_UNTIL_DELETE) " AND Viewed>0 AND PubDate<strftime('%%s','now')-%d",feedid,global_config.entry_retention);

	nb_sqlite3_exec(conn,query,NULL,NULL,NULL);
}

rowid_t cache_update_feed(sqlite3 *conn, const char **data){
	const int qlen=256;
	char query[qlen];
	sqlite3_stmt *s;
	const int map[]={FFI_URL,FFI_TITLE,FFI_DESC,FFI_AUTHNAME,FFI_AUTHEMAIL,FFI_IURL,FFI_ITEXT,-1};
	int i;

	nb_qnprintf(query,qlen,"INSERT OR REPLACE INTO Feed (FeedID,Title,URL,Description,AuthorName,AuthorEmail,ImageURL,ImageText) VALUES ((SELECT FeedID FROM Feed WHERE URL=?),?,?1,?,?,?,?,?)");
	if(sqlite3_prepare_v2(conn,query,qlen,&s,NULL)!=SQLITE_OK)exit(2);
	for(i=0;map[i]>=0;i++)
		if(sqlite3_bind_text(s,i+1,data[map[i]],-1,SQLITE_STATIC)!=SQLITE_OK)exit(2);
	if(sqlite3_step(s)!=SQLITE_DONE)exit(2);
	if(sqlite3_finalize(s)!=SQLITE_OK)exit(2);

	return sqlite3_last_insert_rowid(conn);
}

rowid_t cache_update_entry(sqlite3 *conn, rowid_t feedid, const char **data){
	const int qlen=350;
	char query[qlen];
	sqlite3_stmt *s;
	const int textparam[]={0,1,0,0,1,1,1,1,1,1,-1};
	const int map[]={-2,EFI_UID,EFI_PDATE,EFI_UDATE,EFI_TITLE,EFI_URL,EFI_AUTHNAME,EFI_AUTHEMAIL,EFI_DESC,EFI_CONTENT,-1};
	int i;
	long rnum;

	nb_qnprintf(query,qlen,"INSERT OR REPLACE INTO Entry (EntryID,FeedID,Viewed,PubDate,ModDate,UniqueId,Title,URL,AuthorName,AuthorEmail,Description,Content) VALUES ((SELECT EntryID FROM Entry WHERE FeedID=?1 AND UniqueId=?2),?1,(SELECT IFNULL((SELECT Viewed FROM Entry WHERE FeedID=?1 AND UniqueId=?2),0)),?3,?4,?2,?5,?6,?7,?8,?9,?10)");

	if((i=sqlite3_prepare_v2(conn,query,qlen,&s,NULL))!=SQLITE_OK)exit(2);
	for(i=0;map[i]!=-1;i++){
		if(map[i]==-2){
			if(sqlite3_bind_int64(s,i+1,feedid)!=SQLITE_OK)exit(2);
		}
		else if(textparam[i]){
			if(sqlite3_bind_text(s,i+1,data[map[i]],-1,SQLITE_STATIC)!=SQLITE_OK)exit(2);
		}
		else{
			if(data[map[i]])
				rnum=strtol(data[map[i]],NULL,10);
			else
				rnum=0;

			if(sqlite3_bind_int(s,i+1,rnum)!=SQLITE_OK)exit(2);
		}
	}
	if(sqlite3_step(s)!=SQLITE_DONE)exit(2);
	if(sqlite3_finalize(s)!=SQLITE_OK)exit(2);

	return 0;
}

sqlite3* init_db(){
	char *fn=get_config_file(NB_CACHE_DB);
	sqlite3 *conn=NULL;

	if(sqlite3_open_v2(fn,&conn,SQLITE_OPEN_READWRITE,NULL)!=SQLITE_OK){
		char *dir=get_config_file(NB_CONF_DIR);
		if(mkdir(dir,0700)==0 || errno==EEXIST){
			if(sqlite3_open(fn,&conn)!=SQLITE_OK){
				debug_print("Error creating cache database\n");
				exit(2);
			}
			if(create_db(conn)){
				debug_print("Error creating cache database tables\n");
				exit(2);
			}
		}
		free(dir);
	}

	free(fn);

	return conn;
}

void cache_toggle_read_entry(sqlite3 *conn, rowid_t id, int v){
	const int qlen=256;
	char query[qlen];
	if(v<0)
		nb_qnprintf(query,qlen,"UPDATE Entry SET Viewed=NOT Viewed WHERE EntryID=%lld",id);
	else
		nb_qnprintf(query,qlen,"UPDATE Entry SET Viewed=%d WHERE EntryID=%lld",v,id);
	nb_sqlite3_exec(conn,query,NULL,NULL,NULL);
}

void cache_read_feed(sqlite3 *conn, rowid_t feedid){
	const int tlen=256;
	char tmp[tlen];
	if(feedid==0){
		nb_sqlite3_exec(conn,"UPDATE Entry SET Viewed=1",NULL,NULL,NULL);
	}
	else{
		nb_qnprintf(tmp,tlen,"UPDATE Entry SET Viewed=1 WHERE FeedID=%lld",feedid);
		nb_sqlite3_exec(conn,tmp,NULL,NULL,NULL);
	}
}
