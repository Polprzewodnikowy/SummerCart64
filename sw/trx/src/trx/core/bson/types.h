#pragma once

#include <trx/core/bson/enum.h>
#include <trx/core/json/base.h>

typedef struct {
    BSON_PARSE_ERROR error;
    size_t error_offset;
} BSON_PARSE_RESULT;
