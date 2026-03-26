#pragma once

#include <trx/core/bson/types.h>

// Parse a BSON file, returning a pointer to the root of the JSON structure.
// Returns nullptr if an error occurred (malformed BSON input, or malloc
// failed).
JSON_VALUE *BSON_Parse(const char *src, size_t src_size);

JSON_VALUE *BSON_ParseEx(
    const char *src, size_t src_size, BSON_PARSE_RESULT *result);

const char *BSON_GetErrorDescription(BSON_PARSE_ERROR error);
