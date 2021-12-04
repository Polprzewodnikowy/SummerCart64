#ifndef CRC32_H__
#define CRC32_H__


#include <stddef.h>
#include <stdint.h>


uint32_t crc32_calculate (void *buffer, size_t length);


#endif
