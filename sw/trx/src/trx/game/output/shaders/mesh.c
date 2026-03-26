#include <trx/game/output/shaders/mesh.h>

#include <trx/core/memory.h>
#include <trx/core/utils.h>
#include <trx/game/output/state.h>
#include <trx/game/output/utils.h>
#include <trx/gl/utils.h>
#include <trx/version.h>

#include <string.h>

struct OUTPUT_MESH_SHADER {
    OUTPUT_SHADER *base_tr12;
    OUTPUT_SHADER *base_tr3;

    MATRIX model_matrix[2];
    bool has_model_matrix[2];
    int32_t water_effect[2];
    float water_effect_params[2][3];
    bool is_wibble_effect[2];
    bool is_alpha_discard_enabled[2];
    RGB_F tint[2];
};

static int32_t M_GetVariantIndex(void)
{
    return g_TRVersion >= 3 ? 1 : 0;
}

static OUTPUT_SHADER *M_GetVariantBase(
    const OUTPUT_MESH_SHADER *const shader, const int32_t variant_idx)
{
    return variant_idx != 0 ? shader->base_tr3 : shader->base_tr12;
}

OUTPUT_MESH_SHADER *Output_MeshShader_Create(void)
{
    OUTPUT_MESH_SHADER *const shader = Memory_Alloc(sizeof(*shader));
    shader->has_model_matrix[0] = false;
    shader->has_model_matrix[1] = false;
    shader->water_effect[0] = -1;
    shader->water_effect[1] = -1;
    shader->water_effect_params[0][0] = 0.0f;
    shader->water_effect_params[0][1] = 0.0f;
    shader->water_effect_params[0][2] = 0.0f;
    shader->water_effect_params[1][0] = 0.0f;
    shader->water_effect_params[1][1] = 0.0f;
    shader->water_effect_params[1][2] = 0.0f;
    shader->is_wibble_effect[0] = false;
    shader->is_wibble_effect[1] = false;
    shader->is_alpha_discard_enabled[0] = false;
    shader->is_alpha_discard_enabled[1] = false;
    shader->tint[0] = (RGB_F) { 0.0f, 0.0f, 0.0f };
    shader->tint[1] = (RGB_F) { 0.0f, 0.0f, 0.0f };

    shader->base_tr12 = Output_Shader_Create("meshes_tr12.glsl");
    shader->base_tr3 = Output_Shader_Create("meshes_tr3.glsl");

    Output_Shader_Bind(shader->base_tr12);
    TRX_GL_TRACK_UNIFORM(
        glUniform1i,
        Output_Shader_LookupUniform(shader->base_tr12, "uTexAtlas"), 0);
    TRX_GL_TRACK_UNIFORM(
        glUniform1i,
        Output_Shader_LookupUniform(shader->base_tr12, "uTexEnvMap"), 1);

    Output_Shader_Bind(shader->base_tr3);
    TRX_GL_TRACK_UNIFORM(
        glUniform1i, Output_Shader_LookupUniform(shader->base_tr3, "uTexAtlas"),
        0);
    TRX_GL_TRACK_UNIFORM(
        glUniform1i,
        Output_Shader_LookupUniform(shader->base_tr3, "uTexEnvMap"), 1);
    return shader;
}

void Output_MeshShader_Bind(const OUTPUT_MESH_SHADER *const shader)
{
    const int32_t variant_idx = M_GetVariantIndex();
    Output_Shader_Bind(M_GetVariantBase(shader, variant_idx));
}

void Output_MeshShader_Free(OUTPUT_MESH_SHADER *const shader)
{
    Output_Shader_Free(shader->base_tr12);
    Output_Shader_Free(shader->base_tr3);
    Memory_Free(shader);
}

void Output_MeshShader_UploadModelMatrix(
    OUTPUT_MESH_SHADER *const shader, const MATRIX *const source)
{
    const int32_t variant_idx = M_GetVariantIndex();
    OUTPUT_SHADER *const base = M_GetVariantBase(shader, variant_idx);
    if (shader->has_model_matrix[variant_idx]
        && memcmp(&shader->model_matrix[variant_idx], source, sizeof(*source))
            == 0) {
        return;
    }
    memcpy(&shader->model_matrix[variant_idx], source, sizeof(*source));
    shader->has_model_matrix[variant_idx] = true;

    GLfloat m[4][4];
    Output_FillMatrix(m, source);

    TRX_GL_TRACK_UNIFORM(
        glUniformMatrix4fv, Output_Shader_LookupUniform(base, "uMatModel"), 1,
        GL_FALSE, &m[0][0]);
}

void Output_MeshShader_UploadAlphaDiscard(
    OUTPUT_MESH_SHADER *const shader, const bool is_enabled)
{
    const int32_t variant_idx = M_GetVariantIndex();
    OUTPUT_SHADER *const base = M_GetVariantBase(shader, variant_idx);
    if (is_enabled == shader->is_alpha_discard_enabled[variant_idx]) {
        return;
    }
    TRX_GL_TRACK_UNIFORM(
        glUniform1i, Output_Shader_LookupUniform(base, "uDiscardAlpha"),
        is_enabled);
    shader->is_alpha_discard_enabled[variant_idx] = is_enabled;
}

void Output_MeshShader_UploadWaterEffect(
    OUTPUT_MESH_SHADER *const shader, const int32_t water_effect)
{
    const int32_t variant_idx = M_GetVariantIndex();
    OUTPUT_SHADER *const base = M_GetVariantBase(shader, variant_idx);
    if (water_effect == shader->water_effect[variant_idx]) {
        return;
    }

    static const float m_ChoppyAmp[22] = {
        16.0f, 0.0f,   0.0f,   0.0f,   0.0f,   16.0f, 16.0f, 16.0f,
        16.0f, 53.0f,  53.0f,  53.0f,  53.0f,  90.0f, 90.0f, 90.0f,
        90.0f, 127.0f, 127.0f, 127.0f, 127.0f, 0.0f,
    };
    static const float m_ShimmerAmp[22] = {
        7.875f,   4.0f,     8.0f,     12.0f,    15.875f,  -3.875f,
        -7.875f,  -11.875f, -15.875f, -3.875f,  -7.875f,  -11.875f,
        -15.875f, -3.875f,  -7.875f,  -11.875f, -15.875f, -3.875f,
        -7.875f,  -11.875f, -15.875f, 0.0f,
    };
    static const float m_AbsIntensity[22] = {
        0.0f,  253.0f, 0.0f, 4.0f,  8.0f,  4.0f, 8.0f, 12.0f,
        16.0f, 4.0f,   8.0f, 12.0f, 16.0f, 4.0f, 8.0f, 12.0f,
        16.0f, 4.0f,   8.0f, 12.0f, 16.0f, 0.0f,
    };

    int32_t scheme = water_effect - 2;
    CLAMP(scheme, 0, 21);
    const float p0 = m_ChoppyAmp[scheme];
    const float p1 = m_ShimmerAmp[scheme];
    const float p2 = m_AbsIntensity[scheme];

    GLint loc = -1;
    if (Output_Shader_TryLookupUniform(base, "uWaterEffect", &loc)) {
        TRX_GL_TRACK_UNIFORM(glUniform1i, loc, water_effect);
    }
    if (Output_Shader_TryLookupUniform(base, "uWaterEffectParams", &loc)) {
        TRX_GL_TRACK_UNIFORM(glUniform3f, loc, p0, p1, p2);
    }
    shader->water_effect[variant_idx] = water_effect;
    shader->water_effect_params[variant_idx][0] = p0;
    shader->water_effect_params[variant_idx][1] = p1;
    shader->water_effect_params[variant_idx][2] = p2;
}

void Output_MeshShader_UploadWibbleEffect(
    OUTPUT_MESH_SHADER *const shader, const bool is_enabled)
{
    const int32_t variant_idx = M_GetVariantIndex();
    OUTPUT_SHADER *const base = M_GetVariantBase(shader, variant_idx);
    if (is_enabled == shader->is_wibble_effect[variant_idx]) {
        return;
    }
    TRX_GL_TRACK_UNIFORM(
        glUniform1i, Output_Shader_LookupUniform(base, "uWibbleEffect"),
        is_enabled);
    shader->is_wibble_effect[variant_idx] = is_enabled;
}

void Output_MeshShader_UploadTint(OUTPUT_MESH_SHADER *const shader, RGB_F tint)
{
    const int32_t variant_idx = M_GetVariantIndex();
    OUTPUT_SHADER *const base = M_GetVariantBase(shader, variant_idx);
    if (tint.r == shader->tint[variant_idx].r
        && tint.g == shader->tint[variant_idx].g
        && tint.b == shader->tint[variant_idx].b) {
        return;
    }
    TRX_GL_TRACK_UNIFORM(
        glUniform3f, Output_Shader_LookupUniform(base, "uTint"), tint.r, tint.g,
        tint.b);
    shader->tint[variant_idx] = tint;
}
