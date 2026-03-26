#include <trx/game/camera/cinematic.h>

#include <trx/game/camera.h>
#include <trx/game/game_buf.h>
#include <trx/game/lara.h>
#include <trx/game/rooms.h>
#include <trx/game/viewport.h>
#include <trx/version.h>

static CINE_FRAME *m_CineFrames = nullptr;
static CINE_DATA m_CineData = {};

static void M_UpdateCutscene(const XYZ_32 base_pos, const int16_t angle)
{
    const CINE_FRAME *const frame = Camera_GetCurrentCineFrame();
    const int32_t c = Math_Cos(angle);
    const int32_t s = Math_Sin(angle);

#define SHIFT(prop, axis1, op, axis2)                                          \
    (((frame->prop.shift.axis1 * c) op(frame->prop.shift.axis2 * s))           \
     >> W2V_SHIFT)

    const XYZ_32 camera_target = {
        .x = base_pos.x + SHIFT(target, x, +, z),
        .y = base_pos.y + frame->target.shift.y,
        .z = base_pos.z + SHIFT(target, z, -, x),
    };

    const XYZ_32 camera_pos = {
        .x = base_pos.x + SHIFT(camera, x, +, z),
        .y = base_pos.y + frame->camera.shift.y,
        .z = base_pos.z + SHIFT(camera, z, -, x),
    };

#undef SHIFT

    const int16_t room_num = Room_GetIndexFromPos(camera_pos);
    if (room_num != NO_ROOM) {
        g_Camera.pos.room_num = room_num;
    }

    g_Camera.target.pos = camera_target;
    g_Camera.pos.pos = camera_pos;
    g_Camera.roll = frame->roll;
    g_Camera.shift = 0;

    Viewport_AlterFOV(frame->fov, FOV_MODE_CUTSCENE);
}

void Camera_InitialiseCineFrames(const int32_t num_frames)
{
    m_CineData.frame_count = num_frames;
    m_CineData.frame_idx = 0;
    m_CineFrames = num_frames == 0
        ? nullptr
        : GameBuf_Alloc(num_frames * sizeof(CINE_FRAME), GBUF_CINEMATIC_FRAMES);
}

CINE_FRAME *Camera_GetCineFrame(const int32_t frame_idx)
{
    if (m_CineFrames == nullptr) {
        return nullptr;
    }
    return &m_CineFrames[frame_idx];
}

CINE_FRAME *Camera_GetCurrentCineFrame(void)
{
    return Camera_GetCineFrame(m_CineData.frame_idx);
}

CINE_DATA *Camera_GetCineData(void)
{
    return &m_CineData;
}

void Camera_InvokeCinematic(
    const ITEM *const item, const int32_t frame_idx, const int16_t extra_y_rot)
{
    g_Camera.type = CAM_CINEMATIC;
    m_CineData.frame_idx = frame_idx;
    m_CineData.position.pos = item->pos;
    m_CineData.position.rot = item->rot;
    m_CineData.position.rot.y += extra_y_rot;
}

void Camera_LoadCutsceneFrame(void)
{
    CINE_DATA *const cine_data = Camera_GetCineData();
    if (cine_data->frame_count == 0) {
        return;
    }

    cine_data->frame_idx++;
    if (cine_data->frame_idx >= cine_data->frame_count) {
        cine_data->frame_idx = cine_data->frame_count - 1;
    }

    M_UpdateCutscene(cine_data->position.pos, cine_data->position.rot.y);
    Camera_UpdateMicPosition();
}

void Camera_UpdateCutscene(void)
{
    const CINE_DATA *const cine_data = Camera_GetCineData();
    if (cine_data->frame_count == 0) {
        return;
    }

    const ITEM *const lara_item = Lara_GetItem();
    M_UpdateCutscene(lara_item->pos, g_Camera.target_angle);

    Camera_EnsureEnvironment();
}
