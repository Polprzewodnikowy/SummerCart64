#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/pathing.h>

#define M_MAX_SLOTS 3

typedef struct {
    int32_t cooldown;
    int16_t slots[M_MAX_SLOTS];
} M_PRIV;

static void M_SpawnRaptor(const ITEM *const spawner_item, int32_t slot_idx)
{
    M_PRIV *const p = spawner_item->priv;

    ITEM *const raptor_item = Item_Get(p->slots[slot_idx]);
    raptor_item->pos = spawner_item->pos;
    raptor_item->rot = spawner_item->rot;
    Item_SwitchToAnim(raptor_item, 0, 0);
    raptor_item->current_anim_state =
        Item_GetAnim(raptor_item)->current_anim_state;
    raptor_item->goal_anim_state = raptor_item->current_anim_state;
    raptor_item->required_anim_state = 0;
    raptor_item->flags &= ~(IF_INVISIBLE | IF_KILLED | 3); // 3?
    raptor_item->creature_data = nullptr;
    raptor_item->hit_points = Object_Get(raptor_item->object_id)->hit_points;
    raptor_item->mesh_bits = -1;
    raptor_item->status = IS_ACTIVE;
    raptor_item->collidable = true;

    if (raptor_item->active) {
        Item_RemoveActive(p->slots[slot_idx]);
    }

    Item_AddActive(p->slots[slot_idx]);
    Item_UpdateRoom(p->slots[slot_idx], NO_ITEM);
    Item_UpdateRoom(p->slots[slot_idx], spawner_item->room_num);
    LOT_EnableBaddieAI(p->slots[slot_idx], true);
}

static void M_PopulateSlots(M_PRIV *const p)
{
    int32_t count = 0;
    for (int32_t i = 0; i < Item_GetLevelCount(); i++) {
        ITEM *const item = Item_Get(i);
        if (item->object_id != O_RAPTOR || !(item->ai_bits & AI_MODIFY)) {
            continue;
        }

        p->slots[count++] = i;
        if (count >= M_MAX_SLOTS) {
            break;
        }
    }
}

static int32_t M_GetEmptySlot(const M_PRIV *const p)
{
    for (int32_t i = 0; i < M_MAX_SLOTS; i++) {
        const ITEM *const item = Item_Get(p->slots[i]);
        if (item->creature_data == nullptr) {
            return i;
        }
    }
    return -1;
}

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_SHOULD(JSON_READ(io, "cooldown", &p->cooldown));
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "cooldown", p->cooldown);
}

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    p->cooldown = 96 * (item_num & 3);
    for (int32_t i = 0; i < M_MAX_SLOTS; i++) {
        p->slots[i] = NO_ITEM;
    }
}

static void M_Control(int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (!item->active || item->timer <= 0) {
        return;
    }

    M_PRIV *const p = item->priv;
    if (p->slots[0] == NO_ITEM) {
        M_PopulateSlots(p);
        return;
    }

    int16_t m_EmptySlot = M_GetEmptySlot(p);
    if (m_EmptySlot == -1) {
        return;
    }

    const ITEM *const lara_item = Lara_GetItem();
    const int32_t dist = XYZ_32_GetDistance(lara_item->pos, item->pos);
    if (dist < 4 * WALL_L) {
        return;
    }

    if (p->cooldown > 0) {
        p->cooldown--;
        return;
    }

    p->cooldown = 255;
    item->timer -= 30;
    M_SpawnRaptor(item, m_EmptySlot);
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->priv_size = sizeof(M_PRIV);
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;

    obj->draw_func = nullptr;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_RAPTOR_EMITTER, M_Setup)
