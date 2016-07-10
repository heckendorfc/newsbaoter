/* Copyright 2015 Christian Heckendorf.  All rights reserved.
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file.
 */

#ifndef NBUI_UTIL_H
#define NBUI_UTIL_H

#include "ui_common.h"

void wait_for_process(struct mainwindow *mw, int pid);
int pipe_to_pager(struct mainwindow *mw, int *pid, int *fd);
int download_entry(struct mainwindow *mw, int id);
int pipe_entry(struct mainwindow *mw, int id);
int request_list_update(struct mainwindow *mw);
int catchup_entries(struct mainwindow *mw, int id);
int toggle_read_ipc(struct mainwindow *mw, int id);
int next_unread_ipc(struct mainwindow *mw, int id);

#endif
