#ifndef NBUI_UTIL_H
#define NBUI_UTIL_H

#include "ui_common.h"

int pipe_entry(struct mainwindow *mw, int id);
int request_list_update(struct mainwindow *mw);
int catchup_entries(struct mainwindow *mw, int id);
int toggle_read_ipc(struct mainwindow *mw, int id);
int next_unread_ipc(struct mainwindow *mw, int id);

#endif
