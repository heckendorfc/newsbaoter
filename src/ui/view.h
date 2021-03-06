/* Copyright 2015 Christian Heckendorf.  All rights reserved.
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file.
 */

#ifndef NBUIVIEW_H
#define NBUIVIEW_H

#include "ui_common.h"

void notify_refresh_footer(struct mainwindow *mw);
int next_item(struct mainwindow *mw);
int high_item(struct mainwindow *mw);
int mid_item(struct mainwindow *mw);
int low_item(struct mainwindow *mw);
int next_unread_item(struct mainwindow *mw);
int prev_item(struct mainwindow *mw);
int select_item(struct mainwindow *mw);
int download_item(struct mainwindow *mw);
int context_exit(struct mainwindow *mw);
int refresh_all(struct mainwindow *mw);
int catchup_feed(struct mainwindow *mw);
int catchup_all(struct mainwindow *mw);
int toggle_read(struct mainwindow *mw);

int chars_to_widechars(tchar_t *dst, const char *src, int max);
void update_view(struct mainwindow *mw);
struct mainwindow* setup_ui();
void run_ui(struct mainwindow *mw);
void end_ui();
void catchresize(int sig);
void set_color_pair(int n, int f, int b, int a);
void set_config_colors();

#endif
