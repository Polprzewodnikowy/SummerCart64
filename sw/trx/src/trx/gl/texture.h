#pragma once

#include <GL/glew.h>

typedef struct {
    bool initialized;
    GLuint id;
    GLenum target;
} TRX_GL_TEXTURE;

TRX_GL_TEXTURE *TRX_GL_Texture_Create(GLenum target);
void TRX_GL_Texture_Free(TRX_GL_TEXTURE *texture);

void TRX_GL_Texture_Init(TRX_GL_TEXTURE *texture, GLenum target);
void TRX_GL_Texture_Close(TRX_GL_TEXTURE *texture);
void TRX_GL_Texture_Bind(const TRX_GL_TEXTURE *texture);
void TRX_GL_Texture_Load(
    TRX_GL_TEXTURE *texture, const void *data, int width, int height,
    GLint internal_format, GLint format);
void TRX_GL_Texture_LoadFromBackBuffer(TRX_GL_TEXTURE *texture);
