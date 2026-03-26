#define M_MAX_LIGHTS 32

#include <trx/game/output/uniforms.h>

#include <trx/config.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/utils.h>
#include <trx/core/vector.h>
#include <trx/debug.h>
#include <trx/game/output.h>
#include <trx/game/output/utils.h>
#include <trx/game/rooms.h>
#include <trx/gl/utils.h>
#include <trx/version.h>

#include <math.h>
#include <string.h>

#define M_GLOBAL_MEMBERS                                                       \
    X_DECLARE_MEMBER(float, global_tint, [4])                                  \
    X_DECLARE_MEMBER(float, fog_color, [4])                                    \
    X_DECLARE_MEMBER(float, fog_distance, [2])                                 \
    X_DECLARE_MEMBER(float, viewport_size, [2])                                \
    X_DECLARE_MEMBER(float, time)                                              \
    X_DECLARE_MEMBER(float, time_in_game)                                      \
    X_DECLARE_MEMBER(float, brightness_multiplier)                             \
    X_DECLARE_MEMBER(float, ui_brightness_multiplier)                          \
    X_DECLARE_MEMBER(float, gamma)                                             \
    X_DECLARE_MEMBER(float, desaturation)                                      \
    X_DECLARE_MEMBER(float, sunset_duration)                                   \
    X_DECLARE_MEMBER(float, min_shade)                                         \
    X_DECLARE_MEMBER(int, billboard_lock_mode)                                 \
    X_DECLARE_MEMBER(int, lighting_enabled)                                    \
    X_DECLARE_MEMBER(int, trapezoid_filter_enabled)                            \
    X_DECLARE_MEMBER(int, reflections_enabled)                                 \
    X_DECLARE_MEMBER(int, textures_enabled)                                    \
    X_DECLARE_MEMBER(int, tr_version)

#pragma pack(push, 4)
typedef struct {
#define X_DECLARE_MEMBER(a, b, ...) a b __VA_ARGS__;
    M_GLOBAL_MEMBERS
#undef X_DECLARE_MEMBER
} M_UNIFORM_GENERAL;

typedef struct {
    float mat_proj[4][4];
    float mat_view[4][4];
} M_UNIFORM_MATRICES;

typedef struct {
    float pos[4];
    float color[4];
    float shade;
    float falloff;
    float kind;
    float _pad;
} M_UNIFORM_LIGHT;

typedef struct {
    int num_lights;
    int room_light_mode;
    int _pad[2];
    M_UNIFORM_LIGHT lights[M_MAX_LIGHTS];
} M_UNIFORM_LIGHTS;

typedef struct {
    float adder;
    float divider;
    float _pad0[2];
    float vector_view[4];
    float tr3_ambient[4];
    float tr3_light_dir_view[3][4];
    float tr3_light_color[3][4];
} M_UNIFORM_LS;
#pragma pack(pop)

typedef enum {
    M_LS_MODE_NONE = 0,
    M_LS_MODE_FULL = 1,
    M_LS_MODE_OWN = 2,
} M_LS_MODE;

typedef struct {
    M_UNIFORM_LIGHTS last_lights;
    OUTPUT_LIGHT_INFO last_light_info;
    M_LS_MODE last_ls_mode;
    int32_t last_own_light_adder;
    RGB_F last_own_light_tr3_ambient;
} M_PRIV;

static void M_FillLight(
    M_UNIFORM_LIGHT *const dst_light, const LIGHT *const src_light)
{
    dst_light->pos[0] = src_light->pos.x;
    dst_light->pos[1] = src_light->pos.y;
    dst_light->pos[2] = src_light->pos.z;
    dst_light->pos[3] = 0.0f;
    dst_light->color[0] = src_light->color.r / 255.0f;
    dst_light->color[1] = src_light->color.g / 255.0f;
    dst_light->color[2] = src_light->color.b / 255.0f;
    dst_light->color[3] = 0.0f;
    dst_light->shade = src_light->shade.value_1;
    dst_light->falloff = src_light->falloff.value_1;
    dst_light->kind = src_light->type;
}

static int16_t M_GetMinShade(void)
{
    switch (g_Config.rendering.lighting_contrast) {
    case LIGHTING_CONTRAST_LOW:
        return SHADE_NEUTRAL;
    case LIGHTING_CONTRAST_MEDIUM:
        return SHADE_HIGH;
    case LIGHTING_CONTRAST_HIGH:
        return 0;
    default:
        return SHADE_NEUTRAL;
    }
}

void Output_Uniforms_UploadOrthoMatrix(const OUTPUT_UNIFORMS *const uniforms)
{
    M_UNIFORM_MATRICES matrices = {};
    Output_GetOrthoProjectionMatrix(matrices.mat_proj);
    Output_FillMatrix(matrices.mat_view, &g_IDMatrix);

    glBindBuffer(GL_UNIFORM_BUFFER, uniforms->matrices);
    TRX_GL_TRACK_SUBDATA(
        glBufferSubData, GL_UNIFORM_BUFFER, 0, sizeof(matrices), &matrices);
}

void Output_Uniforms_UploadViewMatrix(
    const OUTPUT_UNIFORMS *const uniforms, const MATRIX *const matrix)
{
    M_UNIFORM_MATRICES matrices = {};
    Output_GetPerspProjectionMatrix(matrices.mat_proj);
    Output_FillMatrix(matrices.mat_view, matrix);

    glBindBuffer(GL_UNIFORM_BUFFER, uniforms->matrices);
    TRX_GL_TRACK_SUBDATA(
        glBufferSubData, GL_UNIFORM_BUFFER, 0, sizeof(matrices), &matrices);
}

void Output_Uniforms_UploadGeneral(const OUTPUT_UNIFORMS *const uniforms)
{
    M_UNIFORM_GENERAL general = {
        .time = Output_GetTime(),
        .time_in_game = Output_GetTimeInGame(),
        .brightness_multiplier = g_Config.visuals.game_brightness,
        .ui_brightness_multiplier = g_Config.visuals.ui_brightness,
        .gamma = g_Config.visuals.gamma,
        .desaturation = Output_GetDesaturation(),
        .sunset_duration = Output_GetSunsetDuration(),
        .tr_version = g_TRVersion,
        .viewport_size = {
            (float)Viewport_GetWidth(VIEWPORT_GAME),
            (float)Viewport_GetHeight(VIEWPORT_GAME),
        },
        .min_shade = M_GetMinShade(),
        .billboard_lock_mode = g_Config.rendering.sprite_lock_mode,
        .lighting_enabled = g_Config.rendering.enable_lighting,
        .textures_enabled = g_Config.rendering.enable_textures,
        .trapezoid_filter_enabled = g_Config.rendering.enable_trapezoid_filter,
        .reflections_enabled = g_Config.visuals.enable_reflections,
        .fog_distance = {Output_GetFogStart(), Output_GetFogEnd()},
        .fog_color = {
            Output_GetFogColor().r,
            Output_GetFogColor().g,
            Output_GetFogColor().b,
            Output_GetFogColor().a,
        },
        .global_tint = {
            Output_GetGlobalTint().r,
            Output_GetGlobalTint().g,
            Output_GetGlobalTint().b,
            1.0f,
        },
    };

    glBindBuffer(GL_UNIFORM_BUFFER, uniforms->general);
    TRX_GL_TRACK_SUBDATA(
        glBufferSubData, GL_UNIFORM_BUFFER, 0, sizeof(general), &general);
}

void Output_Uniforms_UploadFogDistance(
    const OUTPUT_UNIFORMS *const uniforms, const float start, const float end)
{
    ASSERT(uniforms != nullptr);
    const float fog_distance[2] = { start, end };
    glBindBuffer(GL_UNIFORM_BUFFER, uniforms->general);
    TRX_GL_TRACK_SUBDATA(
        glBufferSubData, GL_UNIFORM_BUFFER,
        offsetof(M_UNIFORM_GENERAL, fog_distance), sizeof(fog_distance),
        &fog_distance);
}

void Output_Uniforms_UploadDesaturation(
    const OUTPUT_UNIFORMS *const uniforms, const float desaturation)
{
    ASSERT(uniforms != nullptr);
    float clamped = desaturation;
    CLAMP(clamped, 0.0f, 1.0f);
    glBindBuffer(GL_UNIFORM_BUFFER, uniforms->general);
    TRX_GL_TRACK_SUBDATA(
        glBufferSubData, GL_UNIFORM_BUFFER,
        offsetof(M_UNIFORM_GENERAL, desaturation), sizeof(clamped), &clamped);
}

void Output_Uniforms_UploadGlobalTint(
    const OUTPUT_UNIFORMS *const uniforms, const RGB_F tint)
{
    ASSERT(uniforms != nullptr);
    glBindBuffer(GL_UNIFORM_BUFFER, uniforms->general);
    TRX_GL_TRACK_SUBDATA(
        glBufferSubData, GL_UNIFORM_BUFFER,
        offsetof(M_UNIFORM_GENERAL, global_tint), sizeof(tint), &tint);
}

void Output_Uniforms_UploadGameBrightnessMultiplier(
    const OUTPUT_UNIFORMS *const uniforms,
    const float game_brightness_multiplier)
{
    ASSERT(uniforms != nullptr);

    float clamped = game_brightness_multiplier;
    CLAMP(clamped, CONFIG_MIN_BRIGHTNESS, CONFIG_MAX_BRIGHTNESS);

    glBindBuffer(GL_UNIFORM_BUFFER, uniforms->general);
    TRX_GL_TRACK_SUBDATA(
        glBufferSubData, GL_UNIFORM_BUFFER,
        offsetof(M_UNIFORM_GENERAL, brightness_multiplier), sizeof(clamped),
        &clamped);
}

void Output_Uniforms_UploadUIBrightnessMultiplier(
    const OUTPUT_UNIFORMS *const uniforms, const float brightness_multiplier)
{
    ASSERT(uniforms != nullptr);

    float clamped = brightness_multiplier;
    CLAMP(clamped, CONFIG_MIN_BRIGHTNESS, CONFIG_MAX_BRIGHTNESS);

    glBindBuffer(GL_UNIFORM_BUFFER, uniforms->general);
    TRX_GL_TRACK_SUBDATA(
        glBufferSubData, GL_UNIFORM_BUFFER,
        offsetof(M_UNIFORM_GENERAL, ui_brightness_multiplier), sizeof(clamped),
        &clamped);
}

void Output_Uniforms_UploadRoomLights(
    const OUTPUT_UNIFORMS *const uniforms, const ROOM *const room)
{
    M_UNIFORM_LIGHTS lights = {};

    // Only dynamic lights for now.
    M_UNIFORM_LIGHT *dst_light = lights.lights;
    if (room == nullptr) {
        lights.room_light_mode = RLM_SUNSET;
    } else {
        lights.room_light_mode = room->light_mode;
    }
    VECTOR *const dynamic_lights = Output_GetDynamicLights();
    for (int32_t i = 0; i < dynamic_lights->count; i++) {
        M_FillLight(dst_light, Vector_Get(dynamic_lights, i));
        dst_light++;
        if (dst_light - lights.lights >= M_MAX_LIGHTS) {
            break;
        }
    }

    lights.num_lights = dst_light - lights.lights;
    const size_t size = offsetof(M_UNIFORM_LIGHTS, lights)
        + lights.num_lights * sizeof(M_UNIFORM_LIGHT);

    M_PRIV *const p = uniforms->priv;
    if (memcmp(&p->last_lights, &lights, sizeof(lights)) == 0) {
        return;
    }
    memcpy(&p->last_lights, &lights, sizeof(lights));

    glBindBuffer(GL_UNIFORM_BUFFER, uniforms->lights);
    TRX_GL_TRACK_SUBDATA(glBufferSubData, GL_UNIFORM_BUFFER, 0, size, &lights);
}

void Output_Uniforms_UploadCPULight(
    const OUTPUT_UNIFORMS *const uniforms, const OUTPUT_LIGHT_INFO *const info)
{
    M_PRIV *const p = uniforms->priv;
    if (p->last_ls_mode == M_LS_MODE_FULL
        && memcmp(&p->last_light_info, info, sizeof(*info)) == 0) {
        return;
    }
    memcpy(&p->last_light_info, info, sizeof(*info));
    p->last_own_light_adder = info->ls_adder;
    p->last_own_light_tr3_ambient = info->tr3_ambient;
    p->last_ls_mode = M_LS_MODE_FULL;

    M_UNIFORM_LS ls = {};
    ls.adder = info->ls_adder;
    ls.divider = info->ls_divider / (float)(1 << (W2V_SHIFT));
    ls.vector_view[0] = info->ls_vector_view.x;
    ls.vector_view[1] = info->ls_vector_view.y;
    ls.vector_view[2] = info->ls_vector_view.z;
    ls.vector_view[3] = 0;
    ls.tr3_ambient[0] = info->tr3_ambient.r;
    ls.tr3_ambient[1] = info->tr3_ambient.g;
    ls.tr3_ambient[2] = info->tr3_ambient.b;
    ls.tr3_ambient[3] = 0.0f;
    for (int32_t i = 0; i < 3; i++) {
        float x = (float)info->tr3_light_dir_view[i].x;
        float y = (float)info->tr3_light_dir_view[i].y;
        float z = (float)info->tr3_light_dir_view[i].z;
        const float len2 = x * x + y * y + z * z;
        if (len2 > 0.0f) {
            const float inv_len = 1.0f / sqrtf(len2);
            x *= inv_len;
            y *= inv_len;
            z *= inv_len;
        }
        ls.tr3_light_dir_view[i][0] = x;
        ls.tr3_light_dir_view[i][1] = y;
        ls.tr3_light_dir_view[i][2] = z;
        ls.tr3_light_dir_view[i][3] = 0.0f;
        ls.tr3_light_color[i][0] = info->tr3_light_color[i].r;
        ls.tr3_light_color[i][1] = info->tr3_light_color[i].g;
        ls.tr3_light_color[i][2] = info->tr3_light_color[i].b;
        ls.tr3_light_color[i][3] = 0.0f;
    }

    glBindBuffer(GL_UNIFORM_BUFFER, uniforms->ls);
    TRX_GL_TRACK_SUBDATA(
        glBufferSubData, GL_UNIFORM_BUFFER, 0, sizeof(ls), &ls);
    TRX_GL_CheckError();
}

void Output_Uniforms_UploadOwnLight(
    const OUTPUT_UNIFORMS *const uniforms, const OUTPUT_LIGHT_INFO *const info)
{
    M_PRIV *const priv = uniforms->priv;

    if (g_TRVersion >= 3) {
        if (priv->last_ls_mode == M_LS_MODE_OWN
            && priv->last_own_light_tr3_ambient.r == info->tr3_ambient.r
            && priv->last_own_light_tr3_ambient.g == info->tr3_ambient.g
            && priv->last_own_light_tr3_ambient.b == info->tr3_ambient.b) {
            return;
        }

        const float ambient[4] = {
            info->tr3_ambient.r,
            info->tr3_ambient.g,
            info->tr3_ambient.b,
            0.0f,
        };
        glBindBuffer(GL_UNIFORM_BUFFER, uniforms->ls);
        TRX_GL_TRACK_SUBDATA(
            glBufferSubData, GL_UNIFORM_BUFFER,
            offsetof(M_UNIFORM_LS, tr3_ambient), sizeof(ambient), ambient);
        priv->last_own_light_tr3_ambient = info->tr3_ambient;
    } else {
        if (priv->last_ls_mode == M_LS_MODE_OWN
            && priv->last_own_light_adder == info->ls_adder) {
            return;
        }

        const float light_adder = info->ls_adder;
        glBindBuffer(GL_UNIFORM_BUFFER, uniforms->ls);
        TRX_GL_TRACK_SUBDATA(
            glBufferSubData, GL_UNIFORM_BUFFER, offsetof(M_UNIFORM_LS, adder),
            sizeof(light_adder), &light_adder);
        priv->last_own_light_adder = info->ls_adder;
    }
    priv->last_ls_mode = M_LS_MODE_OWN;
}

OUTPUT_UNIFORMS *Output_Uniforms_Create(void)
{
    OUTPUT_UNIFORMS *const uniforms =
        Memory_Alloc(sizeof(OUTPUT_UNIFORMS) + sizeof(M_PRIV));
    glGenBuffers(4, &uniforms->general);
    glBindBuffer(GL_UNIFORM_BUFFER, uniforms->general);
    glBufferData(
        GL_UNIFORM_BUFFER, sizeof(M_UNIFORM_GENERAL), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniforms->general);

    glBindBuffer(GL_UNIFORM_BUFFER, uniforms->matrices);
    glBufferData(
        GL_UNIFORM_BUFFER, sizeof(M_UNIFORM_MATRICES), nullptr,
        GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, uniforms->matrices);

    glBindBuffer(GL_UNIFORM_BUFFER, uniforms->lights);
    glBufferData(
        GL_UNIFORM_BUFFER, sizeof(M_UNIFORM_LIGHTS), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 2, uniforms->lights);

    glBindBuffer(GL_UNIFORM_BUFFER, uniforms->ls);
    glBufferData(
        GL_UNIFORM_BUFFER, sizeof(M_UNIFORM_LS), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 3, uniforms->ls);
    TRX_GL_CheckError();

    uniforms->priv = (char *)uniforms + sizeof(OUTPUT_UNIFORMS);
    return uniforms;
}

void Output_Uniforms_Free(OUTPUT_UNIFORMS *const uniforms)
{
    if (uniforms == nullptr) {
        return;
    }
    if (uniforms->general != 0) {
        glDeleteBuffers(4, &uniforms->general);
        uniforms->general = 0;
        uniforms->matrices = 0;
        uniforms->lights = 0;
        uniforms->ls = 0;
    }
    Memory_Free(uniforms);
}
