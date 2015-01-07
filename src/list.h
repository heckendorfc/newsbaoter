/* Copyright 2015 Christian Heckendorf.  All rights reserved.
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file.
 */

#ifndef NBLIST_H
#define NBLIST_H

#include "common_defs.h"

#define LIST_ADD(x,t) {t=x; INIT_MEM(x,1); x->next=t;}

#endif
