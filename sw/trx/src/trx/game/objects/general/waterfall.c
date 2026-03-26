#include <trx/core/math.h>
#include <trx/game/effects.h>
#include <trx/game/lara/common.h>
#include <trx/game/output/state.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/sparks.h>
#include <trx/version.h>

#define M_RANGE (WALL_L * 10) // = 10240

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (!Item_IsTriggerActive(item)) {
        return;
    }

    const ITEM *const lara_item = Lara_GetItem();

    if (g_TRVersion == 3) {
        if (!Item_IsNearby(item, lara_item, M_RANGE)) {
            return;
        }

        if ((int32_t)Output_GetTimeInGame() % 4 == 0) {
            const XYZ_32 pos = {
                .x = item->pos.x + ((544 * Math_Sin(item->rot.y)) >> W2V_SHIFT),
                .y = item->pos.y,
                .z = item->pos.z + ((544 * Math_Cos(item->rot.y)) >> W2V_SHIFT),
            };
            Sparks_TriggerWaterfallMist(pos.x, pos.y, pos.z, item->rot.y);
        }

        Sound_Effect(SFX_WATERFALL_LOOP, &item->pos, SPM_NORMAL);
        return;
    }

    if (g_TRVersion >= 2 && Item_IsNearby(item, lara_item, M_RANGE)) {
        Sound_Effect(SFX_WATERFALL_LOOP, &item->pos, SPM_NORMAL);
    }

    const int16_t effect_num = Effect_Create(item->room_num);
    if (effect_num != NO_EFFECT) {
        EFFECT *const effect = Effect_Get(effect_num);
        effect->object_id = O_SPLASH_1;
        effect->pos.x =
            item->pos.x + ((Random_GetDraw() - 0x4000) * WALL_L) / 0x7FFF;
        effect->pos.y = item->pos.y;
        effect->pos.z =
            item->pos.z + ((Random_GetDraw() - 0x4000) * WALL_L) / 0x7FFF;
        effect->speed = 0;
        effect->frame_num = 0;
        effect->shade = -1;
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->draw_func = nullptr;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_WATERFALL, M_Setup)
