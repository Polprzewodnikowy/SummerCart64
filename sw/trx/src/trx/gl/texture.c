#include <trx/gl/texture.h>

#include <trx/core/memory.h>
#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/gl/utils.h>

TRX_GL_TEXTURE *TRX_GL_Texture_Create(GLenum target)
{
    TRX_GL_TEXTURE *texture = Memory_Alloc(sizeof(TRX_GL_TEXTURE));
    TRX_GL_Texture_Init(texture, target);
    return texture;
}

void TRX_GL_Texture_Free(TRX_GL_TEXTURE *texture)
{
    if (texture != nullptr) {
        TRX_GL_Texture_Close(texture);
        Memory_FreePointer(&texture);
    }
}

void TRX_GL_Texture_Init(TRX_GL_TEXTURE *texture, GLenum target)
{
    ASSERT(texture != nullptr);
    texture->target = target;
    glGenTextures(1, &texture->id);
    TRX_GL_CheckError();
    texture->initialized = true;
}

void TRX_GL_Texture_Close(TRX_GL_TEXTURE *texture)
{
    ASSERT(texture != nullptr);
    if (texture->initialized) {
        glDeleteTextures(1, &texture->id);
        TRX_GL_CheckError();
    }
    texture->initialized = false;
}

void TRX_GL_Texture_Bind(const TRX_GL_TEXTURE *texture)
{
    ASSERT(texture != nullptr);
    ASSERT(texture->initialized);
    glBindTexture(texture->target, texture->id);
    TRX_GL_CheckError();
}

void TRX_GL_Texture_Load(
    TRX_GL_TEXTURE *texture, const void *data, int width, int height,
    GLint internal_format, GLint format)
{
    ASSERT(texture != nullptr);
    ASSERT(texture->initialized);

    TRX_GL_Texture_Bind(texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D, 0, internal_format, width, height, 0, format,
        GL_UNSIGNED_BYTE, data);
    TRX_GL_CheckError();

    glGenerateMipmap(GL_TEXTURE_2D);
    TRX_GL_CheckError();
}

void TRX_GL_Texture_LoadFromBackBuffer(TRX_GL_TEXTURE *const texture)
{
    ASSERT(texture != nullptr);
    ASSERT(texture->initialized);

    TRX_GL_Texture_Bind(texture);

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    TRX_GL_CheckError();

    const GLint vp_x = viewport[0];
    const GLint vp_y = viewport[1];
    const GLint vp_w = viewport[2];
    const GLint vp_h = viewport[3];

    const int32_t side = MIN(vp_w, vp_h);
    const int32_t x = vp_x + (vp_w - side) / 2;
    const int32_t y = vp_y + (vp_h - side) / 2;
    const int32_t w = side;
    const int32_t h = side;

    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, x, y, w, h, 0);
    TRX_GL_CheckError();
}
