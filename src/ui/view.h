#ifndef NBUIVIEW_H
#define NBUIVIEW_H

#include "ui_common.h"

int next_item(struct mainwindow *mw);
int prev_item(struct mainwindow *mw);
int select_item(struct mainwindow *mw);
int context_exit(struct mainwindow *mw);

int chars_to_widechars(tchar_t *dst, const char *src, int max);
void update_view(struct mainwindow *mw);
struct mainwindow* setup_ui();
void run_ui(struct mainwindow *mw);
void end_ui();

#endif
