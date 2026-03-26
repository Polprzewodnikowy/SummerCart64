#include <trx/game/objects/creatures/pod.h>

#include <trx/core/utils.h>
#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/pathing.h>

#define POD_EXPLODE_DIST (WALL_L * 4) // = 4096

typedef enum {
    POD_STATE_SET = 0,
    POD_STATE_EXPLODE = 1,
} POD_STATE;

typedef struct {
    int16_t bug_item_num;
} M_PRIV;

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    p->bug_item_num = NO_ITEM;

    const int16_t bug_item_num = Item_CreateLevelItem();
    if (bug_item_num != NO_ITEM) {
        ITEM *const bug = Item_Get(bug_item_num);
        bug->object_id = Pod_GetBugObjectID(item);
        bug->room_num = item->room_num;
        bug->pos.x = item->pos.x;
        bug->pos.y = item->pos.y;
        bug->pos.z = item->pos.z;
        bug->rot.y = item->rot.y;
        bug->flags = IF_INVISIBLE;
        bug->shade.value_1 = -1;

        Item_Initialise(bug_item_num);
        p->bug_item_num = bug_item_num;
    }

    item->flags = 0;
    item->mesh_bits = 0xFF0001FF;
}

static void M_HandleSave(ITEM *const item, const SAVEGAME_STAGE stage)
{
    if (stage == SAVEGAME_STAGE_AFTER_LOAD) {
        if (item->status == IS_DEACTIVATED) {
            item->mesh_bits = 0x1FF;
            item->collidable = false;
        }
    }
}

static int16_t M_GetCarrierItemNum(const ITEM *const item)
{
    const M_PRIV *const p = item->priv;
    return p->bug_item_num;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    if (item->goal_anim_state != POD_STATE_EXPLODE) {
        int32_t explode = 0;

        if (item->flags & IF_ONE_SHOT) {
            explode = 1;
        } else if (item->object_id == O_BIG_POD) {
            explode = 1;
        } else {
            const ITEM *const lara_item = Lara_GetItem();
            int32_t x = lara_item->pos.x - item->pos.x;
            int32_t y = lara_item->pos.y - item->pos.y;
            int32_t z = lara_item->pos.z - item->pos.z;
            if (ABS(x) < POD_EXPLODE_DIST && ABS(y) < POD_EXPLODE_DIST
                && ABS(z) < POD_EXPLODE_DIST) {
                explode = 1;
            }
        }

        if (explode) {
            item->goal_anim_state = POD_STATE_EXPLODE;
            item->mesh_bits = 0xFFFFFF;
            item->collidable = false;
            Item_Explode(item_num, 0xFFFE00, 0);

            const M_PRIV *const p = item->priv;
            if (p->bug_item_num != NO_ITEM) {
                ITEM *const bug = Item_Get(p->bug_item_num);
                if (Object_Get(bug->object_id)->loaded) {
                    bug->touch_bits = 0;
                    Item_AddActive(p->bug_item_num);
                    if (LOT_EnableBaddieAI(p->bug_item_num, 0)) {
                        bug->status = IS_ACTIVE;
                    } else {
                        bug->status = IS_INVISIBLE;
                    }
                }
            }
            item->status = IS_DEACTIVATED;
        }
    }

    Item_Animate(item);
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }
    obj->initialise_func = M_Initialise;
    obj->handle_save_func = M_HandleSave;
    obj->control_func = M_Control;
    obj->collision_func = Object_Collision;
    obj->carrier_item_num_func = M_GetCarrierItemNum;
    obj->priv_size = sizeof(M_PRIV);
    obj->save_anim = true;
    obj->save_flags = true;
}

OBJECT_ID Pod_GetBugObjectID(const ITEM *const item)
{
    switch ((item->flags & IF_CODE_BITS) >> 9) {
    case 1:
        return O_WARRIOR_2;
    case 2:
        return O_CENTAUR;
    case 4:
        return O_TORSO;
    case 8:
        return O_WARRIOR_3;
    default:
        return O_WARRIOR_1;
    }
}

REGISTER_OBJECT(O_PODS, M_Setup)
REGISTER_OBJECT(O_BIG_POD, M_Setup)
