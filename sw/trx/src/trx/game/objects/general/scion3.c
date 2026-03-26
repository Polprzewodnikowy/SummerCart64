// The Great Pyramid shootable Scion.

#include <trx/config.h>
#include <trx/game/camera.h>
#include <trx/game/effects.h>
#include <trx/game/objects.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>

static bool M_ShouldSpawnBlood(const ITEM *const item)
{
    return !g_Config.visuals.fix_texture_issues;
}

static bool M_CanTakeDamage(const ITEM *const item)
{
    return item->status == IS_ACTIVE;
}

static void M_Control(const int16_t item_num)
{
    static int32_t counter = 0;
    ITEM *const item = Item_Get(item_num);

    if (Item_IsTriggerActive(item)) {
        if (!LOT_EnableBaddieAI(item_num, true)) {
            return;
        }
        item->status = IS_ACTIVE;
    }

    if (item->hit_points > 0) {
        counter = 0;
        Item_Animate(item);
        return;
    }

    if (counter == 0) {
        item->status = IS_INVISIBLE;
        item->hit_points = 0;
        Room_TestTriggers(item);
        Item_RemoveDrawn(item_num);
    }

    if (counter % 10 == 0) {
        int16_t effect_num = Effect_Create(item->room_num);
        if (effect_num != NO_EFFECT) {
            EFFECT *effect = Effect_Get(effect_num);
            effect->pos.x = item->pos.x + (Random_GetControl() - 0x4000) / 32;
            effect->pos.y =
                item->pos.y + (Random_GetControl() - 0x4000) / 256 - 500;
            effect->pos.z = item->pos.z + (Random_GetControl() - 0x4000) / 32;
            effect->speed = 0;
            effect->frame_num = 0;
            effect->object_id = O_EXPLOSION_1;
            effect->counter = 0;
            Sound_Effect(SFX_EXPLOSION_1, &effect->pos, SPM_NORMAL);
            g_Camera.bounce = -200;
        }
    }

    counter++;
    if (counter >= LOGIC_FPS * 3) {
        Item_Kill(item_num);
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->can_take_damage_func = M_CanTakeDamage;
    obj->should_spawn_blood_func = M_ShouldSpawnBlood;
    obj->hit_points = 5;
    obj->save_flags = true;
    obj->save_hitpoints = true;
}

REGISTER_OBJECT(O_SCION_ITEM_3, M_Setup)
