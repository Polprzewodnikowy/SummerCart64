#ifndef ISV_H__
#define ISV_H__


#include <stdbool.h>


void isv_set_enabled (bool enabled);
bool isv_get_enabled (void);
void isv_init (void);
void isv_process (void);


#endif
