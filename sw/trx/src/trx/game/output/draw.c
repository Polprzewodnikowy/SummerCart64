#include <trx/game/output/draw.h>

#include <trx/config.h>
#include <trx/core/utils.h>
#include <trx/game/creature/const.h>
#include <trx/game/lara/common.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/output/bind.h>
#include <trx/game/output/sources/lightnings.h>
#include <trx/game/output/sources/misc.h>
#include <trx/game/output/sources/objects.h>
#include <trx/game/output/sources/poly_fx.h>
#include <trx/game/output/sources/rooms.h>
#include <trx/game/output/sources/rooms_debug.h>
#include <trx/game/output/sources/shadows.h>
#include <trx/game/output/sources/sprites.h>
#include <trx/game/output/sources/ui.h>
#include <trx/game/output/state.h>
#include <trx/game/rooms.h>
#include <trx/game/shell.h>
#include <trx/version.h>

#define M_SHADOW_LINE_POINTS 4
#define M_SHADOW_GRID_POINTS (M_SHADOW_LINE_POINTS * M_SHADOW_LINE_POINTS)

static bool M_DrawShadow_Sprite(
    const int32_t size, const BOUNDS_16 *const bounds, const ITEM *const item)
{
    const ITEM *const lara_item = Lara_GetItem();
    if (lara_item == nullptr) {
        return false;
    }

    const OBJECT *const shadow_obj = Object_Get(O_SHADOW);
    if (!shadow_obj->loaded) {
        return false;
    }

    // OG: shadow intensity is based on Lara's height above the floor, even for
    // non-Lara items.
    int32_t c = ((4096 - ABS(item->floor - item->pos.y)) >> 4) - 1;
    CLAMP(c, 32, 255);

    const RGBA_8888 shadow_color = { c, c, c, 255 };
    const RGBA_8888 quad_color[4] = {
        shadow_color,
        shadow_color,
        shadow_color,
        shadow_color,
    };

    const int32_t x_size = size * (bounds->max.x - bounds->min.x) / 128;
    const int32_t z_size = size * (bounds->max.z - bounds->min.z) / 128;
    const int32_t x_dist = x_size / M_SHADOW_LINE_POINTS;
    const int32_t z_dist = z_size / M_SHADOW_LINE_POINTS;

    int32_t grid_local_x[M_SHADOW_GRID_POINTS];
    int32_t grid_local_z[M_SHADOW_GRID_POINTS];
    int32_t x = -x_dist - (x_dist >> 1);
    int32_t z = z_dist + (z_dist >> 1);
    int32_t grid_idx = 0;
    for (int32_t row = 0; row < M_SHADOW_LINE_POINTS; row++) {
        for (int32_t col = 0; col < M_SHADOW_LINE_POINTS; col++) {
            grid_local_x[grid_idx] = x;
            grid_local_z[grid_idx] = z;
            grid_idx++;
            x += x_dist;
        }
        x = -x_dist - (x_dist >> 1);
        z -= z_dist;
    }

    // Determine the shadow anchor position.
    XYZ_32 anchor_pos = item->interp.result.pos;
    int32_t anchor_floor = item->interp.result.floor;
    const int16_t anim_state = item->current_anim_state;
    if (item == lara_item && anim_state != LS(LS_CRAWL_IDLE)
        && anim_state != LS(LS_CRAWL_FORWARD) && anim_state != LS(LS_CRAWL_BACK)
        && anim_state != LS(LS_CRAWL_TURN_LEFT)
        && anim_state != LS(LS_CRAWL_TURN_RIGHT)) {
        ANIM_FRAME *frames[2] = { nullptr, nullptr };
        int32_t rate = 0;
        const int32_t frac = Item_GetFrames(item, frames, &rate);
        if (frames[0] != nullptr) {
            XYZ_32 offset_a = XYZ_32_From16(frames[0]->offset);
            XYZ_32 offset = offset_a;
            if (frames[1] != nullptr && rate != 0 && frac != 0) {
                const XYZ_32 offset_b = XYZ_32_From16(frames[1]->offset);
                offset.x += ((offset_b.x - offset_a.x) * frac) / rate;
                offset.y += ((offset_b.y - offset_a.y) * frac) / rate;
                offset.z += ((offset_b.z - offset_a.z) * frac) / rate;
            }

            const int32_t sy = Math_Sin(item->interp.result.rot.y);
            const int32_t cy = Math_Cos(item->interp.result.rot.y);
            anchor_pos.x += (offset.x * cy + offset.z * sy) >> W2V_SHIFT;
            anchor_pos.y += offset.y;
            anchor_pos.z += (offset.z * cy - offset.x * sy) >> W2V_SHIFT;

            int16_t room_num = item->room_num;
            const SECTOR *const sector = Room_GetSector(anchor_pos, &room_num);
            const int32_t height = Room_GetHeight(sector, anchor_pos);
            if (height != NO_HEIGHT) {
                anchor_floor = height;
            }
        }
    } else {
        // TR3 cutscene actors are driven by cutscene data, so their item origin
        // can diverge from the visual mesh center.
        const int32_t x_mid = (bounds->min.x + bounds->max.x) / 2;
        const int32_t z_mid = (bounds->min.z + bounds->max.z) / 2;
        Matrix_Push();
        *g_MatrixPtr = g_ViewMatrix;
        *g_WMatrixPtr = g_IDMatrix;
        Matrix_TranslateAbs(
            item->interp.result.pos.x, item->interp.result.floor,
            item->interp.result.pos.z);
        Matrix_RotY(item->interp.result.rot.y);
        Matrix_TranslateRel(x_mid, 0, z_mid);
        anchor_pos = Matrix_GetOffset_M(g_WMatrixPtr);
        Matrix_Pop();
    }

    const int32_t base_y = anchor_floor - 16;
    const int32_t sy = Math_Sin(item->interp.result.rot.y);
    const int32_t cy = Math_Cos(item->interp.result.rot.y);

    // Compute the world-space grid points with floor-conforming Y offsets.
    XYZ_32 grid_world[M_SHADOW_GRID_POINTS];
    for (int32_t i = 0; i < M_SHADOW_GRID_POINTS; i++) {
        const int32_t lx = grid_local_x[i];
        const int32_t lz = grid_local_z[i];
        const int32_t rx = (lx * cy + lz * sy) >> W2V_SHIFT;
        const int32_t rz = (lz * cy - lx * sy) >> W2V_SHIFT;

        const int32_t wx = anchor_pos.x + rx;
        const int32_t wz = anchor_pos.z + rz;

        int16_t room_num = item->room_num;
        XYZ_32 test_pos = { wx, anchor_floor, wz };
        const SECTOR *const sector = Room_GetSector(test_pos, &room_num);
        int32_t height = Room_GetHeight(sector, test_pos);
        if (height == NO_HEIGHT) {
            height = anchor_floor;
        }
        if (ABS(height - anchor_floor) > 196) {
            height = anchor_floor;
        }

        grid_world[i] = (XYZ_32) {
            .x = wx,
            .y = base_y + (height - anchor_floor),
            .z = wz,
        };
    }

    const int32_t sprite_idx = shadow_obj->mesh_idx;
    const int32_t uvw_idx = Output_Textures_GetSpriteUVWIndex(sprite_idx, 0);
    const OUTPUT_TEXTURE_SIZE atlas_size =
        Output_Textures_GetAtlasSize(uvw_idx / 4);
    const OUTPUT_TEXTURE_SIZE quad_atlas_size[4] = {
        atlas_size,
        atlas_size,
        atlas_size,
        atlas_size,
    };

    OUTPUT_UVW sprite_uvw[4];
    for (int32_t i = 0; i < 4; i++) {
        const int32_t corner_uvw_idx =
            Output_Textures_GetSpriteUVWIndex(sprite_idx, i);
        sprite_uvw[i] = Output_Textures_GetUVW(corner_uvw_idx);
    }

    const float u_min =
        MIN(MIN(sprite_uvw[0].u, sprite_uvw[1].u),
            MIN(sprite_uvw[2].u, sprite_uvw[3].u));
    const float u_max =
        MAX(MAX(sprite_uvw[0].u, sprite_uvw[1].u),
            MAX(sprite_uvw[2].u, sprite_uvw[3].u));
    const float v_min =
        MIN(MIN(sprite_uvw[0].v, sprite_uvw[1].v),
            MIN(sprite_uvw[2].v, sprite_uvw[3].v));
    const float v_max =
        MAX(MAX(sprite_uvw[0].v, sprite_uvw[1].v),
            MAX(sprite_uvw[2].v, sprite_uvw[3].v));
    const float w = sprite_uvw[0].w;

    const float u_span = u_max - u_min;
    const float v_span = v_max - v_min;
    const float denom = (float)(M_SHADOW_LINE_POINTS - 1);

    for (int32_t row = 0; row < M_SHADOW_LINE_POINTS - 1; row++) {
        const float v0 = v_min + v_span * ((float)row / denom);
        const float v1 = v_min + v_span * ((float)(row + 1) / denom);
        for (int32_t col = 0; col < M_SHADOW_LINE_POINTS - 1; col++) {
            const float u0 = u_min + u_span * ((float)col / denom);
            const float u1 = u_min + u_span * ((float)(col + 1) / denom);

            const int32_t i0 = (row * M_SHADOW_LINE_POINTS) + col;
            const int32_t i1 = i0 + 1;
            const int32_t i2 = i0 + (M_SHADOW_LINE_POINTS + 1);
            const int32_t i3 = i0 + M_SHADOW_LINE_POINTS;

            const XYZ_32 quad_pos[4] = {
                grid_world[i0],
                grid_world[i1],
                grid_world[i2],
                grid_world[i3],
            };
            const OUTPUT_UVW quad_uvw[4] = {
                { u0, v0, w },
                { u1, v0, w },
                { u1, v1, w },
                { u0, v1, w },
            };

            OutputSource_PolyFX_StageQuadExtUV(
                quad_pos, quad_uvw, quad_atlas_size, nullptr, quad_color,
                VERT_NO_LIGHTING | VERT_NO_WIBBLE, DRAW_BLEND_SUB);
        }
    }
    return true;
}

static void M_DrawScreenQuad(
    const float x0, const float y0, const float x1, const float y1,
    const float z, const RGBA_8888 tl, const RGBA_8888 tr, const RGBA_8888 bl,
    const RGBA_8888 br)
{
    OutputSource_UI_StageQuad((OUTPUT_UI_QUAD) {
        .x0 = x0,
        .y0 = y0,
        .x1 = x1,
        .y1 = y1,
        .tl = tl,
        .tr = tr,
        .bl = bl,
        .br = br,
        .z = Output_GetNearZ_UI() + z,
    });
}

void Output_DrawBlackRectangle(const int32_t opacity)
{
    const int32_t sx = 0;
    const int32_t sy = 0;
    const int32_t sw = Viewport_GetWidth(VIEWPORT_UI);
    const int32_t sh = Viewport_GetHeight(VIEWPORT_UI);
    const RGBA_8888 background = { 0, 0, 0, opacity };
    Output_DrawScreenFlatQuad(sx, sy, 0, sw, sh, background);
}

void Output_DrawRoom(const ROOM *const room, const bool is_outside)
{
    OutputSource_Rooms_StageRoom(room);
    OutputSource_RoomsDebug_StageRoom(room);
}

void Output_DrawSprite(
    const int32_t x, const int32_t y, const int32_t z, const int16_t sprite_idx,
    const int16_t shade, const RGB_F tint, const DRAW_TYPE draw_type)
{
    Matrix_Push();
    Matrix_TranslateAbs(x, y, z);
    OutputSource_Sprites_Stage(sprite_idx, shade, tint, draw_type);
    Matrix_Pop();
}

void Output_DrawObjectMesh(const OBJECT_MESH *const mesh, const CLIP clip)
{
    OutputSource_Objects_StageObjectMesh(mesh);
    if (g_Config.debug.enable_debug_spheres) {
        Output_DrawSphere(mesh->center, mesh->radius);
    }
}

void Output_DrawObjectMesh_I(const OBJECT_MESH *const mesh, const CLIP clip)
{
    Matrix_Push();
    Matrix_Interpolate();
    Output_DrawObjectMesh(mesh, clip);
    Matrix_Pop();
}

void Output_DrawSkybox(const OBJECT_MESH *const mesh)
{
    float sunset_progress = Output_GetTimeInGame() / Output_GetSunsetDuration();
    CLAMP(sunset_progress, 0.0f, 1.0f);
    OutputSource_Objects_StageSkyboxMesh(
        mesh, SHADE_NEUTRAL + SHADE_SUNSET * sunset_progress);
    SceneCompositor_Flush();
}

void Output_DrawShadow(
    const int16_t size, const BOUNDS_16 *const bounds, const ITEM *const item)
{
    if (!item->enable_shadow) {
        return;
    }

    OUTPUT_ITEM_BIND *const bind = Output_Bind_GetItem(item);
    if (bind->shadow_drawn) {
        return;
    }
    bind->shadow_drawn = true;

    if (g_Config.visuals.shadow_type == SHADOW_TYPE_SPRITE) {
        if (M_DrawShadow_Sprite(size, bounds, item)) {
            return;
        }
    }

    const int32_t x_mid = (bounds->min.x + bounds->max.x) / 2;
    const int32_t z_mid = (bounds->min.z + bounds->max.z) / 2;
    const int32_t x_size = (bounds->max.x - bounds->min.x) * size / 1024;
    const int32_t z_size = (bounds->max.z - bounds->min.z) * size / 1024;

    Matrix_Push();
    *g_MatrixPtr = g_ViewMatrix;
    *g_WMatrixPtr = g_IDMatrix;
    Matrix_TranslateAbs(
        item->interp.result.pos.x, item->interp.result.floor,
        item->interp.result.pos.z);
    Matrix_RotY(item->interp.result.rot.y);
    Matrix_TranslateRel(x_mid, 0, z_mid);
    Matrix_ScaleX((1 << W2V_SHIFT) * x_size / UNIT_SHADOW);
    Matrix_ScaleZ((1 << W2V_SHIFT) * z_size / UNIT_SHADOW);
    OutputSource_Shadows_StageShadow();
    Matrix_Pop();
}

void Output_DrawLightningSegment(const LIGHTNING_SEGMENT segment)
{
    OutputSource_Lightnings_StageSegment(&segment);
}

void Output_DrawScreenSprite(
    const int32_t sx, const int32_t sy, const int32_t z, const int32_t scale_h,
    const int32_t scale_v, const int32_t sprite_idx, const RGBA_F colors[4])
{
    const SPRITE_TEXTURE *const sprite = Output_GetSpriteTexture(sprite_idx);
    const int32_t x0 = sx + (scale_h * sprite->x0 / PHD_ONE);
    const int32_t x1 = sx + (scale_h * sprite->x1 / PHD_ONE);
    const int32_t y0 = sy + (scale_v * sprite->y0 / PHD_ONE);
    const int32_t y1 = sy + (scale_v * sprite->y1 / PHD_ONE);
    OutputSource_UI_StageSprite((OUTPUT_UI_SPRITE) {
        .sprite_idx = sprite_idx,
        .x0 = x0,
        .y0 = y0,
        .x1 = x1,
        .y1 = y1,
        .z = Output_GetNearZ_UI() + z,
        .color = {
            colors[0],
            colors[1],
            colors[2],
            colors[3],
        },
    });
}

void Output_DrawScreenFlatQuad(
    const int32_t sx, const int32_t sy, const int32_t z, const int32_t w,
    const int32_t h, const RGBA_8888 color)
{
    M_DrawScreenQuad(sx, sy, sx + w, sy + h, z, color, color, color, color);
}

void Output_DrawScreenGradientQuad(
    const int32_t sx, const int32_t sy, const int32_t z, const int32_t w,
    const int32_t h, const RGBA_8888 tl, const RGBA_8888 tr, const RGBA_8888 bl,
    const RGBA_8888 br)
{
    M_DrawScreenQuad(sx, sy, sx + w, sy + h, z, tl, tr, bl, br);
}

void Output_DrawScreenFrame(
    const int32_t sx, const int32_t sy, const int32_t w, const int32_t h,
    const RGBA_8888 col_dark, const RGBA_8888 col_light, const float thickness)
{
    const float e = thickness;
    const float x0 = sx;
    const float y0 = sy;
    const float x1 = sx + w;
    const float y1 = sy + h;
    const RGBA_8888 cd = col_dark;
    const RGBA_8888 cl = col_light;

    // clang-format off
    M_DrawScreenQuad(x0,     y0,     x1 - e, y0 + e, 0, cd, cd, cd, cd);
    M_DrawScreenQuad(x0 - e, y0 - e, x1,     y0,     0, cl, cl, cl, cl);
    M_DrawScreenQuad(x1,     y0 - e, x1 + e, y1 + e, 0, cd, cd, cd, cd);
    M_DrawScreenQuad(x1 - e, y0,     x1,     y1,     0, cl, cl, cl, cl);
    M_DrawScreenQuad(x0,     y0,     x0 + e, y1 - e, 0, cd, cd, cd, cd);
    M_DrawScreenQuad(x0 - e, y0 - e, x0,     y1,     0, cl, cl, cl, cl);
    M_DrawScreenQuad(x0 - e, y1,     x1 + e, y1 + e, 0, cd, cd, cd, cd);
    M_DrawScreenQuad(x0 - e, y1 - e, x1,     y1,     0, cl, cl, cl, cl);
    // clang-format on
}

void Output_DrawPhotoModeFrame(const int32_t thickness)
{
    const VIEWPORT_RECT rect = Viewport_GetRect(VIEWPORT_UI);
    const RGBA_8888 color = { 255, 0, 0, 96 };
    OutputSource_UI_StagePhotoModeFrame(rect, color, thickness);
}

void Output_DrawSphere(const XYZ_16 center, const int32_t radius)
{
    const bool wireframe_state = g_Config.rendering.enable_wireframe;
    const RGBA_8888 color_black = { 0, 0, 0, 128 };
    const RGBA_8888 color_white = { 255, 255, 255, 128 };
    const RGBA_8888 color = wireframe_state ? color_black : color_white;
    Output_DrawSphereEx(center, radius, color);
}

void Output_DrawSphereEx(
    const XYZ_16 center, const int32_t radius, const RGBA_8888 color)
{
    Matrix_Push();
    Matrix_TranslateRel16(center);
    Matrix_Scale(radius << W2V_SHIFT);
    OutputSource_Misc_StageSphere(color);
    Matrix_Pop();
}

void Output_DrawCuboid(const BOUNDS_16 *const bounds)
{
    Output_DrawCuboidEx(bounds, (RGBA_8888) { 255, 0, 0, 255 });
}

void Output_DrawCuboidEx(const BOUNDS_16 *const bounds, const RGBA_8888 color)
{
    const int32_t x0 = bounds->min.x;
    const int32_t x1 = bounds->max.x;
    const int32_t y0 = bounds->min.y;
    const int32_t y1 = bounds->max.y;
    const int32_t z0 = bounds->min.z;
    const int32_t z1 = bounds->max.z;
    const int32_t x_mid = (x0 + x1) / 2;
    const int32_t y_mid = (y0 + y1) / 2;
    const int32_t z_mid = (z0 + z1) / 2;
    const int32_t x_size = (x1 - x0) / 2;
    const int32_t y_size = (y1 - y0) / 2;
    const int32_t z_size = (z1 - z0) / 2;
    Matrix_Push();
    Matrix_TranslateRel32((XYZ_32) { x_mid, y_mid, z_mid });
    Matrix_ScaleX(x_size << W2V_SHIFT);
    Matrix_ScaleY(y_size << W2V_SHIFT);
    Matrix_ScaleZ(z_size << W2V_SHIFT);
    OutputSource_Misc_StageCuboid(color);
    Matrix_Pop();
}
