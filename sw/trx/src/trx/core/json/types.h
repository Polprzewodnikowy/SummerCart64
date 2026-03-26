#pragma once

#define JSON_INVALID_BOOL -1
#define JSON_INVALID_STRING nullptr
#define JSON_INVALID_NUMBER 0x7FFFFFFF

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#define json_uintmax_t uintmax_t
#define json_strtoumax strtoumax

#define JSON_CONST_DISPATCH(arg, ctype, call)                                  \
    _Generic(0 ? (arg) : (void *)1, const void *: (ctype)call, default: call)

typedef struct {
    void *payload;
    size_t type;
    size_t ref_count;
} JSON_VALUE;

typedef struct {
    char *string;
    size_t string_size;
    size_t ref_count;
} JSON_STRING;

typedef struct {
    JSON_STRING string;
    size_t offset;
    size_t line_no;
    size_t row_no;
} JSON_STRING_EX;

typedef struct {
    char *number;
    size_t number_size;
    size_t ref_count;
} JSON_NUMBER;

typedef struct JSON_OBJECT_ELEMENT {
    JSON_STRING *name;
    JSON_VALUE *value;
    struct JSON_OBJECT_ELEMENT *next;
    size_t ref_count;
} JSON_OBJECT_ELEMENT;

typedef struct {
    JSON_OBJECT_ELEMENT *start;
    size_t length;
    size_t ref_count;
} JSON_OBJECT;

typedef struct JSON_ARRAY_ELEMENT {
    JSON_VALUE *value;
    struct JSON_ARRAY_ELEMENT *next;
    size_t ref_count;
} JSON_ARRAY_ELEMENT;

typedef struct {
    JSON_ARRAY_ELEMENT *start;
    size_t length;
    size_t ref_count;
} JSON_ARRAY;

typedef struct {
    JSON_VALUE value;
    size_t offset;
    size_t line_no;
    size_t row_no;
} JSON_VALUE_EX;

typedef struct {
    size_t error;
    size_t error_offset;
    size_t error_line_no;
    size_t error_row_no;
} JSON_PARSE_RESULT;
