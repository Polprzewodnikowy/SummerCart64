#ifndef CIC_H__
#define CIC_H__


#include <stdint.h>


#define IPL3_LENGTH     (4032)


typedef enum {
    CIC_5101,
    CIC_5167,
    CIC_6101,
    CIC_7102,
    CIC_6102_7101,
    CIC_x103,
    CIC_x105,
    CIC_x106,
    CIC_8301,
    CIC_8302,
    CIC_8303,
    CIC_8401,
    CIC_8501,
    CIC_UNKNOWN,
} cic_type_t;


cic_type_t cic_detect (uint8_t *ipl3);
uint8_t cic_get_seed (cic_type_t cic_type);


#endif
