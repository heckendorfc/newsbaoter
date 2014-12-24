#ifndef NBHASH_H
#define NBHASH_H

#include <stdlib.h>
#include <stdint.h>

#define INITIAL_LOCTAB_SIZE 32
#define INITIAL_ALIASTAB_SIZE 32

typedef struct var_t{
	char *name;
	void *value;
}var_t;

void init_urltag_table();
void* get_urltag(const char *name);
void set_urltag(const char *name, void *value);

#endif
