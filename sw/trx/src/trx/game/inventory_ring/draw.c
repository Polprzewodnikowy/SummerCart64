#include <trx/game/inventory_ring/draw.h>

#include <trx/config.h>
#include <trx/core/math.h>
#include <trx/core/utils.h>
#include <trx/game/game.h>
#include <trx/game/input.h>
#include <trx/game/interpolation.h>
#include <trx/game/inventory_ring.h>
#include <trx/game/inventory_ring/priv.h>
#include <trx/game/inventory_ring/vars.h>
#include <trx/game/matrix.h>
#include <trx/game/objects.h>
#include <trx/game/option.h>
#include <trx/game/option/globe_select.h>
#include <trx/game/option/stats.h>
#include <trx/game/output.h>
#include <trx/game/savegame.h>
#include <trx/game/shell.h>
#include <trx/game/viewport.h>
#include <trx/version.h>

#include <math.h>

#define M_CAMERA_2_RING 598
#define M_SHADE_NORMAL SHADE_LOW
#define M_SHADE_SELECTED SHADE_NEUTRAL

static XYZ_32 M_VectorViewFromWorld(const XYZ_32 v_world)
{
    return Matrix_MulVec32_M(&g_ViewMatrix, v_world);
}

static int16_t M_LerpI16(
    const int16_t prev_value, const int16_t cur_value, const double rate)
{
    return (int16_t)round(LERP(prev_value, cur_value, rate));
}

static int32_t M_LerpI32(
    const int32_t prev_value, const int32_t cur_value, const double rate)
{
    return (int32_t)round(LERP(prev_value, cur_value, rate));
}

static int16_t M_LerpAngleI16(
    const int16_t prev_value, const int16_t cur_value, const double rate)
{
    const int32_t prev_u16 = (uint16_t)prev_value;
    const int32_t cur_u16 = (uint16_t)cur_value;
    const int32_t interp = Math_AngleMean(prev_u16, cur_u16, rate);
    return (int16_t)(uint16_t)interp;
}

static float M_GlobeSelectPulse01(const float time)
{
    const int16_t angle = (((uint64_t)time) % 16ULL) * DEG_360 / 16;
    const float s = (float)Math_Sin(angle);
    return (s + 16384.0f) / (16384.0f * 2.0f);
}

static void M_GlobeSelectApplyLight(
    const INV_RING *const ring, const uint32_t bit, const int32_t mesh_idx)
{
    const float ambient_u8 = 32.0f / 255.0f;
    const RGB_F ambient = { ambient_u8, ambient_u8, ambient_u8 };

    RGB_F colors[3] = {};

    if (bit == 1u) {
        colors[0] = (RGB_F) { 0, 256.0f / 4096.0f, 3840.0f / 4096.0f };
        colors[1] = (RGB_F) { 0, 256.0f / 4096.0f, 3840.0f / 4096.0f };
        colors[2] = (RGB_F) { 0, 256.0f / 4096.0f, 3840.0f / 4096.0f };
    } else if ((bit & 0x7Eu) != 0u) {
        const float pulse = M_GlobeSelectPulse01(Output_GetTime());
        const int32_t area_idx = Option_GlobeSelect_AreaFromMeshIdx(mesh_idx);
        const bool completed = area_idx >= 0 && area_idx < MAX_GLOBE_ZONES
            && !ring->globe_select.selectable[area_idx];

        const RGB_F marker = completed ? (RGB_F) { pulse, 0.0f, 0.0f }
                                       : (RGB_F) { 0.0f, pulse, 0.0f };
        colors[0] = marker;
        colors[1] = marker;
        colors[2] = marker;
    } else {
        colors[0] =
            (RGB_F) { 256.0f / 4096.0f, 1024.0f / 4096.0f, 256.0f / 4096.0f };
        colors[1] =
            (RGB_F) { 256.0f / 4096.0f, 1024.0f / 4096.0f, 256.0f / 4096.0f };
        colors[2] =
            (RGB_F) { 256.0f / 4096.0f, 1024.0f / 4096.0f, 256.0f / 4096.0f };
    }

    const XYZ_32 dirs_offsets[3] = {
        { .x = 0x1000, .y = -0x1000, .z = 0xC00 },
        { .x = -0x1000, .y = -0x1000, .z = 0xC00 },
        { .x = 0, .y = 0x800, .z = 0xC00 },
    };
    const XYZ_32 dirs_view[3] = {
        M_VectorViewFromWorld(dirs_offsets[0]),
        M_VectorViewFromWorld(dirs_offsets[1]),
        M_VectorViewFromWorld(dirs_offsets[2]),
    };

    Output_SetTR3Light(ambient, colors, dirs_view);
}

static int32_t M_GetFrames(
    const INV_RING *const ring, const INVENTORY_ITEM *const inv_item,
    ANIM_FRAME **const out_frame1, ANIM_FRAME **const out_frame2,
    int32_t *const out_rate)
{
    const OBJECT *const obj = Object_Get(inv_item->object_id);
    const INVENTORY_ITEM *const cur_inv_item = ring->list[ring->current_object];

    if (inv_item != cur_inv_item || inv_item->current_frame == 0
        || (ring->status != RNG_SELECTED && ring->status != RNG_CLOSING_ITEM)) {
        // only apply to animations, eg. the states where Inv_AnimateItem is
        // being actively called
        goto fallback;
    }

    if (inv_item->current_frame == inv_item->goal_frame
        || inv_item->frames_total == 1 || g_Config.rendering.fps == 30) {
        goto fallback;
    }

    const int32_t cur_frame_num = inv_item->current_frame;
    int32_t next_frame_num = inv_item->current_frame + inv_item->anim_direction;
    if (next_frame_num < 0) {
        next_frame_num = 0;
    }
    if (next_frame_num >= inv_item->frames_total) {
        next_frame_num = 0;
    }

    *out_frame1 = &obj->frame_base[cur_frame_num];
    *out_frame2 = &obj->frame_base[next_frame_num];
    *out_rate = 10;
    return (Interpolation_GetRate() - 0.5) * 10.0;

    // OG
fallback:
    *out_frame1 = &obj->frame_base[inv_item->current_frame];
    *out_frame2 = *out_frame1;
    *out_rate = 1;
    return 0;
}

static void M_DrawItem(
    const INV_RING *const ring, const INVENTORY_ITEM *const inv_item,
    const int16_t view_rot_y)
{
    const double interp_rate = Interpolation_GetRate();
    const int16_t draw_x_rot_pt = M_LerpAngleI16(
        inv_item->prev_x_rot_pt, inv_item->x_rot_pt, interp_rate);
    const int16_t draw_x_rot =
        M_LerpAngleI16(inv_item->prev_x_rot, inv_item->x_rot, interp_rate);
    const int16_t draw_y_rot =
        M_LerpAngleI16(inv_item->prev_y_rot, inv_item->y_rot, interp_rate);
    const int32_t draw_y_trans =
        M_LerpI32(inv_item->prev_y_trans, inv_item->y_trans, interp_rate);
    const int32_t draw_z_trans =
        M_LerpI32(inv_item->prev_z_trans, inv_item->z_trans, interp_rate);
    MATRIX draw_manual_rot = inv_item->prev_manual_rot;
    Matrix_Slerp3x3_M(&draw_manual_rot, &inv_item->manual_rot, interp_rate);

    int32_t shade = M_SHADE_NORMAL;
    if (ring->status != RNG_FADING_OUT && ring->status != RNG_DONE) {
        if (ring->rotating) {
            float t = (ring->rot_count / (float)INV_RING_ROTATE_DURATION);
            CLAMP(t, 0.0f, 1.0f);
            if (inv_item == ring->list[ring->rotate_from_object]) {
                t = 1.0f - t;
            } else if (inv_item != ring->list[ring->rotate_to_object]) {
                t = 1.0f;
            }
            shade = LERP((float)M_SHADE_SELECTED, (float)M_SHADE_NORMAL, t);
        } else if (inv_item == ring->list[ring->current_object]) {
            shade = M_SHADE_SELECTED;
        }
    }
    Output_SetLightAdder(shade);

    Matrix_TranslateRel(0, draw_y_trans, draw_z_trans);

    Matrix_RotX(-draw_x_rot_pt);
    Matrix_RotY(-view_rot_y);
    Matrix_Mul3x3(&draw_manual_rot);
    Matrix_RotY(view_rot_y);
    Matrix_RotX(draw_x_rot_pt);

    Matrix_RotY(draw_y_rot);
    Matrix_RotX(draw_x_rot);

    const OBJECT *const obj = Object_Get(inv_item->object_id);
    if (!obj->loaded || obj->mesh_count < 0) {
        return;
    }

    if (inv_item->object_id == O_GLOBE_SELECT_OPTION) {
        Matrix_Rot16(ring->globe_select.rot);

        InvRing_Light(ring);
        ANIM_FRAME *const frame = &obj->frame_base[0];
        const uint32_t mesh_bits = ring->globe_select.meshes_drawn;
        for (int32_t mesh_idx = 0; mesh_idx < obj->mesh_count; mesh_idx++) {
            if (mesh_idx == 0) {
                Matrix_TranslateRel16(frame->offset);
                Matrix_Rot16(frame->mesh_rots[mesh_idx]);
            } else {
                const ANIM_BONE *const bone = Object_GetBone(obj, mesh_idx - 1);
                if (bone->matrix_pop) {
                    Matrix_Pop();
                }
                if (bone->matrix_push) {
                    Matrix_Push();
                }

                Matrix_TranslateRel32(bone->pos);
                Matrix_Rot16(frame->mesh_rots[mesh_idx]);
            }

            const uint32_t bit = 1u << mesh_idx;
            if ((mesh_bits & bit) == 0u) {
                continue;
            }

            M_GlobeSelectApplyLight(ring, bit, mesh_idx);
            Object_DrawMesh(obj->mesh_idx + mesh_idx, 0, false);
        }
        return;
    }

    int32_t rate;
    ANIM_FRAME *frame1;
    ANIM_FRAME *frame2;
    const int32_t frac = M_GetFrames(ring, inv_item, &frame1, &frame2, &rate);
    if (inv_item->object_id == O_COMPASS_OPTION) {
        const int16_t extra_rotation[1] = {
            Option_Stats_GetCompassNeedleAngle()
        };
        Object_GetBone(obj, 0)->rot.y = true;
        Object_DrawInterpolatedObject(
            obj, inv_item->meshes_drawn, extra_rotation, frame1, frame2, frac,
            rate);
    } else if (inv_item->object_id == O_STOPWATCH_OPTION) {
        const RESUME_INFO *const current_info =
            Savegame_GetCurrentInfo(Game_GetCurrentLevel());
        const int32_t total_seconds = current_info->stats.timer / LOGIC_FPS;
        const int32_t hours = (total_seconds % 43200) * DEG_1 * -360 / 43200;
        const int32_t minutes = (total_seconds % 3600) * DEG_1 * -360 / 3600;
        const int32_t seconds = (total_seconds % 60) * DEG_1 * -360 / 60;

        const int16_t extra_rotation[3] = { hours, minutes, seconds };
        Object_GetBone(obj, 3)->rot.z = true;
        Object_GetBone(obj, 4)->rot.z = true;
        Object_GetBone(obj, 5)->rot.z = true;
        Object_DrawInterpolatedObject(
            obj, inv_item->meshes_drawn, extra_rotation, frame1, frame2, frac,
            rate);
    } else {
        Object_DrawInterpolatedObject(
            obj, inv_item->meshes_drawn, nullptr, frame1, frame2, frac, rate);
    }
}

const INVENTORY_ITEM *InvRing_GetInvItem(const OBJECT_ID obj_id)
{
    for (int32_t i = 0; i < g_InvRing_Items->count; i++) {
        INVENTORY_ITEM *const item =
            *(INVENTORY_ITEM **)Vector_Get(g_InvRing_Items, i);
        if (item->object_id == obj_id) {
            return item;
        }
    }
    return nullptr;
}

void InvRing_Draw(INV_RING *const ring)
{
    InvRing_DrawUI(ring);
    const double interp_rate = Interpolation_GetRate();
    const int16_t draw_radius =
        M_LerpI16(ring->prev_radius, ring->radius, interp_rate);
    const int16_t draw_camera_y =
        M_LerpI16(ring->prev_camera_y, ring->camera.pos.y, interp_rate);
    const int16_t draw_ring_rot_y = M_LerpAngleI16(
        ring->prev_ring_rot_y, ring->ring_pos.rot.y, interp_rate);
    const int16_t draw_camera_pitch = M_LerpAngleI16(
        ring->prev_camera_pitch, ring->camera_pitch, interp_rate);

    INV_RING draw_ring = *ring;
    draw_ring.radius = draw_radius;
    draw_ring.camera.pos.y = draw_camera_y;
    draw_ring.camera_pitch = draw_camera_pitch;
    draw_ring.ring_pos.rot.y = draw_ring_rot_y;
    draw_ring.camera.pos.z = draw_radius + M_CAMERA_2_RING;

    if (ring->mode == INV_TITLE_MODE) {
        if (ring->background_path != nullptr) {
            Output_Overlay_DrawImage(ring->background_path);
        }
        Interpolation_Interpolate();
    } else {
        const float opacity = g_Config.ui.inventory_fade_effects
            ? Fader_GetCurrentValue(&ring->back_fader)
            : ring->back_fader.args.target;

        switch (ring->background_style) {
        case BK_NONE:
            if (ring->mode != INV_GLOBE_SELECT_MODE) {
                Output_Overlay_DrawGame();
            }
            break;

        case BK_TRANSPARENT_MEDIUM:
            Output_Overlay_DrawGame();
            Output_Overlay_DrawBlackRectangle(opacity * 0.5f, false);
            break;

        case BK_TRANSPARENT_DARK:
            Output_Overlay_DrawGame();
            Output_Overlay_DrawBlackRectangle(opacity * 0.8f, false);
            break;

        case BK_BLACK:
            Output_Overlay_DrawGame();
            Output_Overlay_DrawBlackRectangle(opacity * 1.0f, false);
            break;

        case BK_MONOCHROME:
            Output_Overlay_DrawGameMono(opacity);
            break;

        case BK_MONOCHROME_COOL:
            Output_Overlay_DrawGameMonoCool(opacity);
            break;

        case BK_MONOCHROME_WARM:
            Output_Overlay_DrawGameMonoWarm(opacity);
            break;

        case BK_PATTERN_STATIC:
        case BK_PATTERN_WAVE:
            if (opacity < 1.0f) {
                Output_Overlay_DrawGame();
            }
            Output_Overlay_DrawPatternOpacity(
                ring->background_style == BK_PATTERN_WAVE, opacity);
            break;

        case BK_IMAGE:
            if (ring->background_path != nullptr
                && Output_Overlay_LoadImage(ring->background_path)) {
                Output_Overlay_DrawImage(ring->background_path);
                Output_Overlay_DrawBlackRectangle(1.0f - opacity, false);
            } else {
                Output_Overlay_DrawBlackRectangle(1.0f, false);
            }
            break;

        default:
            Output_Overlay_DrawGame();
            break;
        }
    }
    Output_Flush();

    const int16_t old_fov = Viewport_GetSystemFOV();
    const FOV_MODE old_fov_mode = Viewport_GetFOVMode();
    Viewport_AlterFOV(FOV_VALUE_PASSPORT * DEG_1, FOV_MODE_PASSPORT);
    Output_ApplyFOV();

    XYZ_32 view_pos;
    XYZ_16 view_rot;
    InvRing_GetView(&draw_ring, &view_pos, &view_rot);
    Matrix_GenerateW2V(&view_pos, &view_rot);
    const int32_t old_fog_start = Output_GetFogStart();
    const int32_t old_fog_end = Output_GetFogEnd();
    Output_SetFogStart(20 * WALL_L);
    Output_SetFogEnd(100 * WALL_L);

    InvRing_Light(&draw_ring);

    Matrix_Push();
    Matrix_TranslateAbs32(draw_ring.ring_pos.pos);
    Matrix_Rot16(draw_ring.ring_pos.rot);

    if (!(ring->mode == INV_TITLE_MODE
          && (Fader_IsActive(&ring->top_fader)
              || Fader_IsActive(&ring->back_fader))
          && ring->status == RNG_OPENING)) {
        int16_t angle = 0;
        for (int32_t i = 0; i < draw_ring.number_of_objects; i++) {
            INVENTORY_ITEM *const inv_item = draw_ring.list[i];
            Matrix_Push();
            Matrix_RotY(angle);
            Matrix_TranslateRel(draw_ring.radius, 0, 0);
            Matrix_RotY(DEG_90);
            const int16_t draw_x_rot_pt = M_LerpAngleI16(
                inv_item->prev_x_rot_pt, inv_item->x_rot_pt, interp_rate);
            Matrix_RotX(draw_x_rot_pt);
            M_DrawItem(&draw_ring, inv_item, view_rot.y);
            angle += draw_ring.angle_adder;
            Matrix_Pop();
        }
    }

    Matrix_Pop();
    SceneCompositor_Flush();
    Output_SetFogStart(old_fog_start);
    Output_SetFogEnd(old_fog_end);
    Viewport_AlterFOV(old_fov, old_fov_mode);

    if (ring->status == RNG_SELECTED) {
        INVENTORY_ITEM *const inv_item = ring->list[ring->current_object];
        if (inv_item->object_id == O_PASSPORT_CLOSED) {
            inv_item->object_id = O_PASSPORT_OPTION;
        }
        Option_Draw(inv_item);
    }

    float top_opacity = Fader_GetCurrentValue(&ring->top_fader);
    if (ring->mode == INV_TITLE_MODE && ring->status != RNG_OPENING) {
        top_opacity = 0.0f;
    }
    Output_Overlay_DrawBlackRectangle(top_opacity, true);
}
