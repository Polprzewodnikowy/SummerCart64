#include <trx/game/camera/common.h>

#include <trx/config.h>
#include <trx/game/camera.h>
#include <trx/game/input.h>
#include <trx/game/lara.h>
#include <trx/game/matrix.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>

#define M_CHASE_ELEVATION (WALL_L * 3 / 2) // = 1536

static CAMERA_STRATEGY m_Strategies[CAMERA_MODE_NUMBER_OF] = {};

// Camera speed option ranges from 1-10, so index 0 is unused.
static const double m_ManualCameraMultiplier[11] = {
    1.0, .5, .625, .75, .875, 1.0, 1.2, 1.4, 1.6, 1.8, 2.0,
};

static bool m_IsChunky = false;
static bool m_IsInitialised = false;

static void M_OffsetAdditionalAngle(const int16_t delta)
{
    g_Camera.additional_angle += delta;
}

static void M_OffsetAdditionalElevation(const int16_t delta)
{
    // Do not allow elevation to overflow.
    int32_t new_elevation = g_Camera.additional_elevation + delta;
    CLAMP(new_elevation, INT16_MIN, INT16_MAX);
    g_Camera.additional_elevation = new_elevation;
}

static void M_OffsetReset(void)
{
    g_Camera.additional_angle = 0;
    g_Camera.additional_elevation = 0;
}

static const CAMERA_STRATEGY *M_GetStrategy(void)
{
    return &m_Strategies[g_Config.visuals.camera_mode];
}

const CAMERA_LOOK_SETTINGS *Camera_GetLookSettings(const bool on_surface)
{
    return M_GetStrategy()->get_look_settings_func(on_surface);
}

void Camera_RegisterStrategy(
    const CAMERA_MODE mode, const CAMERA_STRATEGY strategy)
{
    m_Strategies[mode] = strategy;
}

bool Camera_IsChunky(void)
{
    return m_IsChunky;
}

void Camera_SetChunky(const bool is_chunky)
{
    m_IsChunky = is_chunky;
}

void Camera_Initialise(void)
{
    m_IsInitialised = false;
    Matrix_ResetStack();
    g_Camera.last = NO_CAMERA;
    g_Camera.underwater = false;
    Camera_ResetPosition();
    Camera_Update();
    m_IsInitialised = true;
}

void Camera_ResetPosition(void)
{
    const CAMERA_STRATEGY *const strategy = M_GetStrategy();
    strategy->reset_func();

    g_Camera.roll = 0;
    g_Camera.target_distance = CAMERA_DEFAULT_DISTANCE;
    g_Camera.item = nullptr;
    g_Camera.speed = 1;
    g_Camera.flags = CF_NORMAL;
    g_Camera.bounce = 0;
    g_Camera.num = NO_CAMERA;
    g_Camera.fixed_camera = false;
    g_Camera.additional_angle = 0;
    g_Camera.additional_elevation = 0;
    const LARA_INFO *const lara_info = Lara_GetLaraInfo();
    if (!lara_info->extra_anim) {
        g_Camera.type = CAM_CHASE;
    }
}

void Camera_Reset(void)
{
    g_Camera.mic_pos.room_num = NO_ROOM;
    g_Camera.pos.room_num = NO_ROOM;
}

void Camera_ApplyBounce(void)
{
    if (g_Camera.bounce > 0) {
        g_Camera.pos.y += g_Camera.bounce;
        g_Camera.target.y += g_Camera.bounce;
        g_Camera.bounce = 0;
    } else if (g_Camera.bounce < 0) {
        const XYZ_32 shake = {
            .x = g_Camera.bounce * (Random_GetControl() - 0x4000) / 0x7FFF,
            .y = g_Camera.bounce * (Random_GetControl() - 0x4000) / 0x7FFF,
            .z = g_Camera.bounce * (Random_GetControl() - 0x4000) / 0x7FFF,
        };
        g_Camera.pos.x += shake.x;
        g_Camera.pos.y += shake.y;
        g_Camera.pos.z += shake.z;
        g_Camera.target.y += shake.x;
        g_Camera.target.y += shake.y;
        g_Camera.target.z += shake.z;
        g_Camera.bounce += 5;
    }
}

void Camera_ClampInterpResult(void)
{
    if (g_Camera.type == CAM_PHOTO_MODE) {
        Room_GetSector(
            (XYZ_32) {
                g_Camera.interp.result.pos.x,
                g_Camera.interp.result.pos.y + g_Camera.interp.result.shift,
                g_Camera.interp.result.pos.z,
            },
            &g_Camera.interp.room_num);
        return;
    }

    const CAMERA_STRATEGY *const strategy = M_GetStrategy();
    strategy->clamp_result_func();
}

void Camera_Update(void)
{
    if (g_Camera.type == CAM_PHOTO_MODE) {
        Camera_PhotoMode_Update();
        Camera_EnsureEnvironment();
        return;
    }

    if (g_Camera.type == CAM_CINEMATIC) {
        Camera_LoadCutsceneFrame();
        Camera_EnsureEnvironment();
        return;
    }

    if (g_Camera.flags != CF_NO_CHUNKY) {
        Camera_SetChunky(true);
    }

    const bool fixed_camera = g_Camera.item != nullptr
        && (g_Camera.type == CAM_FIXED || g_Camera.type == CAM_HEAVY);
    const ITEM *const item = fixed_camera ? g_Camera.item : Lara_GetItem();

    const BOUNDS_16 *const bounds = Item_GetBoundsAccurate(item);
    int32_t y = item->pos.y;
    if (fixed_camera) {
        y += (bounds->min.y + bounds->max.y) / 2;
    } else {
        y += bounds->max.y
            + (((int32_t)(bounds->min.y - bounds->max.y)) * 3 >> 2);
    }

    const CAMERA_STRATEGY *const strategy = M_GetStrategy();
    strategy->update_func(item, fixed_camera, y);

    g_Camera.last = g_Camera.num;
    g_Camera.fixed_camera = fixed_camera;

    switch (g_Camera.type) {
    case CAM_LOOK:
    case CAM_CINEMATIC:
    case CAM_COMBAT:
    case CAM_FIXED:
        g_Camera.additional_angle = 0;
        g_Camera.additional_elevation = 0;
        break;

    default:
        break;
    }

    if (g_Camera.type != CAM_HEAVY || g_Camera.timer == -1) {
        g_Camera.type = CAM_CHASE;
        g_Camera.num = NO_CAMERA;
        g_Camera.last_item = g_Camera.item;
        g_Camera.item = nullptr;
        g_Camera.target_angle = g_Camera.additional_angle;
        g_Camera.target_elevation = g_Camera.additional_elevation;
        g_Camera.target_distance = M_CHASE_ELEVATION;
        g_Camera.flags = CF_NORMAL;
        if (g_Config.visuals.camera_mode != CAMERA_MODE_TR1) {
            g_Camera.speed = strategy->get_chase_speed_func();
        }
    }
    Camera_SetChunky(false);
    if (m_IsInitialised) {
        Camera_EnsureEnvironment();
    }
}

void Camera_MoveManual(void)
{
    if (g_Input.camera_reset) {
        M_OffsetReset();
    }

    if (!g_Config.gameplay.enable_manual_camera) {
        return;
    }

    const int16_t camera_delta = (const int32_t)(DEG_90 / LOGIC_FPS)
        * (double)m_ManualCameraMultiplier[g_Config.gameplay.camera_speed];

    if (g_Input.camera_left) {
        M_OffsetAdditionalAngle(camera_delta);
    } else if (g_Input.camera_right) {
        M_OffsetAdditionalAngle(-camera_delta);
    }
    if (g_Input.camera_forward) {
        M_OffsetAdditionalElevation(-camera_delta);
    } else if (g_Input.camera_back) {
        M_OffsetAdditionalElevation(camera_delta);
    }
}

void Camera_Apply(void)
{
    Matrix_LookAt(
        g_Camera.interp.result.pos.x,
        g_Camera.interp.result.pos.y + g_Camera.interp.result.shift,
        g_Camera.interp.result.pos.z, g_Camera.interp.result.target.x,
        g_Camera.interp.result.target.y, g_Camera.interp.result.target.z,
        g_Camera.roll);
}
