#ifndef INIT_H__
#define INIT_H__


#include <stdint.h>


typedef enum {
    INIT_TV_TYPE_PAL = 0,
    INIT_TV_TYPE_NTSC = 1,
    INIT_TV_TYPE_MPAL = 2,
} init_tv_type_t;

typedef enum {
    INIT_RESET_TYPE_COLD = 0,
    INIT_RESET_TYPE_WARM = 1,
} init_reset_type_t;


extern init_tv_type_t __tv_type;
extern init_reset_type_t __reset_type;
extern uint32_t __entropy;


void init (init_tv_type_t tv_type, init_reset_type_t reset_type, uint32_t entropy);
void deinit (void);


#endif
