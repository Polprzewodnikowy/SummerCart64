#ifndef SC64_H__
#define SC64_H__

#include "boot.h"

void sc64_enable_sdram(void);
void sc64_disable_sdram(void);
cic_type_t sc64_get_cic_type(void);
tv_type_t sc64_get_tv_type(void);

#endif
