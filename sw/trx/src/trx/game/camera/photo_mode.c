#include <trx/game/camera/photo_mode.h>

#include <trx/config.h>
#include <trx/core/math.h>
#include <trx/core/utils.h>
#include <trx/game/camera.h>
#include <trx/game/input.h>
#include <trx/game/lara/pose.h>
#include <trx/game/matrix.h>
#include <trx/game/output.h>
#include <trx/game/rooms.h>
#include <trx/game/viewport.h>

#define MIN_PHOTO_FOV 10
#define MAX_PHOTO_FOV 150
#define PHOTO_ROT_SHIFT (DEG_1 * 4)
#define PHOTO_MAX_PITCH_ROLL (DEG_90 - DEG_1)
#define PHOTO_MAX_SPEED 100

#define M_CosMul(a, b) TRIGMULT2(Math_Cos((a)), (b))
#define M_SinMul(a, b) TRIGMULT2(Math_Sin((a)), (b))

static int32_t m_PhotoSpeed = 0;
static int32_t m_OriginalFOV;
static FOV_MODE m_OriginalFOVMode;
static CAMERA_INFO m_OriginalCamera = {};
static int32_t m_CurrentFOV;
static FOV_MODE m_CurrentFOVMode;
static CAMERA_INFO m_StartingCamera = {};
static struct {
    bool is_chunky;
    int32_t fov;
    FOV_MODE fov_mode;
    CAMERA_INFO camera;
} m_PreviousState;
static BOUNDS_32 m_WorldBounds = {};

static void M_ResetCamera(const bool exiting)
{
    CAMERA_INFO camera = g_Camera;
    g_Camera = exiting ? m_OriginalCamera : m_StartingCamera;
    // ensure Camera_EnsureEnvironment() picks up the flag change
    g_Camera.underwater = camera.underwater;
    Viewport_AlterFOV(m_OriginalFOV, m_OriginalFOVMode);
    m_CurrentFOV = m_OriginalFOV / DEG_1;
}

static int32_t M_GetShiftSpeed(const int32_t val)
{
    return val * m_PhotoSpeed / (float)PHOTO_MAX_SPEED;
}

static int32_t M_GetRotSpeed(void)
{
    return MAX(DEG_1, M_GetShiftSpeed(PHOTO_ROT_SHIFT));
}

static XYZ_32 M_GetShift(const int32_t dx, const int32_t dy, const int32_t dz)
{
    const int16_t yaw = g_Camera.target_angle;
    const int16_t pitch = g_Camera.target_elevation;
    const int16_t roll = g_Camera.roll;

    const int32_t dx_r = M_CosMul(roll, dx) - M_SinMul(roll, dy);
    const int32_t dy_r = M_SinMul(roll, dx) + M_CosMul(roll, dy);
    const int32_t dz_r = dz; // unchanged if roll is around Z

    const int32_t dy_p = M_CosMul(pitch, dy_r) - M_SinMul(pitch, dz_r);
    const int32_t dz_p = M_SinMul(pitch, dy_r) + M_CosMul(pitch, dz_r);
    const int32_t dx_p = dx_r; // unchanged if pitch is around X

    const int32_t dx_y = M_CosMul(yaw, dx_p) + M_SinMul(yaw, dz_p);
    const int32_t dz_y = -M_SinMul(yaw, dx_p) + M_CosMul(yaw, dz_p);
    const int32_t dy_y = dy_p; // unchanged if yaw is around Y

    return (XYZ_32) { dx_y, dy_y, dz_y };
}

static void M_ShiftCamera(int32_t dx, int32_t dy, int32_t dz)
{
    const XYZ_32 shift = M_GetShift(dx, dy, dz);
    g_Camera.pos.x += shift.x;
    g_Camera.pos.y += shift.y;
    g_Camera.pos.z += shift.z;
    g_Camera.target.x += shift.x;
    g_Camera.target.y += shift.y;
    g_Camera.target.z += shift.z;
}

static void M_ApplyRotation(
    const int32_t d_yaw, const int32_t d_pitch, const int32_t d_roll,
    const bool respect_roll)
{
    int32_t yaw = g_Camera.target_angle;
    int32_t pitch = g_Camera.target_elevation;
    int32_t roll = g_Camera.roll;

    // rotate with respect to current upright axis
    if (respect_roll) {
        yaw += M_CosMul(roll, d_yaw) + M_SinMul(roll, d_pitch);
        pitch += M_CosMul(roll, d_pitch) - M_SinMul(roll, d_yaw);
    } else {
        yaw += d_yaw;
        pitch += d_pitch;
    }
    roll += d_roll;

    // handle pivoting
    if (pitch >= DEG_90 || pitch <= -DEG_90) {
        roll += DEG_180;
        yaw += DEG_180;
        pitch = g_Camera.target_elevation;
    }

    g_Camera.target_angle = yaw;
    g_Camera.target_elevation = pitch;
    g_Camera.roll = roll;
}

static void M_RotateCamera(
    const int32_t d_yaw, const int32_t d_pitch, const int32_t d_roll)
{
    M_ApplyRotation(d_yaw, d_pitch, d_roll, true);
    const XYZ_32 shift = M_GetShift(0, 0, g_Camera.target_distance);
    g_Camera.target.x = g_Camera.pos.x + shift.x;
    g_Camera.target.y = g_Camera.pos.y + shift.y;
    g_Camera.target.z = g_Camera.pos.z + shift.z;
}

static void M_RotateTarget(
    const int32_t d_yaw, const int32_t d_pitch, const int32_t d_roll)
{
    M_ApplyRotation(d_yaw, d_pitch, d_roll, false);
    const XYZ_32 shift = M_GetShift(0, 0, g_Camera.target_distance);
    g_Camera.pos.x = g_Camera.target.x - shift.x;
    g_Camera.pos.y = g_Camera.target.y - shift.y;
    g_Camera.pos.z = g_Camera.target.z - shift.z;
}

static void M_ClampCameraPos(void)
{
    // While the camera is free, we want to clamp to within overall world bounds
    // to help counteract getting lost in the void.
    const GAME_VECTOR prev_cam_pos = g_Camera.pos;
    CLAMP(g_Camera.pos.x, m_WorldBounds.min.x, m_WorldBounds.max.x);
    CLAMP(g_Camera.pos.y, m_WorldBounds.min.y, m_WorldBounds.max.y);
    CLAMP(g_Camera.pos.z, m_WorldBounds.min.z, m_WorldBounds.max.z);

    g_Camera.target.x += (g_Camera.pos.x - prev_cam_pos.x);
    g_Camera.target.y += (g_Camera.pos.y - prev_cam_pos.y);
    g_Camera.target.z += (g_Camera.pos.z - prev_cam_pos.z);
}

static bool M_CameraInsideRoom(const XYZ_32 pos, const int16_t room_num)
{
    return Room_PointInside(Room_Get(room_num), pos);
}

static void M_UpdateCameraRooms(void)
{
    Room_GetSector(g_Camera.pos.pos, &g_Camera.pos.room_num);
    Room_GetSector(g_Camera.target.pos, &g_Camera.target.room_num);
    if (!M_CameraInsideRoom(g_Camera.pos.pos, g_Camera.pos.room_num)) {
        const int16_t pos_room_num = Room_GetIndexFromPos(g_Camera.pos.pos);
        const int16_t tar_room_num = Room_GetIndexFromPos(g_Camera.target.pos);

        if (pos_room_num != NO_ROOM) {
            g_Camera.pos.room_num = pos_room_num;
            if (tar_room_num == NO_ROOM) {
                g_Camera.target.room_num = pos_room_num;
            }
        }
        if (tar_room_num != NO_ROOM) {
            g_Camera.target.room_num = tar_room_num;
            if (pos_room_num == NO_ROOM) {
                g_Camera.pos.room_num = tar_room_num;
            }
        }
    }

    Camera_EnsureEnvironment();
}

static bool M_HandleShiftInputs(void)
{
    bool result = false;

    const int32_t distance = M_GetShiftSpeed((WALL_L * 5.0) / LOGIC_FPS);
    if (g_Input.camera_left) {
        M_ShiftCamera(-distance, 0, 0);
        result = true;
    } else if (g_Input.camera_right) {
        M_ShiftCamera(distance, 0, 0);
        result = true;
    }

    if (g_Input.camera_forward) {
        M_ShiftCamera(0, 0, distance);
        result = true;
    } else if (g_Input.camera_back) {
        M_ShiftCamera(0, 0, -distance);
        result = true;
    }

    if (!g_Input.slow && g_Input.camera_up) {
        M_ShiftCamera(0, -distance, 0);
        result = true;
    } else if (!g_Input.slow && g_Input.camera_down) {
        M_ShiftCamera(0, distance, 0);
        result = true;
    }

    return result;
}

static bool M_HandleRotationInputs(void)
{
    bool result = false;

    if (g_Input.forward) {
        M_RotateCamera(0, -M_GetRotSpeed(), 0);
        result = true;
    } else if (g_Input.back) {
        M_RotateCamera(0, M_GetRotSpeed(), 0);
        result = true;
    }

    if (g_Input.left) {
        M_RotateCamera(-M_GetRotSpeed(), 0, 0);
        result = true;
    } else if (g_Input.right) {
        M_RotateCamera(M_GetRotSpeed(), 0, 0);
        result = true;
    }

    if (g_Input.slow && g_Input.camera_up) {
        M_RotateCamera(0, 0, -M_GetRotSpeed());
        result = true;
    } else if (g_Input.slow && g_Input.camera_down) {
        M_RotateCamera(0, 0, M_GetRotSpeed());
        result = true;
    }

    return result;
}

static bool M_HandleTargetRotationInputs(void)
{
    bool result = false;
    if (g_InputDB.roll) {
        M_RotateTarget(-DEG_90, 0, 0);
        result = true;
    }
    return result;
}

static bool M_HandleFOVInputs(void)
{
    if (!g_Input.draw) {
        return false;
    }

    if (g_Input.slow) {
        m_CurrentFOV--;
    } else {
        m_CurrentFOV++;
    }
    CLAMP(m_CurrentFOV, MIN_PHOTO_FOV, MAX_PHOTO_FOV);
    Viewport_AlterFOV(m_CurrentFOV * DEG_1, m_CurrentFOVMode);
    return true;
}

void Camera_PhotoMode_Enter(void)
{
    m_OriginalCamera = g_Camera;

    int16_t angles[2];
    Math_GetVectorAngles(
        g_Camera.target.x - g_Camera.pos.x, g_Camera.target.y - g_Camera.pos.y,
        g_Camera.target.z - g_Camera.pos.z, angles);
    g_Camera.target_angle = angles[0];
    g_Camera.target_elevation = angles[1];
    g_Camera.target_distance = CAMERA_DEFAULT_DISTANCE;
    g_Camera.target_square = SQUARE(g_Camera.target_distance);

    g_Camera.target.room_num = g_Camera.pos.room_num;
    Room_GetSector(g_Camera.target.pos, &g_Camera.target.room_num);

    m_StartingCamera = g_Camera;

    m_OriginalFOV = Viewport_GetEffectiveFOV();
    m_OriginalFOVMode = Viewport_GetFOVMode();
    m_CurrentFOV = m_OriginalFOV / DEG_1;
    m_CurrentFOVMode = m_OriginalFOVMode;
    g_Camera.type = CAM_PHOTO_MODE;
    const int32_t border = WALL_L * 5;
    m_WorldBounds = Room_GetWorldBounds();
    m_WorldBounds.min.x -= border;
    m_WorldBounds.min.y -= border;
    m_WorldBounds.min.z -= border;
    m_WorldBounds.max.x += border;
    m_WorldBounds.max.y += border;
    m_WorldBounds.max.z += border;
    M_UpdateCameraRooms();
}

void Camera_PhotoMode_Exit(void)
{
    Lara_Pose_Clear();
    Viewport_AlterFOV(m_OriginalFOV, m_OriginalFOVMode);
    M_ResetCamera(true);
}

void Camera_PhotoMode_Update(void)
{
    M_HandleFOVInputs();

    bool changed = false;

    if (g_InputDB.camera_reset) {
        M_ResetCamera(false);
        g_Camera.type = CAM_PHOTO_MODE;
        changed = true;
    }

    changed |= M_HandleShiftInputs();
    changed |= M_HandleRotationInputs();

    if (changed) {
        m_PhotoSpeed++;
        CLAMPG(m_PhotoSpeed, PHOTO_MAX_SPEED);
    } else {
        m_PhotoSpeed = 0;
    }

    changed |= M_HandleTargetRotationInputs();

    if (changed) {
        g_Camera.mic_pos = g_Camera.pos;
        M_ClampCameraPos();
        M_UpdateCameraRooms();
    }
}

void Camera_PhotoMode_UpdateFOV(void)
{
    M_HandleFOVInputs();
}

void Camera_PhotoMode_Pause(void)
{
    m_PreviousState.camera = g_Camera;
    m_PreviousState.is_chunky = Camera_IsChunky();
    m_PreviousState.fov = Viewport_GetSystemFOV();
    m_PreviousState.fov_mode = Viewport_GetFOVMode();
    g_Camera = m_OriginalCamera;
}

void Camera_PhotoMode_Resume(void)
{
    g_Camera = m_PreviousState.camera;
    Camera_SetChunky(m_PreviousState.is_chunky);
    Viewport_AlterFOV(m_PreviousState.fov, m_PreviousState.fov_mode);
}
