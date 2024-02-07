#ifndef ISV_H__
#define ISV_H__


#include <stdbool.h>
#include <stdint.h>


bool isv_set_address (uint32_t address);
uint32_t isv_get_address (void);

void isv_init (void);

void isv_process (void);


#endif
