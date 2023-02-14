#ifndef VERSION_H__
#define VERSION_H__


#include <stdint.h>


typedef enum {
    API_USB,
    API_N64,
} version_api_type_t;


uint32_t version_api (version_api_type_t type);


#endif
