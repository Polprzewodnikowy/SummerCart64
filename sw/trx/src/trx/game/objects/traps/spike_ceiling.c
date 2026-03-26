#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/objects/traps/common.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/game/spawn.h>

#define M_DAMAGE 20
#define M_SPEED 1
#define M_STEP_SLOW 5
#define M_STEP_FAST 10

typedef struct {
    int32_t step;
    bool animate;
} M_PRIV;

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_SHOULD(JSON_READ(io, "step", &p->step));
    JSON_SHOULD(JSON_READ(io, "animate", &p->animate));
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "step", p->step);
    JSONW_WRITE(io, "animate", p->animate);
}

static bool M_Trigger(ITEM *const item, const TRIGGER *const trigger)
{
    M_PRIV *const p = item->priv;
    if (p == nullptr) {
        return true;
    }

    if (trigger == nullptr || trigger->type == TT_ANTITRIGGER
        || trigger->type == TT_ANTIPAD) {
        return true;
    }

    item->timer = 0;
    if (trigger->timer == 1) {
        p->step = M_STEP_FAST;
        p->animate = true;
    } else {
        p->step = M_STEP_SLOW;
        p->animate = false;
    }
    return true;
}

static void M_Initialise(const int16_t item_num)
{
    Trap_Initialise(item_num);
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    if (p != nullptr) {
        p->step = M_STEP_SLOW;
        p->animate = false;
    }
}

static void M_Move(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    const M_PRIV *const p = item->priv;
    const int32_t step = (p != nullptr && p->step > 0) ? p->step : M_STEP_SLOW;
    int16_t room_num = item->room_num;
    const XYZ_32 pos = { item->pos.x, item->pos.y + step, item->pos.z };
    const SECTOR *const sector = Room_GetSector(pos, &room_num);
    if (Room_GetHeight(sector, pos) < pos.y + WALL_L) {
        item->status = IS_DEACTIVATED;
        Sound_StopEffect(SFX_SPIKE_WALL);
    } else {
        item->pos.y = pos.y;
        Item_UpdateRoom(item_num, room_num);
        Sound_Effect(SFX_SPIKE_WALL, &item->pos, SPM_NORMAL);
    }
}

static void M_HitLara(ITEM *const item)
{
    Lara_TakeDamage(M_DAMAGE, true);

    const ITEM *const lara_item = Lara_GetItem();
    Spawn_BloodBath(
        lara_item->pos.x, item->pos.y + LARA_HEIGHT, lara_item->pos.z, M_SPEED,
        item->rot.y, lara_item->room_num, 3);
    item->touch_bits = 0;

    Sound_Effect(SFX_LARA_FLESH_WOUND, &item->pos, SPM_NORMAL);
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    if (!Item_IsTriggerActive(item)) {
        Trap_Reset(item);
    } else if (item->status != IS_DEACTIVATED) {
        M_Move(item_num);
    }

    if (item->touch_bits) {
        M_HitLara(item);
    }

    if (Item_IsTriggerActive(item) && item->status != IS_DEACTIVATED) {
        const M_PRIV *const p = item->priv;
        if (p != nullptr && p->animate) {
            Item_Animate(item);
        }
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Object_Collision_Trap;
    obj->priv_size = sizeof(M_PRIV);
    obj->trigger_func = M_Trigger;
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;
    obj->save_position = true;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_CEILING_SPIKES, M_Setup)
