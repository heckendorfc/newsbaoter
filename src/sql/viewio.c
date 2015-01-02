#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>

#include "../config.h"
#include "../urlparse.h"
#include "../ui/view.h"
#include "cache.h"

int nb_qnprintf(char *s, int n, char *fmt, ...){
	int cnt;
	va_list args;
	va_start(args, fmt);
	cnt = vsnprintf(s, n, fmt, args);
	va_end(args);
	if(cnt>=n-1){
		fprintf(stderr,"Query buffer overflow: %s\n",s);
		exit(2);
	}
	return cnt;
}

static int get_long_cb(void *data, int n_col, char **row, char **titles){
	*((long*)data)=strtol(row[0]?row[0]:"0",NULL,10);
	return SQLITE_OK;
}

typedef struct feed_list_arg{
	struct listview *lv;
	//tchar_t *line;
	int len;
	int ind;
}feed_list_arg;

static int feed_line_cb(void *data, int n_col, char **row, char **titles){
	feed_list_arg *fla = (feed_list_arg*)data;
	int off=0;
	const size_t tsize=256;
	char tmp[tsize];
	long r_ent=strtol(row[2],NULL,10);
	long tot_ent=strtol(row[3],NULL,10);
	int nc=0;
	const int count_width=10;
	char urt[30];

	fla->lv[fla->ind].unread=tot_ent-r_ent;

	sprintf(tmp,"%3d  ",fla->ind+1);
	off=chars_to_widechars(fla->lv[fla->ind].line,tmp,fla->len);

	if(r_ent<tot_ent){
		off+=chars_to_widechars(fla->lv[fla->ind].line+off,"N",fla->len-off);
	}
	else{
		off+=chars_to_widechars(fla->lv[fla->ind].line+off," ",fla->len-off);
	}

	sprintf(urt,"[%ld/%ld]",tot_ent-r_ent,tot_ent);
	nc=strlen(urt);
	nc=count_width-nc;
	if(nc<0)
		nc=0;
	snprintf(tmp,tsize,"%*c%s ",nc,' ',urt);
	off+=chars_to_widechars(fla->lv[fla->ind].line+off,tmp,fla->len-off);

	nc=-1;
	if(row[0])
		nc=0;
	else if(row[1])
		nc=1;
	else
		off+=chars_to_widechars(fla->lv[fla->ind].line+off,"Unknown",fla->len);

	if(nc>=0)
		off+=chars_to_widechars(fla->lv[fla->ind].line+off,row[nc],fla->len);

	fla->lv[fla->ind].id=strtol(row[4],NULL,10);

	fla->ind++;

	return SQLITE_OK;
}

static int write_feed_line(struct listview *lv, int len, int width, int start_ind, sqlite3 *db){
	const int qlen=256;
	char query[qlen];
	feed_list_arg fla = {.lv=lv, .len=width, .ind=0};
	const int olen=len;

	if(start_ind==0){
		nb_sqlite3_exec(db,"SELECT \"Unread Articles\",\"None\",0,COUNT(Viewed),0 FROM Entry WHERE Viewed=0",feed_line_cb,&fla,NULL);
		len--;
	}
	else{
		start_ind--;
	}

	nb_qnprintf(query,qlen,"SELECT Feed.Title,Feed.URL,SUM(Entry.Viewed),COUNT(Entry.Viewed),Feed.FeedID FROM Feed,Entry WHERE Feed.FeedID=Entry.FeedID %s GROUP BY Feed.FeedID LIMIT %d,%d",global_config.show_read_feeds?"":"AND Viewed=0",start_ind,len);

	nb_sqlite3_exec(db,query,feed_line_cb,&fla,NULL);
/*
	if(fla.ind>=len){
		long count=0;
		nb_qnprintf(query,qlen,"SELECT COUNT(Feed.FeedID) FROM Feed WHERE FeedID IN (SELECT FeedID FROM Feed %s LIMIT %d,%d)",global_config.show_read_feeds?"":", Entry WHERE Feed.FeedID=Entry.FeedID AND Viewed=0",start_ind+len,len);
		nb_sqlite3_exec(db,query,get_long_cb,&count,NULL);
		return count==0;
	}
*/
	if(fla.ind==0)
		return 1;

	for(;fla.ind<olen;fla.ind++)
		lv[fla.ind].line[0]=0;

	return 0;
}


typedef feed_list_arg entry_list_arg;

static int entry_line_cb(void *data, int n_col, char **row, char **titles){
	entry_list_arg *ela = (entry_list_arg*)data;
	int off=0;
	const size_t tsize=256;
	char tmp[tsize];
	long r=strtol(row[1],NULL,10);
	struct tm tp;

	ela->lv[ela->ind].unread=!r;

	sprintf(tmp,"%3d  ",ela->ind+1);
	off=chars_to_widechars(ela->lv[ela->ind].line,tmp,ela->len);

	if(!r){
		off+=chars_to_widechars(ela->lv[ela->ind].line+off,"N",ela->len-off);
	}
	else{
		off+=chars_to_widechars(ela->lv[ela->ind].line+off," ",ela->len-off);
	}

	strptime(row[2],"%s",&tp);
	strftime(tmp,tsize,"  %d %b  ",&tp);
	off+=chars_to_widechars(ela->lv[ela->ind].line+off,tmp,ela->len-off);

	if(n_col>4){ /* Typically, feed title */
		snprintf(tmp,tsize,"%s | ",row[4]);
		off+=chars_to_widechars(ela->lv[ela->ind].line+off,tmp,ela->len-off);
	}

	off+=chars_to_widechars(ela->lv[ela->ind].line+off,row[0],ela->len-off);

	ela->lv[ela->ind].id=strtol(row[3],NULL,10);

	ela->ind++;

	return SQLITE_OK;
}

static int write_entry_line(struct listview *lv, int len, int width, int feedid, int start_ind, sqlite3 *db){
	const int qlen=256;
	char query[qlen];
	entry_list_arg ela = {.lv=lv, .len=width, .ind=0};

	if(feedid>0){
		nb_qnprintf(query,qlen,"SELECT Title,Viewed,PubDate,EntryID FROM Entry WHERE FeedID=%d %s ORDER BY PubDate DESC LIMIT %d,%d",feedid,global_config.show_read_entries?"":"AND Viewed=0",start_ind,len);
	}
	else{ /* Aggregation of unread entries */
		nb_qnprintf(query,qlen,"SELECT Entry.Title,Entry.Viewed,Entry.PubDate,Entry.EntryID,Feed.Title FROM Entry,Feed WHERE Entry.FeedID=Feed.FeedID AND Viewed=0 ORDER BY Entry.PubDate DESC LIMIT %d,%d",start_ind,len);
		//nb_qnprintf(query,qlen,"SELECT Title,Viewed,PubDate,EntryID FROM Entry WHERE Viewed=0 ORDER BY PubDate DESC LIMIT %d,%d",start_ind,len);
	}

	nb_sqlite3_exec(db,query,entry_line_cb,&ela,NULL);

/*
	if(ela.ind>=len){
		long count;
		if(feedid>0){
			nb_qnprintf(query,qlen,"SELECT COUNT(EntryID) FROM Entry WHERE EntryID IN (SELECT EntryID From Entry WHERE FeedID=%d %s ORDER BY PubDate DESC LIMIT %d,%d)",feedid,global_config.show_read_entries?"":"AND Viewed=0",start_ind+len,1);
		}
		else{ // Aggregation of unread entries
			nb_qnprintf(query,qlen,"SELECT COUNT(EntryID) FROM Entry WHERE EntryID IN (SELECT EntryID FROM Entry WHERE Viewed=0 ORDER BY PubDate DESC LIMIT %d,%d)",start_ind+len,len);
		}
		nb_sqlite3_exec(db,query,get_long_cb,&count,NULL);
		return count==0;
	}
*/
	if(ela.ind==0)
		return 1;

	for(;ela.ind<len;ela.ind++)
		lv[ela.ind].line[0]=0;

	return 0;
}

struct set_str_args{
	char *s;
	int len;
};

static int set_string_cb(void *data, int n_col, char **row, char **titles){
	struct set_str_args *ssa = (struct set_str_args*)data;

	snprintf(ssa->s,ssa->len,"%s",row[0]);

	return SQLITE_OK;
}

static void set_title_string(struct mainwindow *mw, char *tail, sqlite3 *db){
	const int tlen=256;
	char tmp[tlen];
	char shead[tlen];
	const char *head="";
	struct set_str_args ssa={.s=shead, .len=tlen};

	if(mw->ctx_type==CTX_FEEDS)
		head="Your Feeds";
	else if(mw->ctx_type==CTX_ENTRIES){
		nb_qnprintf(tmp,tlen,"SELECT Title FROM Feed WHERE FeedID=%d",mw->ctx_id);
		nb_sqlite3_exec(db,tmp,set_string_cb,&ssa,NULL);
		head=shead;
	}

	snprintf(tmp,tlen,"%s %s",head,tail);

	chars_to_widechars(mw->header,tmp,mw->width);
}

int cache_gen_lines(struct mainwindow *mw, sqlite3 *db){
	int r=1;
	const int tlen=256;
	char tmp[tlen];
	int poff=mw->page*mw->body_len;

	if(mw->ctx_type==CTX_FEEDS){
		r=write_feed_line(mw->data.lv,mw->body_len,mw->width,poff,db);
	}
	else if(mw->ctx_type==CTX_ENTRIES){
		r=write_entry_line(mw->data.lv,mw->body_len,mw->width,mw->ctx_id,poff,db);
	}

	if(!r){
		snprintf(tmp,tlen,"(Page %d)",mw->page+1);
		set_title_string(mw,tmp,db);
	}

	return r;
}

struct write_entry_info{
	char *s;
	int len;
	int fd;
};

static int write_entry_cb(void *data, int n_col, char **row, char **titles){
	struct write_entry_info *wi = (struct write_entry_info*)data;
	int i;

	/* i=0 is FeedID; i=1 is EntryID; skip them */
	for(i=2;i<n_col;i++){
		if(row[i]==NULL || row[i][0]==0)
			continue;
		write(wi->fd,titles[i],strlen(titles[i]));
		write(wi->fd,": ",2);
		write(wi->fd,row[i],strlen(row[i]));
		if(global_config.html_pager)
			write(wi->fd,"<br/>",5);
		write(wi->fd,"\n",1);
	}

	return SQLITE_OK;
}

int cache_write_entry(struct mainwindow *mw, int id, int fd, sqlite3 *db){
	const int qlen=256;
	char query[qlen];
	struct write_entry_info wi = {.s=query, .len=qlen, .fd=fd};

	cache_toggle_read_entry(db,id,1);

	nb_qnprintf(query,qlen,"SELECT * FROM PubEntry WHERE EntryID=%d",id);
	nb_sqlite3_exec(db,query,write_entry_cb,&wi,NULL);

	return 0;
}

int get_num_unread(sqlite3 *db){
	long num;
	nb_sqlite3_exec(db,"SELECT COUNT(EntryID) FROM Entry WHERE Viewed=0",get_long_cb,&num,NULL);
	return num;
}

int cache_next_unread(struct mainwindow *mw, int id, sqlite3 *db){
	return 0;
}
