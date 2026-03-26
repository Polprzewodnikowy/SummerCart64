#pragma once

typedef enum {
    COT_BOOL,
    COT_INT32,
    COT_FLOAT,
    COT_FLOAT_PERCENT,
    COT_DOUBLE,
    COT_ENUM,
    COT_RGB888,
    COT_STRING,
    COT_DYNAMIC_ENUM,
} CONFIG_OPTION_TYPE;

typedef struct {
    const char *name;
    CONFIG_OPTION_TYPE type;
    const void *target;
    const void *default_value;
    const void *param;
} CONFIG_OPTION;
