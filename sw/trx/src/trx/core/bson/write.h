#pragma once

#include <trx/core/bson/types.h>

/* Write out a BSON binary string. Return 0 if an error occurred (malformed
 * JSON input, or malloc failed). The out_size parameter is optional. */
void *BSON_Write(const JSON_VALUE *value, size_t *out_size);
