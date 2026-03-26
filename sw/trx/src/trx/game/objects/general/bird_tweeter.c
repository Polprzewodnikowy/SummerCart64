#include <trx/game/objects/common.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>

static void M_Control(const int16_t item_num)
{
    const ITEM *const item = Item_Get(item_num);

    if (item->object_id == O_BIRD_TWEETER_2) {
        if (Random_GetDraw() < 1024) {
            Sound_Effect(SFX_BIRDS_CHIRP, &item->pos, SPM_NORMAL);
        }
    } else if (Random_GetDraw() < 256) {
        Sound_Effect(SFX_DRIPS_REVERB, &item->pos, SPM_NORMAL);
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->draw_func = nullptr;
}

REGISTER_OBJECT(O_BIRD_TWEETER_1, M_Setup)
REGISTER_OBJECT(O_BIRD_TWEETER_2, M_Setup)
