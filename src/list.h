#ifndef NBLIST_H
#define NBLIST_H

#include "common_defs.h"

#define LIST_ADD(x,t) {t=x; INIT_MEM(x,1); x->next=t;}

#endif
