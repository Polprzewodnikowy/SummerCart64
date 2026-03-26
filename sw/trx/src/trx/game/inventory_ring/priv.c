#include <trx/game/inventory_ring/priv.h>

#include <trx/config.h>
#include <trx/core/math.h>
#include <trx/core/strings.h>
#include <trx/game/const.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/input.h>
#include <trx/game/inventory.h>
#include <trx/game/inventory_ring/vars.h>
#include <trx/game/matrix.h>
#include <trx/game/music.h>
#include <trx/game/objects/names.h>
#include <trx/game/output/state.h>
#include <trx/game/overlay.h>
#include <trx/game/sound.h>
#include <trx/game/ui.h>
#include <trx/version.h>

#include <stdio.h>
#include <string.h>

#define M_RING_SWITCH_FRAMES (96 / 2)
#define M_CAMERA_Y_OFFSET (-96)
#define M_MANUAL_ROT_RESET_RATE 0.15

typedef enum {
    // clang-format off
    PASS_MESH_SPINE    = 1 << 0,
    PASS_MESH_FRONT    = 1 << 1,
    PASS_MESH_IN_FRONT = 1 << 2,
    PASS_MESH_PAGE_2   = 1 << 3,
    PASS_MESH_BACK     = 1 << 4,
    PASS_MESH_IN_BACK  = 1 << 5,
    PASS_MESH_PAGE_1   = 1 << 6,
    PASS_MESH_COMMON   = PASS_MESH_SPINE | PASS_MESH_BACK | PASS_MESH_IN_BACK | PASS_MESH_FRONT,
    // clang-format on
} PASS_MESH;

static bool m_ShowExamine = false;
static bool m_ShowUseItemButton = false;
static void (*m_ButtonHintDrawFunc)(void *) = nullptr;
static void *m_ButtonHintUserData = nullptr;
static char *m_CountText = nullptr;
static size_t m_CountTextCap = 0;
static OBJECT_ID m_RequestedObjectID = NO_OBJECT;

static void M_DrawExamineHint(void *const user_data)
{
    UI_BeginStack(UI_STACK_HORIZONTAL);
    UI_ButtonLabel(INPUT_ROLE_LOOK, GS("general/actions/examine_item"));
    if (m_ShowUseItemButton) {
        UI_Spacer(60.0f, 0.0f);
        UI_ButtonLabel(INPUT_ROLE_ACTION, GS("general/actions/use_item"));
    }
    UI_EndStack();
}

static void M_AdjustRot(int16_t *const rot, const int16_t dest_rot)
{
    const int32_t delta = dest_rot - *rot;
    if (delta != 0) {
        if (delta > 0 && delta < DEG_180) {
            *rot += 1024;
        } else {
            *rot -= 1024;
        }
        *rot &= ~(1024 - 1);
    }
}

static XYZ_32 M_VectorViewFromWorld(const XYZ_32 v_world)
{
    return Matrix_MulVec32_M(&g_ViewMatrix, v_world);
}

static void M_HandleRequestedObject(INV_RING *const ring)
{
    if (m_RequestedObjectID == NO_OBJECT) {
        return;
    }

    for (int32_t i = 0; i < ring->number_of_objects; i++) {
        const OBJECT_ID object_id = ring->list[i]->object_id;
        if (object_id == m_RequestedObjectID
            && Inv_RequestItem(object_id) > 0) {
            ring->current_object = i;
            break;
        }
    }

    m_RequestedObjectID = NO_OBJECT;
}

static void M_MotionInit(INV_RING *const ring)
{
    INV_RING_MOTION *const motion = &ring->motion;
    motion->radius_target = 0;
    motion->radius_rate = 0;
    motion->camera_y_target = 0;
    motion->camera_y_rate = 0;
    motion->camera_pitch_target = 0;
    motion->camera_pitch_rate = 0;
    motion->rotate_target = 0;
    motion->rotate_rate = 0;
    motion->item_pt_x_rot_target = 0;
    motion->item_pt_x_rot_rate = 0;
    motion->item_x_rot_target = 0;
    motion->item_x_rot_rate = 0;
    motion->item_y_trans_target = 0;
    motion->item_y_trans_rate = 0;
    motion->item_z_trans_target = 0;
    motion->item_z_trans_rate = 0;

    motion->misc = 0;
}

static void M_MotionCameraPos(INV_RING *const ring, const int16_t target)
{
    INV_RING_MOTION *const motion = &ring->motion;
    motion->camera_y_target = target;
    motion->camera_y_rate = (target - ring->camera.pos.y) / ring->status_frames;
}

static void M_MotionCameraPitch(INV_RING *const ring, const int16_t target)
{
    INV_RING_MOTION *const motion = &ring->motion;
    motion->camera_pitch_target = target;
    motion->camera_pitch_rate = target / ring->status_frames;
    motion->misc = target;
}

static void M_MotionRotation(
    INV_RING *const ring, const int16_t rotation, const int16_t target)
{
    INV_RING_MOTION *const motion = &ring->motion;
    motion->rotate_target = target;
    motion->rotate_rate = rotation / ring->status_frames;
}

static void M_MotionRadius(INV_RING *const ring, const int16_t target)
{
    INV_RING_MOTION *const motion = &ring->motion;
    motion->radius_target = target;
    motion->radius_rate = (target - ring->radius) / ring->status_frames;
}

static void M_MotionItemSelect(
    INV_RING *const ring, const INVENTORY_ITEM *const inv_item)
{
    INV_RING_MOTION *const motion = &ring->motion;
    motion->item_pt_x_rot_target = inv_item->x_rot_pt_sel;
    motion->item_pt_x_rot_rate = inv_item->x_rot_pt_sel / ring->status_frames;
    motion->item_x_rot_target = inv_item->x_rot_sel;
    motion->item_x_rot_rate =
        (inv_item->x_rot_sel - inv_item->x_rot_nosel) / ring->status_frames;
    motion->item_y_trans_target = inv_item->y_trans_sel;
    motion->item_y_trans_rate = inv_item->y_trans_sel / ring->status_frames;
    motion->item_z_trans_target = inv_item->z_trans_sel;
    motion->item_z_trans_rate = inv_item->z_trans_sel / ring->status_frames;
}

static void M_MotionItemDeselect(
    INV_RING *const ring, const INVENTORY_ITEM *const inv_item)
{
    INV_RING_MOTION *const motion = &ring->motion;
    motion->item_pt_x_rot_target = 0;
    motion->item_pt_x_rot_rate =
        -(inv_item->x_rot_pt_sel / ring->status_frames);
    motion->item_x_rot_target = inv_item->x_rot_nosel;
    motion->item_x_rot_rate =
        (inv_item->x_rot_nosel - inv_item->x_rot_sel) / ring->status_frames;
    motion->item_y_trans_target = 0;
    motion->item_y_trans_rate = -(inv_item->y_trans_sel / ring->status_frames);
    motion->item_z_trans_target = 0;
    motion->item_z_trans_rate = -(inv_item->z_trans_sel / ring->status_frames);
}

void InvRing_AdjustMusicVolume(const INV_RING *const ring)
{
    if (ring->mode == INV_TITLE_MODE) {
        Music_SetVolume(g_Config.audio.music_volume);
        return;
    }
    const bool is_ambient =
        Music_GetCurrentPlayingTrack() == Music_GetCurrentLoopedTrack();
    const double base_volume = is_ambient ? g_Config.audio.ambient_volume
                                          : g_Config.audio.music_volume;
    const double multiplier = is_ambient
        ? g_Config.audio.inventory_ambient_volume
        : g_Config.audio.inventory_music_volume;
    Music_SetVolume(base_volume * multiplier);

    if (ring->mode != INV_GLOBE_SELECT_MODE) {
        Sound_ResetAmbient();
        Sound_UpdateEffects();
    }
}

void InvRing_SetRequestedObjectID(const OBJECT_ID obj_id)
{
    m_RequestedObjectID = obj_id;
}

void InvRing_InitRing(
    INV_RING *const ring, const RING_TYPE type, INVENTORY_ITEM **const list,
    const int16_t qty, const int16_t current)
{
    ring->type = type;
    ring->list = list;
    ring->radius = 0;
    ring->prev_radius = 0;
    ring->number_of_objects = qty;
    ring->current_object = current;
    ring->angle_adder = DEG_360 / qty;

    ring->is_pass_open = false;
    ring->is_demo_needed = false;
    ring->has_spun_out = false;

    M_HandleRequestedObject(ring);

    if (ring->mode == INV_TITLE_MODE) {
        ring->camera_pitch = 1024;
    } else {
        ring->camera_pitch = 0;
    }
    ring->prev_camera_pitch = ring->camera_pitch;

    ring->rotating = false;
    ring->rotate_from_object = 0;
    ring->rotate_to_object = 0;
    ring->rot_count = 0;
    ring->target_object = 0;
    ring->rot_adder = 0;
    ring->rot_adder_l = 0;
    ring->rot_adder_r = 0;

    ring->camera.pos.x = 0;
    ring->camera.pos.y = INV_RING_CAMERA_START_HEIGHT;
    ring->camera.pos.z = 896;
    ring->camera.rot.x = 0;
    ring->camera.rot.y = 0;
    ring->camera.rot.z = 0;

    ring->status = RNG_OPENING;
    ring->status_target = RNG_OPEN;
    ring->status_frames = INV_RING_OPEN_FRAMES;

    M_MotionRadius(ring, INV_RING_RADIUS);
    M_MotionCameraPos(ring, INV_RING_CAMERA_HEIGHT);
    M_MotionRotation(
        ring, INV_RING_OPEN_ROTATION,
        -DEG_90 - ring->current_object * ring->angle_adder);

    ring->ring_pos.pos.x = 0;
    ring->ring_pos.pos.y = 0;
    ring->ring_pos.pos.z = 0;
    ring->ring_pos.rot.x = 0;
    ring->ring_pos.rot.y = ring->motion.rotate_target - INV_RING_OPEN_ROTATION;
    ring->prev_ring_rot_y = ring->ring_pos.rot.y;
    ring->ring_pos.rot.z = 0;

    ring->light.x = -1536;
    ring->light.y = 256;
    ring->light.z = 1024;

    ring->prev_camera_y = ring->camera.pos.y;

    m_ShowExamine = false;
    m_ShowUseItemButton = false;
    m_ButtonHintDrawFunc = nullptr;
    m_ButtonHintUserData = nullptr;
}

void InvRing_InitInvItem(INVENTORY_ITEM *const inv_item)
{
    inv_item->meshes_drawn = inv_item->meshes_sel;
    inv_item->current_frame = 0;
    inv_item->goal_frame = 0;
    inv_item->manual_rot = g_IDMatrix;
    inv_item->x_rot_pt = 0;
    inv_item->prev_x_rot_pt = inv_item->x_rot_pt;
    inv_item->x_rot = inv_item->x_rot_nosel;
    inv_item->prev_x_rot = inv_item->x_rot;
    inv_item->y_rot = 0;
    inv_item->prev_y_rot = inv_item->y_rot;
    inv_item->y_trans = 0;
    inv_item->prev_y_trans = inv_item->y_trans;
    inv_item->z_trans = 0;
    inv_item->prev_z_trans = inv_item->z_trans;
    inv_item->action = ACTION_USE;
    inv_item->prev_manual_rot = inv_item->manual_rot;
    if (inv_item->object_id == O_PASSPORT_OPTION) {
        inv_item->object_id = O_PASSPORT_CLOSED;
    }
}

void InvRing_GetView(
    const INV_RING *const ring, XYZ_32 *const out_pos, XYZ_16 *const out_rot)
{
    int16_t angles[2];
    Math_GetVectorAngles(
        -ring->camera.pos.x, M_CAMERA_Y_OFFSET - ring->camera.pos.y,
        ring->radius - ring->camera.pos.z, angles);
    out_pos->x = ring->camera.pos.x;
    out_pos->y = ring->camera.pos.y;
    out_pos->z = ring->camera.pos.z;
    out_rot->x = angles[1] + ring->camera_pitch;
    out_rot->y = angles[0];
    out_rot->z = 0;
}

void InvRing_Light(const INV_RING *const ring)
{
    int16_t angles[2];
    Math_GetVectorAngles(ring->light.x, ring->light.y, ring->light.z, angles);
    Output_SetLightDivider(0x6000);
    Output_RotateLight(angles[1], angles[0]);

    if (g_TRVersion >= 3) {
        // OG Inv_RingLight() LightCol columns are (sun, spot, dynamic):
        // sun = (3312, 1664, 0);
        // spot = (3312, 3312, 3312);
        // dynamic = (0, 0, 3072) with an ambient of (32, 32, 32).
        const float ambient_u8 = 32.0f / 255.0f;
        const RGB_F ambient = { ambient_u8, ambient_u8, ambient_u8 };
        const RGB_F colors[3] = {
            {
                .r = 3312.0f / 4096.0f,
                .g = 1664.0f / 4096.0f,
                .b = 0.0f,
            },
            {
                .r = 3312.0f / 4096.0f,
                .g = 3312.0f / 4096.0f,
                .b = 3312.0f / 4096.0f,
            },
            {
                .r = 0.0f,
                .g = 0.0f,
                .b = 3072.0f / 4096.0f,
            },
        };
        const XYZ_32 dirs_view[3] = {
            M_VectorViewFromWorld((XYZ_32) {
                .x = 0x4000,
                .y = -0x4000,
                .z = 0x3000,
            }),
            M_VectorViewFromWorld((XYZ_32) {
                .x = -0x4000,
                .y = -0x4000,
                .z = 0x3000,
            }),
            M_VectorViewFromWorld(
                (XYZ_32) { .x = 0, .y = 0x2000, .z = 0x3000 }),
        };
        Output_SetTR3Light(ambient, colors, dirs_view);
    }
}

void InvRing_CalcAdders(INV_RING *const ring, const int16_t rotation_duration)
{
    ring->angle_adder = DEG_360 / ring->number_of_objects;
    ring->rot_adder_l = ring->angle_adder / rotation_duration;
    ring->rot_adder_r = -ring->angle_adder / rotation_duration;
}

void InvRing_DoMotions(INV_RING *const ring)
{
    INV_RING_MOTION *const motion = &ring->motion;

    if (ring->status_frames != 0) {
        ring->radius += motion->radius_rate;
        ring->camera.pos.y += motion->camera_y_rate;
        ring->ring_pos.rot.y += motion->rotate_rate;
        ring->camera_pitch += motion->camera_pitch_rate;

        INVENTORY_ITEM *const inv_item = ring->list[ring->current_object];
        inv_item->x_rot_pt += motion->item_pt_x_rot_rate;
        inv_item->x_rot += motion->item_x_rot_rate;
        inv_item->y_trans += motion->item_y_trans_rate;
        inv_item->z_trans += motion->item_z_trans_rate;

        ring->status_frames--;
        if (ring->status_frames == 0) {
            ring->status = ring->status_target;

            if (motion->radius_rate != 0) {
                motion->radius_rate = 0;
                ring->radius = motion->radius_target;
            }
            if (motion->camera_y_rate != 0) {
                motion->camera_y_rate = 0;
                ring->camera.pos.y = motion->camera_y_target;
            }
            if (motion->rotate_rate != 0) {
                motion->rotate_rate = 0;
                ring->ring_pos.rot.y = motion->rotate_target;
            }
            if (motion->item_pt_x_rot_rate != 0) {
                motion->item_pt_x_rot_rate = 0;
                inv_item->x_rot_pt = motion->item_pt_x_rot_target;
            }
            if (motion->item_x_rot_rate != 0) {
                motion->item_x_rot_rate = 0;
                inv_item->x_rot = motion->item_x_rot_target;
            }
            if (motion->item_y_trans_rate != 0) {
                motion->item_y_trans_rate = 0;
                inv_item->y_trans = motion->item_y_trans_target;
            }
            if (motion->item_z_trans_rate != 0) {
                motion->item_z_trans_rate = 0;
                inv_item->z_trans = motion->item_z_trans_target;
            }
            if (motion->camera_pitch_rate != 0) {
                motion->camera_pitch_rate = 0;
                ring->camera_pitch = motion->camera_pitch_target;
            }
        }
    }

    if (ring->rotating) {
        ring->ring_pos.rot.y += ring->rot_adder;
        ring->rot_count--;

        if (ring->rot_count == 0) {
            ring->current_object = ring->target_object;
            ring->ring_pos.rot.y =
                -DEG_90 - ring->target_object * ring->angle_adder;
            ring->rotating = false;
        }
    }
}

void InvRing_RotateLeft(INV_RING *const ring)
{
    ring->rotating = true;
    ring->rotate_from_object = ring->current_object;
    if (ring->current_object <= 0) {
        ring->target_object = ring->number_of_objects - 1;
    } else {
        ring->target_object = ring->current_object - 1;
    }
    ring->rotate_to_object = ring->target_object;
    ring->rot_count = INV_RING_ROTATE_DURATION;
    ring->rot_adder = ring->rot_adder_l;
}

void InvRing_RotateRight(INV_RING *const ring)
{
    ring->rotating = true;
    ring->rotate_from_object = ring->current_object;
    if (ring->current_object + 1 >= ring->number_of_objects) {
        ring->target_object = 0;
    } else {
        ring->target_object = ring->current_object + 1;
    }
    ring->rotate_to_object = ring->target_object;
    ring->rot_count = INV_RING_ROTATE_DURATION;
    ring->rot_adder = ring->rot_adder_r;
}

void InvRing_SetStatusTransition(
    INV_RING *const ring, const RING_STATUS status,
    const RING_STATUS status_target, const int16_t frames)
{
    INV_RING_MOTION *const motion = &ring->motion;
    ring->status_frames = frames;
    ring->status = status;
    ring->status_target = status_target;
    motion->radius_rate = 0;
    motion->camera_y_rate = 0;

    const INVENTORY_ITEM *const inv_item = ring->list[ring->current_object];

    switch (status) {
    case RNG_OPENING:
        M_MotionRadius(ring, INV_RING_RADIUS);
        ring->camera_pitch = -ring->motion.misc;
        ring->motion.camera_pitch_rate =
            ring->motion.misc / (M_RING_SWITCH_FRAMES / 2);
        ring->motion.camera_pitch_target = 0;
        InvRing_CalcAdders(ring, INV_RING_ROTATE_DURATION);
        M_MotionRotation(
            ring, INV_RING_OPEN_ROTATION,
            -DEG_90 - ring->angle_adder * ring->current_object);
        ring->ring_pos.rot.y =
            ring->motion.rotate_target + INV_RING_OPEN_ROTATION;
        break;

    case RNG_CLOSING:
        M_MotionRadius(ring, 0);

        switch (status_target) {
        case RNG_DONE:
        case RNG_FADING_OUT:
            M_MotionCameraPos(ring, INV_RING_CAMERA_START_HEIGHT);
            break;
        case RNG_MAIN2KEYS:
        case RNG_OPTION2MAIN:
            M_MotionCameraPitch(ring, DEG_45);
            break;
        case RNG_MAIN2OPTION:
        case RNG_KEYS2MAIN:
            M_MotionCameraPitch(ring, -DEG_45);
            break;
        default:
            break;
        }

        M_MotionRotation(
            ring, INV_RING_CLOSE_ROTATION,
            ring->ring_pos.rot.y - INV_RING_CLOSE_ROTATION);
        break;

    case RNG_SELECTING:
        M_MotionRotation(
            ring, 0, -DEG_90 - ring->angle_adder * ring->current_object);
        M_MotionItemSelect(ring, inv_item);
        break;

    case RNG_DESELECT:
    case RNG_EXITING_INVENTORY:
        M_MotionItemDeselect(ring, inv_item);
        break;

    case RNG_DESELECTING:
        M_MotionRotation(
            ring, 0, -DEG_90 - ring->angle_adder * ring->current_object);
        break;

    default:
        break;
    }
}

void InvRing_SelectMeshes(INVENTORY_ITEM *const inv_item)
{
    switch (inv_item->object_id) {
    case O_PASSPORT_OPTION: {
        struct {
            int32_t frame;
            uint32_t meshes;
        } frame_map[] = {
            { 14, PASS_MESH_IN_FRONT | PASS_MESH_PAGE_1 },
            { 18, PASS_MESH_IN_FRONT | PASS_MESH_PAGE_1 | PASS_MESH_PAGE_2 },
            { 19, PASS_MESH_PAGE_1 | PASS_MESH_PAGE_2 },
            { 23, PASS_MESH_PAGE_1 | PASS_MESH_PAGE_2 | PASS_MESH_IN_BACK },
            { 28, PASS_MESH_PAGE_2 | PASS_MESH_IN_BACK },
            { 29, 0 },
            { -1, -1 }, // sentinel
        };

        for (int32_t i = 0; frame_map[i].frame != -1; i++) {
            if (inv_item->current_frame <= frame_map[i].frame) {
                inv_item->meshes_drawn = PASS_MESH_COMMON | frame_map[i].meshes;
                break;
            }
        }
        break;
    }

    case O_COMPASS_OPTION:
    case O_STOPWATCH_OPTION:
        if (inv_item->current_frame == 0 || inv_item->current_frame >= 18) {
            inv_item->meshes_drawn = inv_item->meshes_sel;
        } else {
            inv_item->meshes_drawn = -1;
        }
        break;

    default:
        inv_item->meshes_drawn = -1;
        break;
    }
}

void InvRing_ShowItemName(const INVENTORY_ITEM *const inv_item)
{
    if (inv_item->object_id == O_PASSPORT_OPTION
        || inv_item->object_id == O_GLOBE_SELECT_OPTION) {
        return;
    }
    Overlay_SetBottomText((OVERLAY_TEXT) {
        .kind = UI_OVERLAY_TEXT_OBJECT_NAME,
        .object_id = inv_item->object_id,
        .fmt_gs_key = GS_ID("general/inventory_ring/object_name_fmt"),
    });
}

void InvRing_ShowItemQuantity(const char *const fmt, const int32_t qty)
{
    const char *const full_fmt =
        String_FormatStatic(GS("general/inventory_ring/item_count_fmt"), fmt);
    String_FormatInto(&m_CountText, &m_CountTextCap, full_fmt, qty);
}

void InvRing_SetButtonHintDrawer(void (*draw_func)(void *), void *user_data)
{
    m_ButtonHintDrawFunc = draw_func;
    m_ButtonHintUserData = user_data;
}

void InvRing_ClearButtonHint(void)
{
    InvRing_SetButtonHintDrawer(nullptr, nullptr);
}

void InvRing_ShowExamine(const OBJECT_ID object_id, const bool show)
{
    m_ShowExamine = show;
    m_ShowUseItemButton = show;
    if (show) {
        switch (object_id) {
        case O_QUEST_ITEM_1:
        case O_QUEST_ITEM_2:
        case O_QUEST_ITEM_3:
        case O_QUEST_ITEM_4:
        case O_QUEST_OPTION_1:
        case O_QUEST_OPTION_2:
        case O_QUEST_OPTION_3:
        case O_QUEST_OPTION_4:
        case O_PICKUP_OPTION_1:
        case O_PICKUP_OPTION_2:
            m_ShowUseItemButton = false;
            break;
        default:
            break;
        }
        InvRing_SetButtonHintDrawer(M_DrawExamineHint, nullptr);
    } else if (m_ButtonHintDrawFunc == M_DrawExamineHint) {
        InvRing_ClearButtonHint();
    }
}

void InvRing_DrawUI(INV_RING *const ring)
{
    UI_BeginModal(0.5f, 1.0f);
    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_VERTICAL,
        .align = { .h = UI_STACK_H_ALIGN_CENTER },
        .spacing = { .v = 20.0f },
    });

    if (m_ButtonHintDrawFunc != nullptr) {
        m_ButtonHintDrawFunc(m_ButtonHintUserData);
    }

    if (m_CountText != nullptr && m_CountText[0] != '\0') {
        UI_BeginOffset(64.0f, 0.0f);
        UI_Label(m_CountText);
        UI_EndOffset();
    }
    UI_Spacer(0.0f, 50.0f);
    UI_EndStack();
    UI_EndModal();
}

void InvRing_RemoveItemTexts(void)
{
    Overlay_SetBottomText((OVERLAY_TEXT) { 0 });
    if (m_CountText != nullptr) {
        strcpy(m_CountText, "");
    }
}

void InvRing_ShowHeader(INV_RING *const ring)
{
    if (ring->mode == INV_TITLE_MODE) {
        return;
    }

    switch (ring->type) {
    case RT_MAIN:
        Overlay_SetTopText((OVERLAY_TEXT) {
            .kind = UI_OVERLAY_TEXT_GS_KEY,
            .gs_key = GS_ID("general/inventory_ring/heading_inventory"),
            .fmt_gs_key = GS_ID("general/inventory_ring/heading_fmt"),
        });
        break;
    case RT_OPTION:
        if (ring->mode == INV_DEATH_MODE) {
            Overlay_SetTopText((OVERLAY_TEXT) {
                .kind = UI_OVERLAY_TEXT_GS_KEY,
                .gs_key = GS_ID("general/inventory_ring/heading_game_over"),
                .fmt_gs_key = GS_ID("general/inventory_ring/heading_fmt"),
            });
        } else {
            Overlay_SetTopText((OVERLAY_TEXT) {
                .kind = UI_OVERLAY_TEXT_GS_KEY,
                .gs_key = GS_ID("general/inventory_ring/heading_option"),
                .fmt_gs_key = GS_ID("general/inventory_ring/heading_fmt"),
            });
        }
        break;
    case RT_KEYS:
        Overlay_SetTopText((OVERLAY_TEXT) {
            .kind = UI_OVERLAY_TEXT_GS_KEY,
            .gs_key = GS_ID("general/inventory_ring/heading_items"),
            .fmt_gs_key = GS_ID("general/inventory_ring/heading_fmt"),
        });
        break;
    case RT_GLOBE_SELECT:
        break;
    case RT_NUMBER_OF:
        break;
    }

    if (ring->mode != INV_GAME_MODE) {
        return;
    }

    const bool show_up_arrow = ring->type == RT_OPTION
        || (ring->type == RT_MAIN && g_InvRing_Source[RT_KEYS].count > 0);
    const bool show_bottom_arrow = ring->type == RT_KEYS
        || (ring->type == RT_MAIN && !InvRing_IsOptionLockedOut());

    Overlay_ShowArrow(UI_OVERLAY_ARROW_TL, show_up_arrow);
    Overlay_ShowArrow(UI_OVERLAY_ARROW_TR, show_up_arrow);

    Overlay_ShowArrow(UI_OVERLAY_ARROW_BL, show_bottom_arrow);
    Overlay_ShowArrow(UI_OVERLAY_ARROW_BR, show_bottom_arrow);
}

void InvRing_RemoveHeader(void)
{
    Overlay_SetTopText((OVERLAY_TEXT) { 0 });
    Overlay_ShowArrow(UI_OVERLAY_ARROW_TL, false);
    Overlay_ShowArrow(UI_OVERLAY_ARROW_TR, false);
    Overlay_ShowArrow(UI_OVERLAY_ARROW_BL, false);
    Overlay_ShowArrow(UI_OVERLAY_ARROW_BR, false);
}

bool InvRing_CanExamine(void)
{
    return g_Config.gameplay.enable_item_examining && m_ShowExamine;
}

void InvRing_ShowVersionText(void)
{
    Overlay_ShowVersion(true);
}

void InvRing_RemoveVersionText(void)
{
    Overlay_ShowVersion(false);
}

void InvRing_UpdateInventoryItem(
    const INV_RING *const ring, INVENTORY_ITEM *const inv_item)
{

    if (inv_item != ring->list[ring->current_object]) {
        if (inv_item->y_rot < 0) {
            inv_item->y_rot += 256;
        } else if (inv_item->y_rot > 0) {
            inv_item->y_rot -= 256;
        }
    } else if (ring->rotating) {
        if (inv_item->y_rot > 0) {
            inv_item->y_rot -= 512;
        } else if (inv_item->y_rot < 0) {
            inv_item->y_rot += 512;
        }
    } else if (
        ring->status == RNG_SELECTED || ring->status == RNG_DESELECTING
        || ring->status == RNG_SELECTING || ring->status == RNG_DESELECT
        || ring->status == RNG_CLOSING_ITEM) {

        if (inv_item->has_manual_rot) {
            return;
        }

        M_AdjustRot(&inv_item->y_rot, inv_item->y_rot_sel);
        Matrix_Slerp3x3_M(
            &inv_item->manual_rot, &g_IDMatrix, M_MANUAL_ROT_RESET_RATE);
    } else if (
        ring->number_of_objects == 1
        || (!g_Input.menu_right && !g_Input.menu_left)) {
        inv_item->y_rot += 256;
    }
}
