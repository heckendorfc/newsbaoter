/* Copyright 2015 Christian Heckendorf.  All rights reserved.
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file.
 */

#ifndef NBUI_KEYS_H
#define NBUI_KEYS_H

#include "ui_common.h"

handler_t find_handler(char *s);
void unset_handler(char *s);
void bind_key(int c, handler_t handler);
void unbind_key(int c);
int process_key(int c, struct mainwindow *mw);
void set_foot_help_string(char *s, int len);
void bind_defaults();

#endif
