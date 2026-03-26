#include <trx/gl/utils.h>

#include <GL/glew.h>

const char *TRX_GL_GetErrorString(GLenum err)
{
    switch (err) {
    case GL_NO_ERROR:
        return "GL_NO_ERROR";
    case GL_INVALID_ENUM:
        return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:
        return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:
        return "GL_INVALID_OPERATION";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case GL_OUT_OF_MEMORY:
        return "GL_OUT_OF_MEMORY";
    case GL_STACK_UNDERFLOW:
        return "GL_STACK_UNDERFLOW";
    case GL_STACK_OVERFLOW:
        return "GL_STACK_OVERFLOW";
    default:
        return "UNKNOWN";
    }
}

void TRX_GL_CheckError(void)
{
    for (GLenum err; (err = glGetError()) != GL_NO_ERROR;) {
        LOG_ERROR("glGetError: (%s)", TRX_GL_GetErrorString(err));
    }
}
