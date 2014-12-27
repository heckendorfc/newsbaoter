#ifndef NBUI_KEYS_H
#define NBUI_KEYS_H

enum hk_ret{
	KH_RET_NO_HANDLER,
	KH_RET_OK,
	KH_RET_EXIT,
};

typedef int(*handler_t)();

void bind_key(int c, handler_t handler);
void unbind_key(int c);
int process_key(int c);
void bind_defaults();

#endif
