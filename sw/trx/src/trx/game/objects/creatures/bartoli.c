#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/camera.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/spawn.h>

#define M_BOOM_TIME 130
#define M_BARTOLI_RANGE (WALL_L * 5) // = 5120

typedef struct {
    int16_t dragon_item_num;
} M_PRIV;

static void M_CreateBoom(const OBJECT_ID obj_id, const ITEM *const origin_item)
{
    const int16_t item_num = Item_Create();
    if (item_num == NO_ITEM) {
        return;
    }

    ITEM *const sphere_item = Item_Get(item_num);
    sphere_item->object_id = obj_id;
    sphere_item->pos.x = origin_item->pos.x;
    sphere_item->pos.y = origin_item->pos.y + 256;
    sphere_item->pos.z = origin_item->pos.z;
    sphere_item->room_num = origin_item->room_num;
    sphere_item->shade.value_1 = -1;
    Item_Initialise(item_num);
    Item_AddActive(item_num);
    sphere_item->status = IS_ACTIVE;
}

static void M_ConvertBartoliToDragon(const int16_t item_num)
{
    const ITEM *const bartoli_item = Item_Get(item_num);
    const M_PRIV *const p = bartoli_item->priv;
    const int16_t dragon_item_num = p->dragon_item_num;
    if (dragon_item_num != NO_ITEM) {
        ITEM *const dragon_item = Item_Get(dragon_item_num);
        const OBJECT *const dragon_obj = Object_Get(dragon_item->object_id);
        if (dragon_obj->activate_func != nullptr) {
            dragon_obj->activate_func(dragon_item);
        }
    }
    Item_Kill(item_num);
}

static bool M_CheckLaraProximity(const ITEM *const origin_item)
{
    const ITEM *const lara_item = Lara_GetItem();
    const int32_t dx = ABS(lara_item->pos.x - origin_item->pos.x);
    const int32_t dz = ABS(lara_item->pos.z - origin_item->pos.z);
    return dx < M_BARTOLI_RANGE && dz < M_BARTOLI_RANGE;
}

static int16_t M_GetCarrierItemNum(const ITEM *const item)
{
    const M_PRIV *const p = item->priv;
    return p->dragon_item_num;
}

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    p->dragon_item_num = Item_CreateLevelItem();
    ASSERT(p->dragon_item_num != NO_ITEM);

    ITEM *const dragon_item = Item_Get(p->dragon_item_num);
    dragon_item->object_id = O_DRAGON_BACK;
    dragon_item->pos = item->pos;
    dragon_item->rot.y = item->rot.y;
    dragon_item->room_num = item->room_num;
    Item_Initialise(p->dragon_item_num);
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    if (item->timer == 0) {
        if (M_CheckLaraProximity(item)) {
            item->timer = 1;
        }
        return;
    }

    item->timer++;
    if ((item->timer & 7) == 0) {
        g_Camera.bounce = item->timer;
    }

    Spawn_MysticLight(item_num);
    Item_Animate(item);

    if (item->timer == M_BOOM_TIME + 0) {
        M_CreateBoom(O_SPHERE_OF_DOOM_1, item);
    } else if (item->timer == M_BOOM_TIME + 10) {
        M_CreateBoom(O_SPHERE_OF_DOOM_2, item);
    } else if (item->timer == M_BOOM_TIME + 20) {
        M_CreateBoom(O_SPHERE_OF_DOOM_3, item);
    } else if (item->timer >= M_BOOM_TIME + 20) {
        M_ConvertBartoliToDragon(item_num);
    }
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->carrier_item_num_func = M_GetCarrierItemNum;
    obj->priv_size = sizeof(M_PRIV);

    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_BARTOLI, M_Setup)
