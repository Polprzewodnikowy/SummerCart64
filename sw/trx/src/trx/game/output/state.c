#include <trx/game/output/state.h>

#include <trx/config.h>
#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/const.h>
#include <trx/game/output/common.h>
#include <trx/game/output/lights.h>
#include <trx/game/output/scene_compositor.h>
#include <trx/game/output/sources/objects.h>
#include <trx/game/output/sources/rooms.h>
#include <trx/game/output/textures.h>
#include <trx/game/output/uniforms.h>
#include <trx/game/viewport.h>
#include <trx/version.h>

#include <math.h>

static float m_Time = 0.0f;
static float m_TimeInGame = 0.0f;
static bool m_ControlFrame = false;
static int32_t m_AnimatedTexturesOffset = 0;

static int32_t m_FogStart = 0;
static int32_t m_FogEnd = 0;
static RGBA_F m_FogColor = {};
static RGB_F m_WaterColor = {};

static float m_DepthFactor = 0.0f;
static float m_DepthUnits = 0.0f;

static const ROOM *m_CurrentRoom = nullptr;
static int32_t m_LsAdder = 0;
static int32_t m_LsDivider = 0;
static XYZ_32 m_LsVectorView = {};
static RGB_F m_TR3Ambient = { 1.0f, 1.0f, 1.0f };
static RGB_F m_TR3LightColor[3] = {};
static XYZ_32 m_TR3LightDirView[3] = {};

static bool m_IsWibbleEffect = false;
static bool m_IsWaterEffect = false;
static bool m_IsShadeEffect = false;
static bool m_IsSkyboxEnabled = false;
static float m_Desaturation = 0.0f;

static int32_t m_TintOverrideDepth = 0;
static RGB_F m_TintOverrideStack[8] = {};

static RGB_F m_GlobalTint = { 1.0f, 1.0f, 1.0f };

float Output_GetTime(void)
{
    return m_Time;
}

float Output_GetTimeInGame(void)
{
    return m_TimeInGame;
}

void Output_SetTimeInGame(const float time)
{
    m_TimeInGame = time;
}

int32_t Output_GetNearZ(void)
{
    return 20 << W2V_SHIFT;
}

int32_t Output_GetFarZ(void)
{
    return Output_GetFogEnd() << W2V_SHIFT;
}

int32_t Output_GetNearZ_UI(void)
{
    return 20;
}

int32_t Output_GetFarZ_UI(void)
{
    return 10000;
}

void Output_SetSkyboxEnabled(const bool enabled)
{
    m_IsSkyboxEnabled = enabled;
}

bool Output_IsSkyboxEnabled(void)
{
    return m_IsSkyboxEnabled && g_Config.visuals.enable_skybox;
}

RGBA_F Output_GetFogColor(void)
{
    return m_FogColor;
}

int32_t Output_GetFogStart(void)
{
    return MIN(m_FogStart, Output_GetFogEnd());
}

int32_t Output_GetFogEnd(void)
{
    return m_FogEnd;
}

void Output_SetFogStart(const int32_t dist)
{
    m_FogStart = dist;
    const OUTPUT_UNIFORMS *const uniforms = Output_GetUniforms();
    if (uniforms != nullptr) {
        Output_Uniforms_UploadFogDistance(
            uniforms, Output_GetFogStart(), Output_GetFogEnd());
    }
}

void Output_SetFogEnd(const int32_t dist)
{
    m_FogEnd = dist;
    const OUTPUT_UNIFORMS *const uniforms = Output_GetUniforms();
    if (uniforms != nullptr) {
        Output_Uniforms_UploadFogDistance(
            uniforms, Output_GetFogStart(), Output_GetFogEnd());
    }
}

void Output_SetupBelowWater(const bool underwater)
{
    m_IsWaterEffect = true;
    m_IsWibbleEffect = !underwater;
    m_IsShadeEffect = true;
}

void Output_SetupAboveWater(const bool underwater)
{
    m_IsWaterEffect = false;
    m_IsWibbleEffect = underwater;
    m_IsShadeEffect = underwater;
}

bool Output_GetWaterEffect(void)
{
    return m_IsWaterEffect;
}

bool Output_GetWibbleEffect(void)
{
    return m_IsWibbleEffect;
}

float Output_GetDesaturation(void)
{
    return m_Desaturation;
}

void Output_SetDesaturation(const float desaturation)
{
    if (m_Desaturation == desaturation) {
        return;
    }
    m_Desaturation = desaturation;

    const OUTPUT_UNIFORMS *const uniforms = Output_GetUniforms();
    if (uniforms != nullptr) {
        Output_Uniforms_UploadDesaturation(uniforms, m_Desaturation);
    }
}

void Output_SetFogColor(const RGBA_8888 color)
{
    m_FogColor.r = color.r / 255.0f;
    m_FogColor.g = color.g / 255.0f;
    m_FogColor.b = color.b / 255.0f;
    m_FogColor.a = color.a / 255.0f;
}

RGB_F Output_GetWaterColor(void)
{
    return m_WaterColor;
}

void Output_SetWaterColor(const RGB_888 color)
{
    m_WaterColor.r = color.r / 255.0f;
    m_WaterColor.g = color.g / 255.0f;
    m_WaterColor.b = color.b / 255.0f;
}

RGB_F Output_GetGlobalTint(void)
{
    return m_GlobalTint;
}

void Output_SetGlobalTint(const RGB_F tint)
{
    m_GlobalTint = tint;
    const OUTPUT_UNIFORMS *const uniforms = Output_GetUniforms();
    if (uniforms != nullptr) {
        Output_Uniforms_UploadGlobalTint(uniforms, m_GlobalTint);
    }
}

RGB_F Output_GetTint(void)
{
    if (m_TintOverrideDepth != 0) {
        return m_TintOverrideStack[m_TintOverrideDepth - 1];
    }
    if (m_IsShadeEffect) {
        return m_WaterColor;
    }
    return COLOR_RGB_F_WHITE;
}

void Output_PushTintOverride(const RGB_F tint)
{
    ASSERT(m_TintOverrideDepth < (int32_t)ARRAY_SIZE(m_TintOverrideStack));
    m_TintOverrideStack[m_TintOverrideDepth++] = tint;
}

void Output_PopTintOverride(void)
{
    ASSERT(m_TintOverrideDepth > 0);
    m_TintOverrideDepth--;
}

void Output_GetPerspProjectionMatrix(GLfloat output[][4])
{
    const float left = Viewport_GetMinX(VIEWPORT_GAME);
    const float top = Viewport_GetMinY(VIEWPORT_GAME);
    const float right = Viewport_GetWidth(VIEWPORT_GAME);
    const float bottom = Viewport_GetHeight(VIEWPORT_GAME);
    const float near = Output_GetNearZ() / (float)(1 << W2V_SHIFT);
    const float far = Output_GetFarZ() / (float)(1 << W2V_SHIFT);
    const float aspect = (float)(right - left) / (float)(bottom - top);
    const float fov = Viewport_GetEffectiveFOV() * M_PI / (float)DEG_180;

    float f_x, f_y;
    switch (Viewport_GetFOVMode()) {
    case FOV_MODE_HORIZONTAL:
        f_x = 1.0f / tanf(fov * 0.5f);
        f_y = f_x * aspect;
        break;
    case FOV_MODE_VERTICAL:
        f_y = 1.0f / tanf(fov * 0.5f);
        f_x = f_y / aspect;
        break;
    case FOV_MODE_PC: {
        const float persp = ((4.0f / 3.0f) / aspect);
        f_x = persp / tanf(fov * 0.5f);
        f_y = f_x * aspect;
        break;
    }
    case FOV_MODE_PS1: {
        const float persp = ((4.0f / 3.0f) / aspect) * (240.0f / 200.0f);
        f_x = persp / tanf(fov * 0.5f);
        f_y = f_x * aspect;
        break;
    }
    default:
        ASSERT_FAIL();
    }

    const float near_z = Output_GetNearZ();
    const float far_z = Output_GetFarZ();
    const float res_z = 0.99 * near_z * far_z / (far_z - near_z);

    output[0][0] = f_x;
    output[0][1] = 0.0f;
    output[0][2] = 0.0f;
    output[0][3] = 0.0f;

    output[1][0] = 0.0f;
    output[1][1] = -f_y;
    output[1][2] = 0.0f;
    output[1][3] = 0.0f;

    output[2][0] = 0.0f;
    output[2][1] = 0.0f;
    output[2][2] = 0.005 + res_z / near_z;
    output[2][3] = 1.0f;

    output[3][0] = 0.0f;
    output[3][1] = 0.0f;
    output[3][2] = -res_z / (float)(1 << W2V_SHIFT);
    output[3][3] = 0.0f;
}

void Output_GetOrthoProjectionMatrix(GLfloat output[][4])
{
    const float left = 0.0f;
    const float top = 0.0f;
    const float right = Viewport_GetWidth(VIEWPORT_UI);
    const float bottom = Viewport_GetHeight(VIEWPORT_UI);
    const float near = Output_GetNearZ_UI();
    const float far = Output_GetFarZ_UI();

    output[0][0] = 2.0f / (right - left);
    output[0][1] = 0.0f;
    output[0][2] = 0.0f;
    output[0][3] = 0.0f;

    output[1][0] = 0.0f;
    output[1][1] = 2.0f / (top - bottom);
    output[1][2] = 0.0f;
    output[1][3] = 0.0f;

    output[2][0] = 0.0f;
    output[2][1] = 0.0f;
    output[2][2] = 2.0f / (far - near);
    output[2][3] = 0.0f;

    output[3][0] = -(right + left) / (right - left);
    output[3][1] = -(top + bottom) / (top - bottom);
    output[3][2] = -(far + near) / (far - near);
    output[3][3] = 1.0f;
}

void Output_SetCurrentRoom(const ROOM *const room)
{
    m_CurrentRoom = room;
}

const ROOM *Output_GetCurrentRoom(void)
{
    return m_CurrentRoom;
}

int32_t Output_GetLightAdder(void)
{
    return m_LsAdder;
}

void Output_SetLightAdder(const int32_t adder)
{
    m_LsAdder = adder;
}

int32_t Output_GetLightDivider(void)
{
    return m_LsDivider;
}

void Output_SetLightDivider(const int32_t divider)
{
    m_LsDivider = divider;
}

XYZ_32 Output_GetLightVectorView(void)
{
    return m_LsVectorView;
}

OUTPUT_LIGHT_INFO Output_GetLightInfo(void)
{
    OUTPUT_LIGHT_INFO info = {
        .ls_adder = m_LsAdder,
        .ls_divider = m_LsDivider,
        .ls_vector_view = m_LsVectorView,
        .tr3_ambient = m_TR3Ambient,
    };
    for (int32_t i = 0; i < 3; i++) {
        info.tr3_light_color[i] = m_TR3LightColor[i];
        info.tr3_light_dir_view[i] = m_TR3LightDirView[i];
    }
    return info;
}

void Output_RotateLight(const int16_t pitch, const int16_t yaw)
{
    const int32_t cp = Math_Cos(pitch);
    const int32_t sp = Math_Sin(pitch);
    const int32_t cy = Math_Cos(yaw);
    const int32_t sy = Math_Sin(yaw);
    const int32_t x = TRIGMULT2(cp, sy);
    const int32_t y = -sp;
    const int32_t z = TRIGMULT2(cp, cy);
    const MATRIX *const m = &g_ViewMatrix;
    m_LsVectorView.x = (m->_00 * x + m->_01 * y + m->_02 * z) >> W2V_SHIFT;
    m_LsVectorView.y = (m->_10 * x + m->_11 * y + m->_12 * z) >> W2V_SHIFT;
    m_LsVectorView.z = (m->_20 * x + m->_21 * y + m->_22 * z) >> W2V_SHIFT;
}

void Output_SetTR3Light(
    const RGB_F ambient, const RGB_F colors[3], const XYZ_32 dirs_view[3])
{
    m_TR3Ambient = ambient;
    for (int32_t i = 0; i < 3; i++) {
        m_TR3LightColor[i] = colors[i];
        m_TR3LightDirView[i] = dirs_view[i];
    }
}

void Output_SetTime(const float time)
{
    m_Time = time;
}

void Output_AnimateTextures(int32_t num_frames)
{
    const int32_t anim_delta = g_TRVersion == 3 ? 2 : 1;

    m_TimeInGame += num_frames;
    m_AnimatedTexturesOffset += num_frames * anim_delta;
    bool update = false;
    while (m_AnimatedTexturesOffset > 5) {
        Output_CycleAnimatedTextures();
        update = true;
        m_AnimatedTexturesOffset -= 5;
    }
    if (update) {
        Output_Textures_CycleAnimations();
        SceneCompositor_AnimateTextures();
    }

    Output_AnimateLights(num_frames);
}

void Output_EnableScissor(
    const float x, const float y, const float w, const float h)
{
    // Causes the rendering pipeline to discard every pixel outside of the
    // specified window. The window is in game framebuffer viewport's
    // coordinates; to make it work properly, we need to translate it to the
    // SDL window coordinates first.

    // To deal with precision issues coming from using integer matrix ops
    const int32_t border = 4;

    const VIEWPORT_RECT game = Viewport_GetRect(VIEWPORT_GAME);
    const VIEWPORT_RECT window = Viewport_GetRect(VIEWPORT_GAME);
    const float scale_x = window.w / (float)game.w;
    const float scale_y = window.h / (float)game.h;
    VIEWPORT_RECT scissor = {
        .x = window.x + (x * scale_x) - border,
        .y = window.y + (game.h - y) * scale_y - border,
        .w = w * scale_x + border * 2,
        .h = h * scale_y + border * 2,
    };

    glEnable(GL_SCISSOR_TEST);
    glScissor(scissor.x, scissor.y, scissor.w, scissor.h);
}

void Output_DisableScissor(void)
{
    glDisable(GL_SCISSOR_TEST);
}

void Output_AdjustDepth(const float factor, const float units)
{
    if (factor != m_DepthFactor || units != m_DepthUnits) {
        glPolygonOffset(factor, units);
        m_DepthFactor = factor;
        m_DepthUnits = units;
    }
}

bool Output_IsControlFrame(void)
{
    return m_ControlFrame;
}

void Output_SetControlFrame(const bool is_control_frame)
{
    m_ControlFrame = is_control_frame;
}
