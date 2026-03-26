#pragma once

#include <GL/glew.h>
#include <stdint.h>

bool TRX_GL_Screenshot_CaptureToFile(const char *path);

void TRX_GL_Screenshot_CaptureToBuffer(
    uint8_t *out_buffer, GLint *out_width, GLint *out_height, GLint depth,
    GLenum format, GLenum type, bool vflip);
