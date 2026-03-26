#include <trx/game/items.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/sound.h>

typedef struct {
    int32_t counter;
} M_PRIV;

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if ((item->flags & IF_CODE_BITS) != IF_CODE_BITS) {
        return;
    }

    Sound_Effect(SFX_PLATFORM_ALARM, &item->pos, SPM_NORMAL);

    M_PRIV *const p = item->priv;
    p->counter++;
    if (p->counter > 6) {
        Output_AddDynamicLight(item->pos, 12, 11);
        if (p->counter > 12) {
            p->counter = 0;
        }
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->priv_size = sizeof(M_PRIV);
    obj->save_flags = true;
}

REGISTER_OBJECT(O_ALARM_SOUND, M_Setup)
