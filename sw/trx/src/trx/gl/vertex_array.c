#include <trx/gl/vertex_array.h>

#include <trx/debug.h>
#include <trx/gl/utils.h>

#include <stdint.h>

void TRX_GL_VertexArray_Init(TRX_GL_VERTEX_ARRAY *array)
{
    ASSERT(array != nullptr);
    glGenVertexArrays(1, &array->id);
    TRX_GL_CheckError();
    array->initialized = true;
}

void TRX_GL_VertexArray_Close(TRX_GL_VERTEX_ARRAY *array)
{
    ASSERT(array != nullptr);
    if (array->initialized) {
        glDeleteVertexArrays(1, &array->id);
        TRX_GL_CheckError();
    }
    array->initialized = false;
}

void TRX_GL_VertexArray_Bind(TRX_GL_VERTEX_ARRAY *array)
{
    ASSERT(array != nullptr);
    ASSERT(array->initialized);
    glBindVertexArray(array->id);
    TRX_GL_CheckError();
}

void TRX_GL_VertexArray_Attribute(
    TRX_GL_VERTEX_ARRAY *array, GLuint index, GLint size, GLenum type,
    GLboolean normalized, GLsizei stride, GLsizei offset)
{
    ASSERT(array != nullptr);
    ASSERT(array->initialized);
    glEnableVertexAttribArray(index);
    TRX_GL_CheckError();

    glVertexAttribPointer(
        index, size, type, normalized, stride, (void *)(intptr_t)offset);
    TRX_GL_CheckError();
}

void TRX_GL_VertexArray_IAttribute(
    TRX_GL_VERTEX_ARRAY *array, GLuint index, GLint size, GLenum type,
    GLsizei stride, GLsizei offset)
{
    ASSERT(array != nullptr);
    ASSERT(array->initialized);
    glEnableVertexAttribArray(index);
    TRX_GL_CheckError();

    glVertexAttribIPointer(index, size, type, stride, (void *)(intptr_t)offset);
    TRX_GL_CheckError();
}
