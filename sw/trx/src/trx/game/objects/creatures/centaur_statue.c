#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/pathing.h>
#include <trx/game/sound.h>

#define STATUE_EXPLODE_DIST (WALL_L * 7 / 2) // = 3584
#define CENTAUR_REARING_ANIM 7
#define CENTAUR_REARING_FRAME 36

typedef struct {
    int16_t centaur_item_num;
} M_PRIV;

static void M_Initialise(const int16_t item_num)
{
    OBJECT *const obj = Object_Get(O_CENTAUR);
    if (!obj->loaded) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    p->centaur_item_num = NO_ITEM;

    const int16_t centaur_item_num = Item_CreateLevelItem();
    ASSERT(centaur_item_num != NO_ITEM);

    ITEM *const centaur = Item_Get(centaur_item_num);
    centaur->object_id = O_CENTAUR;
    centaur->room_num = item->room_num;
    centaur->pos.x = item->pos.x;
    centaur->pos.y = item->pos.y;
    centaur->pos.z = item->pos.z;
    centaur->flags = IF_INVISIBLE;
    centaur->shade.value_1 = -1;

    Item_Initialise(centaur_item_num);

    Item_SwitchToAnim(centaur, CENTAUR_REARING_ANIM, CENTAUR_REARING_FRAME);
    centaur->current_anim_state = Item_GetAnim(centaur)->current_anim_state;
    centaur->goal_anim_state = centaur->current_anim_state;
    centaur->rot.y = item->rot.y;

    p->centaur_item_num = centaur_item_num;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (item->flags & IF_KILLED) {
        return;
    }

    const ITEM *const lara_item = Lara_GetItem();
    const int32_t x = lara_item->pos.x - item->pos.x;
    const int32_t y = lara_item->pos.y - item->pos.y;
    const int32_t z = lara_item->pos.z - item->pos.z;

    if (y > -WALL_L && y < WALL_L
        && SQUARE(x) + SQUARE(z) < SQUARE(STATUE_EXPLODE_DIST)) {
        Item_Explode(item_num, -1, 0);
        Item_Kill(item_num);
        item->status = IS_DEACTIVATED;

        const M_PRIV *const p = item->priv;
        if (p->centaur_item_num != NO_ITEM) {
            ITEM *const centaur = Item_Get(p->centaur_item_num);
            centaur->touch_bits = 0;
            Item_AddActive(p->centaur_item_num);
            LOT_EnableBaddieAI(p->centaur_item_num, 1);
            centaur->status = IS_ACTIVE;
            Sound_Effect(SFX_EXPLOSION_1, &centaur->pos, SPM_NORMAL);
        } else {
            Sound_Effect(SFX_EXPLOSION_1, &item->pos, SPM_NORMAL);
        }
    }
}

static int16_t M_GetCarrierItemNum(const ITEM *const item)
{
    const M_PRIV *const p = item->priv;
    return p->centaur_item_num;
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }
    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Object_Collision;
    obj->carrier_item_num_func = M_GetCarrierItemNum;
    obj->priv_size = sizeof(M_PRIV);
    obj->save_anim = true;
    obj->save_flags = true;
    obj->enable_interpolation = false;
}

REGISTER_OBJECT(O_CENTAUR_STATUE, M_Setup)
