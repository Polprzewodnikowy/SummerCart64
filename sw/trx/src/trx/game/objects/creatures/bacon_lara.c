#include <trx/game/objects/creatures/bacon_lara.h>

#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/pathing.h>
#include <trx/game/rooms.h>

#define M_SMASH_JUMP_FRAME 1

typedef struct {
    bool status;
} M_PRIV;

static int32_t m_AnchorX = -1;
static int32_t m_AnchorZ = -1;

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_SHOULD(JSON_READ(io, "status", &p->status));
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "status", p->status);
}

static void M_Initialise(const int16_t item_num)
{
    const ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    const OBJECT *const lara_obj = Object_Get(O_LARA);
    OBJECT *const bacon_obj = Object_Get(O_BACON_LARA);
    bacon_obj->anim_idx = lara_obj->anim_idx;
    bacon_obj->frame_base = lara_obj->frame_base;
    p->status = false;
}

static void M_Control(const int16_t item_num)
{
    if (m_AnchorX == -1) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    const ITEM *const lara_item = Lara_GetItem();

    if (Item_IsTriggerActive(item)) {
        if (!LOT_EnableBaddieAI(item_num, true)) {
            return;
        }
        item->status = IS_ACTIVE;
    }

    if (item->hit_points < LARA_MAX_HITPOINTS) {
        Lara_TakeDamage((LARA_MAX_HITPOINTS - item->hit_points) * 10, false);
        item->hit_points = LARA_MAX_HITPOINTS;
    }

    if (!p->status) {
        const XYZ_32 pos = {
            .x = 2 * m_AnchorX - lara_item->pos.x,
            .z = 2 * m_AnchorZ - lara_item->pos.z,
            .y = lara_item->pos.y,
        };

        int16_t room_num = item->room_num;
        const SECTOR *sector = Room_GetSector(pos, &room_num);
        const int32_t h = Room_GetHeight(sector, pos);
        item->floor = h;

        room_num = lara_item->room_num;
        sector = Room_GetSector(lara_item->pos, &room_num);
        int32_t lh = Room_GetHeight(sector, lara_item->pos);

        const int16_t relative_anim = Item_GetRelativeAnim(lara_item);
        const int16_t relative_frame = Item_GetRelativeFrame(lara_item);
        Item_SwitchToObjAnim(item, relative_anim, relative_frame, O_LARA);
        item->pos = pos;
        item->rot = lara_item->rot;
        item->rot.y -= DEG_180;
        Item_UpdateRoom(item_num, lara_item->room_num);

        if (h >= lh + WALL_L && !lara_item->gravity) {
            item->current_anim_state = LS(LS_FAST_FALL);
            item->goal_anim_state = LS(LS_FAST_FALL);
            Item_SwitchToAnim(item, LA(LA_SMASH_JUMP), M_SMASH_JUMP_FRAME);
            item->speed = 0;
            item->fall_speed = 0;
            item->gravity = true;
            item->pos.y += 50;
            p->status = true;
        }
    }

    if (p->status) {
        Item_Animate(item);

        int16_t room_num = item->room_num;
        const SECTOR *sector = Room_GetSector(item->pos, &room_num);
        const int32_t h = Room_GetHeight(sector, item->pos);
        item->floor = h;

        Room_TestTriggers(item);
        if (item->pos.y >= h) {
            item->floor = h;
            item->pos.y = h;
            Room_TestTriggers(item);
            item->gravity = false;
            item->fall_speed = 0;
            item->goal_anim_state = LS(LS_DEATH);
            item->required_anim_state = LS(LS_DEATH);
        }
    }
}

static bool M_Draw(const ITEM *const item)
{
    M_PRIV *const p = item->priv;
    if (p->status || item->current_anim_state == LS(LS_DEATH)) {
        return Object_DrawAnimatingItem(item);
    }

    OBJECT_MESH *old_mesh_ptrs[LM_NUMBER_OF];

    for (LARA_MESH mesh = LM_FIRST; mesh < LM_NUMBER_OF; mesh++) {
        old_mesh_ptrs[mesh] = Lara_Mesh_Get(mesh);
        Lara_Mesh_SwapSingle(mesh, O_BACON_LARA);
    }

    Lara_Draw(item);

    for (LARA_MESH mesh = LM_FIRST; mesh < LM_NUMBER_OF; mesh++) {
        Lara_Mesh_Set(mesh, old_mesh_ptrs[mesh]);
    }
    return true;
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->draw_func = M_Draw;
    obj->collision_func = Creature_Collision;
    obj->priv_size = sizeof(M_PRIV);
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;
    obj->hit_points = LARA_MAX_HITPOINTS;
    obj->shadow_size = (UNIT_SHADOW * 10) / 16;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;
}

bool BaconLara_InitialiseAnchor(const int32_t room_index)
{
    if (room_index >= Room_GetCount()) {
        return false;
    }
    const ROOM *const room = Room_Get(room_index);
    m_AnchorX = room->pos.x + room->size.x * (WALL_L >> 1);
    m_AnchorZ = room->pos.z + room->size.z * (WALL_L >> 1);
    return true;
}

REGISTER_OBJECT(O_BACON_LARA, M_Setup)
