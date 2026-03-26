#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/random.h>
#include <trx/game/sparks.h>

typedef struct {
    int32_t life;
} M_PRIV;

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;

    int32_t rg, b;
    if (!Item_IsTriggerActive(item)) {
        p->life = 0;
        return;
    }

    if (p->life < 16) {
        rg = (Random_GetControl() % 8) << 2;
        b = rg + (Random_GetControl() % 4);
        p->life++;
    } else if (p->life < 96) {
        if (((int32_t)Output_GetTimeInGame() % 16)
            && (Random_GetControl() % 8)) {
            rg = Random_GetControl() % 8;
        } else {
            rg = 24 - (Random_GetControl() % 8);
        }

        b = rg + (Random_GetControl() % 4);
        p->life++;
    } else if (p->life < 160) {
        rg = 12 - (Random_GetControl() % 4);
        b = rg + (Random_GetControl() % 4);

        if (!(Random_GetControl() % 0x20) && p->life > 128) {
            p->life = 160;
        } else {
            p->life++;
        }
    } else {
        rg = 31 - (Random_GetControl() % 4);
        b = 31 - (Random_GetControl() % 2);

        if (item->object_id == O_FLICKERING_LIGHT
            && (Random_GetControl() < 0x200)) {
            p->life = 0;
        }
    }

    rg <<= 3;
    b <<= 3;
    Output_AddDynamicLightRGB(item->pos, 16, (RGB_888) { rg, rg, b });
}

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_SHOULD(JSON_READ(io, "life", &p->life));
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "life", p->life);
}

static void M_Setup(OBJECT *const obj)
{
    obj->priv_size = sizeof(M_PRIV);
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;
    obj->control_func = M_Control;
    obj->draw_func = nullptr;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_ELECTRICAL_LIGHT, M_Setup)
REGISTER_OBJECT(O_FLICKERING_LIGHT, M_Setup)
