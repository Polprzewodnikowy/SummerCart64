#pragma once

#include <trx/core/colors.h>
#include <trx/core/json.h>
#include <trx/core/log.h>
#include <trx/core/math/types.h>

#include <stddef.h>
#include <stdint.h>

typedef struct JSON_READ_IO JSON_READ_IO;

typedef struct {
    void *tmp;
} JSON_READ_IO_DUMMY;

#define JSON_READ_IO_TYPE_LIST_BASE(X)                                         \
    X(Bool, bool)                                                              \
    X(S8, int8_t)                                                              \
    X(U8, uint8_t)                                                             \
    X(S16, int16_t)                                                            \
    X(U16, uint16_t)                                                           \
    X(S32, int32_t)                                                            \
    X(U32, uint32_t)                                                           \
    X(Float, float)                                                            \
    X(Double, double)                                                          \
    X(XYZ16, XYZ_16)                                                           \
    X(XYZ32, XYZ_32)                                                           \
    X(RGB888, RGB_888)                                                         \
    X(RGBA8888, RGBA_8888)                                                     \
    X(String, const char *)
#define JSON_READ_IO_TYPE_LIST JSON_READ_IO_TYPE_LIST_BASE

#define JSON_READ_IO_TYPE_TO_CURRENT_FN(name, ctype)                           \
    ctype:                                                                     \
    JSON_ReadIO_Read##name##Current,

// ============================================================================
// Public APIs

JSON_READ_IO *JSON_ReadIO_Create(
    JSON_VALUE *root, uint16_t version, const char *source_path);
void JSON_ReadIO_Destroy(JSON_READ_IO *io);

const char *JSON_ReadIO_GetError(const JSON_READ_IO *io);
const char *JSON_ReadIO_GetErrorPath(const JSON_READ_IO *io);
const char *JSON_ReadIO_GetErrorBody(const JSON_READ_IO *io);
int32_t JSON_ReadIO_GetErrorLine(const JSON_READ_IO *io);
int32_t JSON_ReadIO_GetErrorCol(const JSON_READ_IO *io);
uint16_t JSON_ReadIO_GetVersion(const JSON_READ_IO *io);
void JSON_ReadIO_FormatError(
    const JSON_READ_IO *io, bool multiline, char *buffer, size_t buffer_size);

void JSON_ReadIO_SetError(JSON_READ_IO *io, const char *fmt, ...);
void JSON_ReadIO_SetErrorAt(
    JSON_READ_IO *io, int32_t line, int32_t col, const char *fmt, ...);
bool JSON_ReadIO_PushObject(JSON_READ_IO *io, const char *key);
bool JSON_ReadIO_PushArrayElem(JSON_READ_IO *io, size_t index);
bool JSON_ReadIO_Pop(JSON_READ_IO *io);
int32_t JSON_ReadIO_GetArrayLength(JSON_READ_IO *io);
bool JSON_ReadIO_HasKey(JSON_READ_IO *io, const char *key);
JSON_OBJECT *JSON_ReadIO_GetCurrentObject(JSON_READ_IO *io);
JSON_VALUE *JSON_ReadIO_GetCurrentValue(JSON_READ_IO *io);

#define L_DECLARE_JSON_READ_IO_TYPE(name, ctype)                               \
    bool JSON_ReadIO_Read##name##Current(JSON_READ_IO *io, void *target);
JSON_READ_IO_TYPE_LIST(L_DECLARE_JSON_READ_IO_TYPE)
#undef L_DECLARE_JSON_READ_IO_TYPE

#define JSON_PUSH(io, key) JSON_ReadIO_PushObject((io), (key))
#define JSON_PUSH_INDEX(io, idx) JSON_ReadIO_PushArrayElem((io), (idx))
#define JSON_POP(io) JSON_ReadIO_Pop((io))
#define JSON_ARRAY_LEN(io) JSON_ReadIO_GetArrayLength((io))

// Read the value into target_ptr from the current stack value.
#define JSON_READ_CURRENT(io, target_ptr)                                      \
    _Generic(                                                                  \
        *(target_ptr),                                                         \
        JSON_READ_IO_TYPE_LIST(JSON_READ_IO_TYPE_TO_CURRENT_FN)                \
            JSON_READ_IO_DUMMY: JSON_ReadIO_ReadS32Current)(                   \
        (io), (target_ptr))

// Push a key onto stack, read the value into target_ptr, and pop the value.
// Fails if the key is missing, or reading failed.
#define JSON_READ(io, key, target_ptr)                                         \
    (JSON_PUSH((io), (key))                                                    \
         ? (JSON_READ_CURRENT((io), (target_ptr)) ? JSON_POP((io))             \
                                                  : (JSON_POP((io)), false))   \
         : false)

// Like JSON_READ(), except also supports default value to fall back to.
#define JSON_READ_D(io, key, target_ptr, default_value)                        \
    ((*(target_ptr) = (default_value)), JSON_READ((io), (key), (target_ptr)))

// Like JSON_READ(), except push an array index instead of an object key.
#define JSON_READ_A(io, idx, target_ptr)                                       \
    (JSON_PUSH_INDEX((io), (idx))                                              \
         ? (JSON_READ_CURRENT((io), (target_ptr)) ? JSON_POP((io))             \
                                                  : (JSON_POP((io)), false))   \
         : false)

// ============================================================================
// Control flow macros.

// Users of this API are expected to consume JSON_ReadIO_GetError() in the
// outermost scope. Example usage:
//
// static bool M_CustomSectionReader(JSON_READ_IO *io)
// {
//     int32_t foo_value;
//     JSON_MUST(JSON_READ(io, "foo", &foo_value);
//     JSON_FINISH();
// }
//
// static void M_OuterReader(void)
// {
//     JSON_READ_IO *io = JSON_ReadIO_Create(…);
//     bool success = M_CustomSectionReader(io);
//     JSON_ReadIO_Destroy(io);
// }

// If the expr fails, log an error, and carry on.
#define JSON_SHOULD(expr)                                                      \
    ((expr) ? 1 : (LOG_WARNING("%s", JSON_ReadIO_GetError(io)), 0))

// Do nothing.
#define JSON_OPTIONAL(expr) (expr)

// If the expr fails, immediately go to the failure route.
#define JSON_MUST(expr)                                                        \
    if (!(expr)) {                                                             \
        goto fail;                                                             \
    }

// Immediately go to the failure route (with `goto fail`).
#define JSON_FAIL() goto fail;

// Declare the failure route: by default, it just does return a bool.
// To be used at the end of the function.
#define JSON_FINISH()                                                          \
    do {                                                                       \
    success:                                                                   \
        return true;                                                           \
    fail:                                                                      \
        return false;                                                          \
    } while (0);
