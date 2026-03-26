#pragma once

#include <trx/core/json/base.h>

typedef enum {
    JSON_PARSE_ERROR_NONE = 0,
    JSON_PARSE_ERROR_EXPECTED_COMMA_OR_CLOSING_BRACKET,
    JSON_PARSE_ERROR_EXPECTED_COLON,
    JSON_PARSE_ERROR_EXPECTED_OPENING_QUOTE,
    JSON_PARSE_ERROR_INVALID_STRING_ESCAPE_SEQUENCE,
    JSON_PARSE_ERROR_INVALID_NUMBER_FORMAT,
    JSON_PARSE_ERROR_INVALID_VALUE,
    JSON_PARSE_ERROR_PREMATURE_END_OF_BUFFER,
    JSON_PARSE_ERROR_INVALID_STRING,
    JSON_PARSE_ERROR_ALLOCATOR_FAILED,
    JSON_PARSE_ERROR_UNEXPECTED_TRAILING_CHARACTERS,
    JSON_PARSE_ERROR_UNKNOWN
} JSON_PARSE_ERROR;

typedef enum {
    JSON_PARSE_FLAGS_DEFAULT = 0,

    /* allow trailing commas in objects and arrays. For example, both [true,]
       and
       {"a" : null,} would be allowed with this option on. */
    JSON_PARSE_FLAGS_ALLOW_TRAILING_COMMA = 0x1,

    /* allow unquoted keys for objects. For example, {a : null} would be allowed
       with this option on. */
    JSON_PARSE_FLAGS_ALLOW_UNQUOTED_KEYS = 0x2,

    /* allow a global unbracketed object. For example, a : null, b : true, c :
       {} would be allowed with this option on. */
    JSON_PARSE_FLAGS_ALLOW_GLOBAL_OBJECT = 0x4,

    /* allow objects to use '=' instead of ':' between key/value pairs. For
       example, a = null, b : true would be allowed with this option on. */
    JSON_PARSE_FLAGS_ALLOW_EQUALS_IN_OBJECT = 0x8,

    /* allow that objects don't have to have comma separators between key/value
       pairs. */
    JSON_PARSE_FLAGS_ALLOW_NO_COMMAS = 0x10,

    /* allow c-style comments (either variants) to be ignored in the input JSON
       file. */
    JSON_PARSE_FLAGS_ALLOW_C_STYLE_COMMENTS = 0x20,

    /* deprecated flag, unused. */
    JSON_PARSE_FLAGS_DEPRECATED = 0x40,

    /* record location information for each value. */
    JSON_PARSE_FLAGS_ALLOW_LOCATION_INFORMATION = 0x80,

    /* allow strings to be 'single quoted'. */
    JSON_PARSE_FLAGS_ALLOW_SINGLE_QUOTED_STRINGS = 0x100,

    /* allow numbers to be binary. */
    JSON_PARSE_FLAGS_ALLOW_BINARY_NUMBERS = 0x4000,

    /* allow numbers to be hexadecimal. */
    JSON_PARSE_FLAGS_ALLOW_HEXADECIMAL_NUMBERS = 0x200,

    /* allow numbers like +123 to be parsed. */
    JSON_PARSE_FLAGS_ALLOW_LEADING_PLUS_SIGN = 0x400,

    /* allow numbers like .0123 or 123. to be parsed. */
    JSON_PARSE_FLAGS_ALLOW_LEADING_OR_TRAILING_DECIMAL_POINT = 0x800,

    /* allow Infinity, -Infinity, NaN, -NaN. */
    JSON_PARSE_FLAGS_ALLOW_INF_AND_NAN = 0x1000,

    /* allow multi line string values. */
    JSON_PARSE_FLAGS_ALLOW_MULTI_LINE_STRINGS = 0x2000,

    /* allow simplified JSON to be parsed. Simplified JSON is an enabling of a
       set of other parsing options. */
    JSON_PARSE_FLAGS_ALLOW_SIMPLIFIED_JSON =
        (JSON_PARSE_FLAGS_ALLOW_TRAILING_COMMA
         | JSON_PARSE_FLAGS_ALLOW_UNQUOTED_KEYS
         | JSON_PARSE_FLAGS_ALLOW_GLOBAL_OBJECT
         | JSON_PARSE_FLAGS_ALLOW_EQUALS_IN_OBJECT
         | JSON_PARSE_FLAGS_ALLOW_NO_COMMAS),

    /* allow JSON5 to be parsed. JSON5 is an enabling of a set of other parsing
       options. */
    JSON_PARSE_FLAGS_ALLOW_JSON5 =
        (JSON_PARSE_FLAGS_ALLOW_TRAILING_COMMA
         | JSON_PARSE_FLAGS_ALLOW_UNQUOTED_KEYS
         | JSON_PARSE_FLAGS_ALLOW_C_STYLE_COMMENTS
         | JSON_PARSE_FLAGS_ALLOW_SINGLE_QUOTED_STRINGS
         | JSON_PARSE_FLAGS_ALLOW_HEXADECIMAL_NUMBERS
         | JSON_PARSE_FLAGS_ALLOW_BINARY_NUMBERS
         | JSON_PARSE_FLAGS_ALLOW_LEADING_PLUS_SIGN
         | JSON_PARSE_FLAGS_ALLOW_LEADING_OR_TRAILING_DECIMAL_POINT
         | JSON_PARSE_FLAGS_ALLOW_INF_AND_NAN
         | JSON_PARSE_FLAGS_ALLOW_MULTI_LINE_STRINGS)
} JSON_PARSE_FLAGS;

/* Parse a JSON text file, returning a pointer to the root of the JSON
 * structure. JSON_Parse performs 1 call to malloc for the entire encoding.
 * Returns 0 if an error occurred (malformed JSON input, or malloc failed). */
JSON_VALUE *JSON_Parse(const void *src, size_t src_size);

/* Parse a JSON text file, returning a pointer to the root of the JSON
 * structure. JSON_Parse performs 1 call to alloc_func_ptr for the entire
 * encoding. Returns 0 if an error occurred (malformed JSON input, or malloc
 * failed). If an error occurred, the result struct (if not nullptr) will
 * explain the type of error, and the location in the input it occurred. If
 * alloc_func_ptr is null then malloc is used. */
JSON_VALUE *JSON_ParseEx(
    const void *src, size_t src_size, size_t flags_bitset,
    void *(*alloc_func_ptr)(void *, size_t), void *user_data,
    JSON_PARSE_RESULT *result);

const char *JSON_GetErrorDescription(JSON_PARSE_ERROR error);
