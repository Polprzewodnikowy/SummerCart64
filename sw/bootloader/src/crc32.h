#ifndef CRC32_H__
#define CRC32_H__

#include <inttypes.h>
#include <stddef.h>

uint32_t crc32_calculate(void *buffer, size_t length);

#endif
