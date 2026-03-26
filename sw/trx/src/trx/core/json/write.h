#pragma once

#include <trx/core/json/base.h>

/* Write out a minified JSON utf-8 string. This string is an encoding of the
 * minimal string characters required to still encode the same data.
 * json_write_minified performs 1 call to malloc for the entire encoding. Return
 * 0 if an error occurred (malformed JSON input, or malloc failed). The out_size
 * parameter is optional as the utf-8 string is null terminated. */
void *JSON_WriteMinified(const JSON_VALUE *value, size_t *out_size);

/* Write out a pretty JSON utf-8 string. This string is encoded such that the
 * resultant JSON is pretty in that it is easily human readable. The indent and
 * newline parameters allow a user to specify what kind of indentation and
 * newline they want (two spaces / three spaces / tabs? \r, \n, \r\n ?). Both
 * indent and newline can be nullptr, indent defaults to two spaces ("  "), and
 * newline defaults to linux newlines ('\n' as the newline character).
 * json_write_pretty performs 1 call to malloc for the entire encoding. Return 0
 * if an error occurred (malformed JSON input, or malloc failed). The out_size
 * parameter is optional as the utf-8 string is null terminated. */
void *JSON_WritePretty(
    const JSON_VALUE *value, const char *indent, const char *newline,
    size_t *out_size);
