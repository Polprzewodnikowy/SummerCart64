#pragma once

#include <trx/core/json.h>
#include <trx/core/math/types.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct JSON_WRITE_IO JSON_WRITE_IO;

typedef enum JSON_WRITE_TYPE {
    JSON_WRITE_TYPE_BOOL = 0,
    JSON_WRITE_TYPE_INT,
    JSON_WRITE_TYPE_DOUBLE,
    JSON_WRITE_TYPE_STRING,
} JSON_WRITE_TYPE;

JSON_WRITE_IO *JSON_WriteIO_Create(void);
void JSON_WriteIO_Destroy(JSON_WRITE_IO *io);

JSON_VALUE *JSON_WriteIO_GetRoot(JSON_WRITE_IO *io);

void JSON_WriteIO_PushObject(JSON_WRITE_IO *io);
void JSON_WriteIO_PushArray(JSON_WRITE_IO *io);
void JSON_WriteIO_PopAndSet(JSON_WRITE_IO *io, const char *key);
void JSON_WriteIO_PopAndSetNZ(JSON_WRITE_IO *io, const char *key);
void JSON_WriteIO_PopAndAppend(JSON_WRITE_IO *io);
void JSON_WriteIO_DiscardCurrent(JSON_WRITE_IO *io);
JSON_OBJECT *JSON_WriteIO_GetCurrentObject(JSON_WRITE_IO *io);

void JSON_WriteIO_PushBool(JSON_WRITE_IO *io, bool value);
void JSON_WriteIO_PushInt(JSON_WRITE_IO *io, int32_t value);
void JSON_WriteIO_PushDouble(JSON_WRITE_IO *io, double value);
void JSON_WriteIO_PushXYZ16(JSON_WRITE_IO *io, XYZ_16 value);
void JSON_WriteIO_PushXYZ32(JSON_WRITE_IO *io, XYZ_32 value);
void JSON_WriteIO_PushString(JSON_WRITE_IO *io, const char *value);

#define JSONW_PUSH_OBJECT(io) JSON_WriteIO_PushObject((io))
#define JSONW_PUSH_ARRAY(io) JSON_WriteIO_PushArray((io))
#define JSONW_POP_AND_SET(io, key) JSON_WriteIO_PopAndSet((io), (key))
#define JSONW_POP_AND_SET_NZ(io, key) JSON_WriteIO_PopAndSetNZ((io), (key))
#define JSONW_POP_AND_APPEND(io) JSON_WriteIO_PopAndAppend((io))

#define JSONW_PUSH_VALUE(io, value)                                            \
    _Generic(                                                                  \
        (value),                                                               \
        bool: JSON_WriteIO_PushBool,                                           \
        int8_t: JSON_WriteIO_PushInt,                                          \
        uint8_t: JSON_WriteIO_PushInt,                                         \
        int16_t: JSON_WriteIO_PushInt,                                         \
        uint16_t: JSON_WriteIO_PushInt,                                        \
        int32_t: JSON_WriteIO_PushInt,                                         \
        uint32_t: JSON_WriteIO_PushInt,                                        \
        float: JSON_WriteIO_PushDouble,                                        \
        double: JSON_WriteIO_PushDouble,                                       \
        XYZ_16: JSON_WriteIO_PushXYZ16,                                        \
        XYZ_32: JSON_WriteIO_PushXYZ32,                                        \
        const char *: JSON_WriteIO_PushString,                                 \
        char *: JSON_WriteIO_PushString)(io, value)

#define JSONW_WRITE(io, key, value)                                            \
    do {                                                                       \
        JSONW_PUSH_VALUE((io), (value));                                       \
        JSONW_POP_AND_SET((io), (key));                                        \
    } while (0)

#define JSONW_WRITE_NZ(io, key, value)                                         \
    do {                                                                       \
        typeof(value) _tmp = (value);                                          \
        unsigned char _zero[sizeof(_tmp)] = { 0 };                             \
        if (memcmp(&_tmp, _zero, sizeof(_tmp)) != 0) {                         \
            JSONW_WRITE(io, key, _tmp);                                        \
        }                                                                      \
    } while (0)
