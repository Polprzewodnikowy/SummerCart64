#pragma once

#include <GL/glew.h>

typedef struct {
    bool initialized;
    GLuint id;
} TRX_GL_SAMPLER;

void TRX_GL_Sampler_Init(TRX_GL_SAMPLER *sampler);
void TRX_GL_Sampler_Close(TRX_GL_SAMPLER *sampler);

void TRX_GL_Sampler_Bind(TRX_GL_SAMPLER *sampler, GLuint unit);
void TRX_GL_Sampler_Parameteri(
    TRX_GL_SAMPLER *sampler, GLenum pname, GLint param);
void TRX_GL_Sampler_Parameterf(
    TRX_GL_SAMPLER *sampler, GLenum pname, GLfloat param);
