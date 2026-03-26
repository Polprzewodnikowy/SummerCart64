#include <trx/game/effects.h>
#include <trx/game/objects.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (!Item_IsTriggerActive(item)) {
        item->status = IS_INACTIVE;
        Item_RemoveActive(item_num);
        return;
    }

    const int16_t effect_num = Effect_Create(item->room_num);
    if (effect_num == NO_EFFECT) {
        return;
    }

    EFFECT *const effect = Effect_Get(effect_num);
    effect->pos = item->pos;
    effect->rot.y = 2 * Random_GetControl() + 0x8000;
    effect->speed = Random_GetControl() >> 10;
    effect->fall_speed = Random_GetControl() / -200;
    effect->frame_num = (-4 * Random_GetControl()) / 0x7FFF;
    effect->object_id = O_EMBER;
    Sound_Effect(SFX_LAVA_FOUNTAIN, &item->pos, SPM_NORMAL);
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->collision_func = Object_Collision;
    obj->draw_func = nullptr;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_EMBER_EMITTER, M_Setup)
