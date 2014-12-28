#ifndef NBUI_KEYS_H
#define NBUI_KEYS_H

#include "ui_common.h"

void bind_key(int c, handler_t handler);
void unbind_key(int c);
int process_key(int c, struct mainwindow *mw);
void bind_defaults();

#endif
