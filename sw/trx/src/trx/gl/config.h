#pragma once

#include <trx/gl/enum.h>

#include <stdint.h>

typedef struct {
    TEXTURE_FILTER display_filter;
    bool enable_wireframe;
    int32_t line_width;
} TRX_GL_CONFIG;
