#ifndef LOADER_H__
#define LOADER_H__


#include "errors.h"


void loader_init(void);
void loader_cleanup(void);
void loader_display_error_and_halt(menu_load_error_t error, const char *path);


#endif
