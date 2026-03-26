#include <trx/game/output/shaders/ui.h>

#include <trx/gl/utils.h>

OUTPUT_UI_SHADER *Output_UIShader_Create(void)
{
    OUTPUT_SHADER *const shader = Output_Shader_Create("ui.glsl");
    TRX_GL_TRACK_UNIFORM(
        glUniform1i, Output_Shader_LookupUniform(shader, "uTexAtlas"), 0);
    return shader;
}

void Output_UIShader_Bind(const OUTPUT_UI_SHADER *const shader)
{
    Output_Shader_Bind(shader);
}

void Output_UIShader_Free(OUTPUT_UI_SHADER *const shader)
{
    Output_Shader_Free(shader);
}
