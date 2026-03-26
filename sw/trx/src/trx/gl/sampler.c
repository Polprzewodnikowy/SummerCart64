#include <trx/gl/sampler.h>

#include <trx/debug.h>
#include <trx/gl/utils.h>

void TRX_GL_Sampler_Init(TRX_GL_SAMPLER *sampler)
{
    ASSERT(sampler != nullptr);
    glGenSamplers(1, &sampler->id);
    TRX_GL_CheckError();
    sampler->initialized = true;
}

void TRX_GL_Sampler_Close(TRX_GL_SAMPLER *sampler)
{
    ASSERT(sampler != nullptr);
    if (sampler->initialized) {
        glDeleteSamplers(1, &sampler->id);
        TRX_GL_CheckError();
    }
    sampler->initialized = false;
}

void TRX_GL_Sampler_Bind(TRX_GL_SAMPLER *sampler, GLuint unit)
{
    ASSERT(sampler != nullptr);
    ASSERT(sampler->initialized);
    glBindSampler(unit, sampler->id);
    TRX_GL_CheckError();
}

void TRX_GL_Sampler_Parameteri(
    TRX_GL_SAMPLER *sampler, GLenum pname, GLint param)
{
    ASSERT(sampler != nullptr);
    ASSERT(sampler->initialized);
    glSamplerParameteri(sampler->id, pname, param);
    TRX_GL_CheckError();
}

void TRX_GL_Sampler_Parameterf(
    TRX_GL_SAMPLER *sampler, GLenum pname, GLfloat param)
{
    ASSERT(sampler != nullptr);
    ASSERT(sampler->initialized);
    glSamplerParameterf(sampler->id, pname, param);
    TRX_GL_CheckError();
}
