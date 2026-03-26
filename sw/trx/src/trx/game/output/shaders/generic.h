#pragma once

#include <GL/glew.h>

typedef struct OUTPUT_SHADER OUTPUT_SHADER;

OUTPUT_SHADER *Output_Shader_Create(const char *path);
void Output_Shader_Free(OUTPUT_SHADER *shader);
void Output_Shader_Bind(const OUTPUT_SHADER *shader);

GLint Output_Shader_LookupUniform(
    const OUTPUT_SHADER *shader, const char *name);

bool Output_Shader_TryLookupUniform(
    const OUTPUT_SHADER *shader, const char *name, GLint *out_location);
