#ifndef FONT_H__
#define FONT_H__


#include <stdint.h>


#define FONT_WIDTH      (8)
#define FONT_HEIGHT     (8)
#define FONT_CHAR_BYTES (8)


extern const uint8_t font_data[96][FONT_CHAR_BYTES];


#endif
