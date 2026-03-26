#include <trx/game/effects.h>
#include <trx/game/objects/common.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>

typedef enum {
    // clang-format off
    BIG_BOWL_STATE_TIP  = 0,
    BIG_BOWL_STATE_POUR = 1,
    // clang-format on
} BIG_BOWL_STATE;

static void M_CreateHotLiquid(const ITEM *const bowl_item)
{
    const int16_t effect_num = Effect_Create(bowl_item->room_num);
    const OBJECT *const obj = Object_Get(O_HOT_LIQUID);
    if (effect_num != NO_EFFECT) {
        EFFECT *const effect = Effect_Get(effect_num);
        effect->object_id = O_HOT_LIQUID;
        effect->pos.x = bowl_item->pos.x + STEP_L * 2;
        effect->pos.z = bowl_item->pos.z + STEP_L * 2;
        effect->pos.y = bowl_item->pos.y + STEP_L * 2 + 100;
        effect->room_num = bowl_item->room_num;
        effect->frame_num = (obj->mesh_count * Random_GetDraw()) >> 15;
        effect->fall_speed = 0;
        effect->shade = 2048;
    }
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    if (item->current_anim_state == BIG_BOWL_STATE_POUR) {
        M_CreateHotLiquid(item);
        item->timer++;
        if (item->timer == 5 * LOGIC_FPS && !Room_GetFlipStatus()) {
            // TODO: poorly hardcoded flimap number
            Room_SetFlipSlotFlags(4, IF_CODE_BITS | IF_ONE_SHOT);
            Room_FlipMap();
        }
    }

    Item_Animate(item);

    if (item->status == IS_DEACTIVATED && item->timer >= LOGIC_FPS * 7) {
        Item_RemoveActive(item_num);
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_BIG_BOWL, M_Setup)
