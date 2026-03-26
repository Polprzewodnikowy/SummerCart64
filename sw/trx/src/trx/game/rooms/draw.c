#include <trx/config.h>
#include <trx/core/utils.h>
#include <trx/core/vector.h>
#include <trx/game/camera.h>
#include <trx/game/effects.h>
#include <trx/game/fx.h>
#include <trx/game/lara.h>
#include <trx/game/matrix.h>
#include <trx/game/output.h>
#include <trx/game/output/bind.h>
#include <trx/game/rooms.h>
#include <trx/game/sparks.h>
#include <trx/version.h>

#include <string.h>

#define M_MAX_BOUND_ROOMS 128

typedef struct {
    int32_t xv;
    int32_t yv;
    int32_t zv;
} M_PORTAL_VBUF;

static inline void M_DrawSet_Init(ROOM_DRAWSET *const s)
{
    s->count = 0;
    memset(s->bits, 0, sizeof(s->bits));
}

static inline bool M_DrawSet_Has(
    const ROOM_DRAWSET *const s, const int16_t item_num)
{
    const uint32_t w = item_num >> 6;
    const uint32_t b = item_num & 63;
    return (s->bits[w] >> b) & 1ULL;
}

static inline bool M_DrawSet_Add(ROOM_DRAWSET *const s, const int16_t item_num)
{
    const uint32_t w = item_num >> 6;
    const uint32_t b = item_num & 63;
    const uint64_t mask = 1ULL << b;
    if (s->bits[w] & mask) {
        return false;
    }
    s->bits[w] |= mask;
    s->count++;
    return true;
}

static inline bool M_DrawSet_Remove(
    ROOM_DRAWSET *const s, const int16_t item_num)
{
    const uint32_t w = item_num >> 6;
    const uint32_t b = item_num & 63;
    const uint64_t mask = 1ULL << b;
    if (!(s->bits[w] & mask)) {
        return false;
    }
    s->bits[w] &= ~mask;
    s->count--;
    return true;
}

static inline void M_DrawSet_ForEach(
    const ROOM_DRAWSET *const s, void (*const fn)(int16_t item, void *ud),
    void *ud)
{
    for (uint32_t w = 0; w < ROOM_DRAWSET_WORDS; w++) {
        uint64_t x = s->bits[w];
        while (x != 0ULL) {
            const uint32_t b = __builtin_ctzll(x);
            fn((int16_t)((w << 6) + b), ud);
            x &= x - 1; // clear lowest set bit
        }
    }
}

static VECTOR *m_RoomsToDraw = nullptr;

static int32_t m_Outside;
static int32_t m_OutsideRight;
static int32_t m_OutsideLeft;
static int32_t m_OutsideTop;
static int32_t m_OutsideBottom;

static int32_t m_BoundStart;
static int32_t m_BoundEnd;
static int32_t m_BoundRooms[M_MAX_BOUND_ROOMS] = {};

static void M_EnsureRoomsToDraw(void)
{
    if (m_RoomsToDraw != nullptr) {
        return;
    }
    m_RoomsToDraw = Vector_CreateAtCapacity(sizeof(int16_t), 100);
}

static inline void M_SetupWaterStatus(const ROOM *const room)
{
    if (room->flags.underwater) {
        Output_SetupBelowWater(g_Camera.underwater);
    } else {
        Output_SetupAboveWater(g_Camera.underwater);
    }
}

static void M_SetBounds(
    const PORTAL *const portal, int32_t room_num, const ROOM *parent);

static inline bool M_PortalFacesCamera(
    const ROOM *const room, const PORTAL *const portal)
{
    // clang-format off
    const XYZ_32 offset = {
        .x = portal->normal.x * (room->pos.x + portal->vertex[0].x - g_ViewPos.x),
        .y = portal->normal.y * (room->pos.y + portal->vertex[0].y - g_ViewPos.y),
        .z = portal->normal.z * (room->pos.z + portal->vertex[0].z - g_ViewPos.z),
    };
    // clang-format on
    return offset.x + offset.y + offset.z < 0;
}

static void M_GetBounds(void)
{
    while (m_BoundStart != m_BoundEnd) {
        const int16_t room_num = m_BoundRooms[m_BoundStart % M_MAX_BOUND_ROOMS];
        m_BoundStart++;
        const ROOM *const room = Room_Get(room_num);
        OUTPUT_ROOM_BIND *const bind = Output_Bind_GetRoom(room);
        bind->active = false;

        CLAMPG(bind->bound_left, bind->test_left);
        CLAMPG(bind->bound_top, bind->test_top);
        CLAMPL(bind->bound_right, bind->test_right);
        CLAMPL(bind->bound_bottom, bind->test_bottom);

        if (!bind->drawn) {
            Room_MarkToBeDrawn(room_num);
            bind->drawn = true;
            if (room->flags.outside) {
                m_Outside = 1;
            }
        }

        if (!room->flags.inside || room->flags.outside) {
            CLAMPG(m_OutsideLeft, bind->bound_left);
            CLAMPG(m_OutsideTop, bind->bound_top);
            CLAMPL(m_OutsideRight, bind->bound_right);
            CLAMPL(m_OutsideBottom, bind->bound_bottom);
        }

        if (room->portals == nullptr) {
            continue;
        }

        Matrix_Push();
        Matrix_TranslateAbs32(room->pos);
        for (int32_t i = 0; i < room->portals->count; i++) {
            PORTAL *const portal = &room->portals->portal[i];
            if (M_PortalFacesCamera(room, portal)) {
                M_SetBounds(portal, portal->room_num, room);
            }
        }
        Matrix_Pop();
    }
}

static void M_SetBounds(
    const PORTAL *const portal, const int32_t room_num,
    const ROOM *const parent)
{
    const ROOM *const room = Room_Get(room_num);
    const OUTPUT_ROOM_BIND *const parent_bind = Output_Bind_GetRoom(parent);
    OUTPUT_ROOM_BIND *const bind = Output_Bind_GetRoom(room);

    if (bind->bound_left <= parent_bind->test_left
        && bind->bound_top <= parent_bind->test_top
        && bind->bound_right >= parent_bind->test_right
        && bind->bound_bottom >= parent_bind->test_bottom) {
        return;
    }

    const MATRIX *const m = g_MatrixPtr;
    int32_t left = parent_bind->test_right;
    int32_t right = parent_bind->test_left;
    int32_t bottom = parent_bind->test_top;
    int32_t top = parent_bind->test_bottom;

    M_PORTAL_VBUF portal_vbuf[4];
    int32_t too_near = 0;

    for (int32_t i = 0; i < 4; i++) {
        M_PORTAL_VBUF *const dvbuf = &portal_vbuf[i];
        const XYZ_16 *const dvtx = &portal->vertex[i];
        const int32_t xv =
            dvtx->x * m->_00 + dvtx->y * m->_01 + dvtx->z * m->_02 + m->_03;
        const int32_t yv =
            dvtx->x * m->_10 + dvtx->y * m->_11 + dvtx->z * m->_12 + m->_13;
        const int32_t zv =
            dvtx->x * m->_20 + dvtx->y * m->_21 + dvtx->z * m->_22 + m->_23;
        dvbuf->xv = xv;
        dvbuf->yv = yv;
        dvbuf->zv = zv;

        if (zv <= 0) {
            too_near++;
            continue;
        }

        int32_t xs;
        int32_t ys;
        const int32_t zp = zv / g_PhdPersp;
        if (zp != 0) {
            xs = Viewport_GetCenterX(VIEWPORT_GAME) + xv / zp;
            ys = Viewport_GetCenterY(VIEWPORT_GAME) + yv / zp;
        } else {
            xs = xv < 0 ? g_PhdLeft : g_PhdRight;
            ys = yv < 0 ? g_PhdTop : g_PhdBottom;
        }

        if (xs - 1 < left) {
            left = xs - 1;
        }
        if (xs + 1 > right) {
            right = xs + 1;
        }
        if (ys - 1 < top) {
            top = ys - 1;
        }
        if (ys + 1 > bottom) {
            bottom = ys + 1;
        }
    }

    if (too_near == 4) {
        return;
    }

    if (too_near > 0) {
        const M_PORTAL_VBUF *dest = &portal_vbuf[0];
        const M_PORTAL_VBUF *last = &portal_vbuf[3];

        for (int32_t i = 0; i < 4; i++, last = dest, dest++) {
            if ((dest->zv <= 0) == (last->zv <= 0)) {
                continue;
            }

            if (dest->xv < 0 && last->xv < 0) {
                left = Viewport_GetMinX(VIEWPORT_GAME);
            } else if (dest->xv > 0 && last->xv > 0) {
                right = Viewport_GetMaxX(VIEWPORT_GAME);
            } else {
                left = Viewport_GetMinX(VIEWPORT_GAME);
                right = Viewport_GetMaxX(VIEWPORT_GAME);
            }

            if (dest->yv < 0 && last->yv < 0) {
                top = Viewport_GetMinY(VIEWPORT_GAME);
            } else if (dest->yv > 0 && last->yv > 0) {
                bottom = Viewport_GetMaxY(VIEWPORT_GAME);
            } else {
                top = Viewport_GetMinY(VIEWPORT_GAME);
                bottom = Viewport_GetMaxY(VIEWPORT_GAME);
            }
        }
    }

    if (left < parent_bind->test_left) {
        left = parent_bind->test_left;
    }
    if (right > parent_bind->test_right) {
        right = parent_bind->test_right;
    }
    if (top < parent_bind->test_top) {
        top = parent_bind->test_top;
    }
    if (bottom > parent_bind->test_bottom) {
        bottom = parent_bind->test_bottom;
    }

    if (left >= right || top >= bottom) {
        return;
    }

    if (bind->active) {
        CLAMPG(bind->test_left, left);
        CLAMPG(bind->test_top, top);
        CLAMPL(bind->test_right, right);
        CLAMPL(bind->test_bottom, bottom);
    } else {
        m_BoundRooms[m_BoundEnd % M_MAX_BOUND_ROOMS] = room_num;
        m_BoundEnd++;
        bind->active = true;
        bind->test_left = left;
        bind->test_top = top;
        bind->test_right = right;
        bind->test_bottom = bottom;
    }
}

static void M_DrawSkybox(void)
{
    if (!Output_IsSkyboxEnabled()) {
        return;
    }

    g_PhdLeft = m_OutsideLeft;
    g_PhdTop = m_OutsideTop;
    g_PhdRight = m_OutsideRight;
    g_PhdBottom = m_OutsideBottom;

    const OBJECT *const skybox = Object_Get(O_SKYBOX);
    if (skybox->loaded) {
        Output_SetupAboveWater(g_Camera.underwater);
        Matrix_PushUnit();
        Matrix_TranslateAbs32(g_ViewPos);
        Matrix_Rot16(skybox->frame_base->mesh_rots[0]);
        Output_CalculateStaticLight(Output_GetSkyShade());
        Output_DrawSkybox(Object_GetMesh(skybox->mesh_idx));
        Matrix_Pop();
    } else {
        m_Outside = -1;
    }
}

static void M_DrawRoomItem(const int16_t item_num, void *const ud)
{
    ITEM *const item = Item_Get(item_num);
    const OBJECT *const obj = Object_Get(item->object_id);
    OUTPUT_ITEM_BIND *const bind = Output_Bind_GetItem(item);
    if (bind->drawn || item->status == IS_INVISIBLE
        || obj->draw_func == nullptr) {
        return;
    }

    M_SetupWaterStatus(Room_Get(item->room_num));
    bind->drawn |= obj->draw_func(item);

    if (Output_IsControlFrame()) {
        Item_ControlDraw(item);
    }
}

static void M_DrawSingleRoom(const ROOM *const room)
{
    Output_SetCurrentRoom(room);
    M_SetupWaterStatus(room);

    OUTPUT_ROOM_BIND *const bind = Output_Bind_GetRoom(room);
    g_PhdLeft = bind->bound_left;
    g_PhdTop = bind->bound_top;
    g_PhdRight = bind->bound_right;
    g_PhdBottom = bind->bound_bottom;

    if (g_Config.debug.enable_debug_room_clip) {
        Output_DrawScreenFrame(
            g_PhdLeft, g_PhdTop, g_PhdRight - g_PhdLeft, g_PhdBottom - g_PhdTop,
            (RGBA_8888) { 0, 255, 0, 128 }, (RGBA_8888) { 0, 255, 0, 128 }, 1);
    }

    Matrix_TranslateAbs32(room->pos);
    Output_DrawRoom(room, false);

    M_SetupWaterStatus(room);

    Matrix_Push();
    Matrix_TranslateAbs32(room->pos);

    g_PhdLeft = bind->bound_left;
    g_PhdTop = bind->bound_top;
    g_PhdRight = bind->bound_right;
    g_PhdBottom = bind->bound_bottom;

    for (int32_t i = 0; i < room->num_static_meshes; i++) {
        const STATIC_MESH *const mesh = &room->static_meshes[i];
        const STATIC_OBJECT_3D *const obj =
            Object_Get3DStatic(mesh->static_num);
        if (!obj->visible) {
            continue;
        }

        Matrix_Push();
        Matrix_TranslateAbs32(mesh->pos);
        Matrix_RotY(mesh->rot.y);
        const CLIP clip = Output_CheckBoundsClip(&obj->draw_bounds);
        if (clip != CLIP_NOT_VISIBLE) {
            Output_CalculateStaticMeshLight(mesh->pos, mesh->shade, room);
            Object_DrawMesh(obj->mesh_idx, clip, false);
            if (g_Config.debug.enable_debug_bounding_boxes) {
                Output_DrawCuboid(&obj->draw_bounds);
            }
        }
        Matrix_Pop();
    }

    M_DrawSet_ForEach(&room->drawn_items, M_DrawRoomItem, nullptr);
    M_SetupWaterStatus(room);

    g_PhdLeft = Viewport_GetMinX(VIEWPORT_GAME);
    g_PhdTop = Viewport_GetMinY(VIEWPORT_GAME);
    g_PhdRight = Viewport_GetMaxX(VIEWPORT_GAME);
    g_PhdBottom = Viewport_GetMaxY(VIEWPORT_GAME);

    int16_t effect_num = room->effect_num;
    while (effect_num != NO_EFFECT) {
        const EFFECT *const effect = Effect_Get(effect_num);
        Effect_Draw(effect_num);
        effect_num = effect->next_free;
    }

    Matrix_Pop();

    bind->bound_left = Viewport_GetMaxX(VIEWPORT_GAME);
    bind->bound_top = Viewport_GetMaxY(VIEWPORT_GAME);
    bind->bound_right = Viewport_GetMinX(VIEWPORT_GAME);
    bind->bound_bottom = Viewport_GetMinY(VIEWPORT_GAME);
}

void Room_DrawReset(void)
{
    M_EnsureRoomsToDraw();
    Vector_Clear(m_RoomsToDraw);
}

void Room_MarkToBeDrawn(const int16_t room_num)
{
    if (Vector_Contains(m_RoomsToDraw, &room_num)) {
        return;
    }
    Vector_Add(m_RoomsToDraw, &room_num);
}

int32_t Room_DrawGetCount(void)
{
    return m_RoomsToDraw->count;
}

int16_t Room_DrawGetRoom(const int16_t idx)
{
    return *(int16_t *)Vector_Get(m_RoomsToDraw, idx);
}

void Room_DrawAllRooms(const int16_t current_room, const int16_t target_room)
{
    const ROOM *const room = Room_Get(current_room);
    Output_Bind_ResetRooms();
    OUTPUT_ROOM_BIND *const bind = Output_Bind_GetRoom(room);
    bind->test_left = Viewport_GetMinX(VIEWPORT_GAME);
    bind->test_top = Viewport_GetMinY(VIEWPORT_GAME);
    bind->test_right = Viewport_GetMaxX(VIEWPORT_GAME);
    bind->test_bottom = Viewport_GetMaxY(VIEWPORT_GAME);
    bind->active = true;
    bind->drawn = false;

    g_PhdLeft = bind->test_left;
    g_PhdTop = bind->test_top;
    g_PhdRight = bind->test_right;
    g_PhdBottom = bind->test_bottom;

    m_BoundRooms[0] = current_room;
    m_BoundStart = 0;
    m_BoundEnd = 1;

    Room_DrawReset();
    m_Outside = room->flags.outside;

    if (m_Outside) {
        m_OutsideLeft = Viewport_GetMinX(VIEWPORT_GAME);
        m_OutsideTop = Viewport_GetMinY(VIEWPORT_GAME);
        m_OutsideRight = Viewport_GetMaxX(VIEWPORT_GAME);
        m_OutsideBottom = Viewport_GetMaxY(VIEWPORT_GAME);
    } else {
        m_OutsideLeft = Viewport_GetMaxX(VIEWPORT_GAME);
        m_OutsideTop = Viewport_GetMaxY(VIEWPORT_GAME);
        m_OutsideBottom = Viewport_GetMinY(VIEWPORT_GAME);
        m_OutsideRight = Viewport_GetMinX(VIEWPORT_GAME);
    }

    M_GetBounds();

    if (m_Outside) {
        M_DrawSkybox();
    }

    Output_Bind_ResetItems();

    for (int32_t i = 0; i < Room_DrawGetCount(); i++) {
        const int16_t draw_room_num = Room_DrawGetRoom(i);
        const ROOM *const draw_room = Room_Get(draw_room_num);
        M_DrawSingleRoom(draw_room);
        OUTPUT_ROOM_BIND *const draw_bind = Output_Bind_GetRoom(draw_room);
        draw_bind->active = false;
        draw_bind->drawn = false;
    }

    const ITEM *const lara_item = Lara_GetItem();
    if (Object_Get(O_LARA)->loaded) {
        const ROOM *const lara_room = Room_Get(lara_item->room_num);
        M_SetupWaterStatus(lara_room);
        Output_SetCurrentRoom(lara_room);
        Lara_Draw(lara_item);
    }

    Output_SetupAboveWater(false);
    FX_Draw();
    Sparks_Draw();
}

void Room_AddDrawnItem(const int16_t room_num, const int16_t item_num)
{
    if (room_num != NO_ROOM) {
        ROOM *const room = Room_Get(room_num);
        M_DrawSet_Add(&room->drawn_items, item_num);
    }
}

void Room_RemoveDrawnItem(const int16_t room_num, const int16_t item_num)
{
    if (room_num != NO_ROOM) {
        ROOM *const room = Room_Get(room_num);
        M_DrawSet_Remove(&room->drawn_items, item_num);
    }
}

__attribute__((destructor)) static void M_Shutdown(void)
{
    Vector_Free(m_RoomsToDraw);
    m_RoomsToDraw = nullptr;
}
