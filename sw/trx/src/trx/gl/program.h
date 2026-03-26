#pragma once

#include <trx/gl/enum.h>

#include <GL/glew.h>

typedef struct {
    char *path;
    bool initialized;
    GLuint id;
} TRX_GL_PROGRAM;

bool TRX_GL_Program_Init(TRX_GL_PROGRAM *program);
void TRX_GL_Program_Close(TRX_GL_PROGRAM *program);

void TRX_GL_Program_Bind(const TRX_GL_PROGRAM *program);
void TRX_GL_Program_AttachShader(
    TRX_GL_PROGRAM *program, GLenum type, const char *path);
void TRX_GL_Program_Link(TRX_GL_PROGRAM *program);
void TRX_GL_Program_FragmentData(TRX_GL_PROGRAM *program, const char *name);
GLint TRX_GL_Program_UniformLocation(TRX_GL_PROGRAM *program, const char *name);

void TRX_GL_Program_Uniform4f(
    TRX_GL_PROGRAM *program, GLint loc, GLfloat v0, GLfloat v1, GLfloat v2,
    GLfloat v3);
void TRX_GL_Program_Uniform1i(TRX_GL_PROGRAM *program, GLint loc, GLint v0);
void TRX_GL_Program_Uniform1f(TRX_GL_PROGRAM *program, GLint loc, GLfloat v0);
void TRX_GL_Program_Uniform2f(
    TRX_GL_PROGRAM *program, GLint loc, GLfloat v0, GLfloat v1);
