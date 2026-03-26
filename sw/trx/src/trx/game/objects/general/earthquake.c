#include <trx/core/utils.h>
#include <trx/game/camera.h>
#include <trx/game/objects.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/version.h>

typedef struct {
    int32_t shake_intensity;
    int32_t target_intensity;
    int32_t target_timer;
} M_PRIV;

static void M_ActivateRelatedItem(ITEM *const earth_item)
{
    Item_AddActive(Item_GetIndex(earth_item));
    earth_item->status = IS_ACTIVE;
    earth_item->flags = IF_CODE_BITS;
    earth_item->timer = 0;
}

static void M_FindAndActivateRelatedItems(const ITEM *const item)
{
    OBJECT_ID object_id_to_activate = NO_OBJECT;
    const int32_t random = Random_GetControl();
    if (random < 512) {
        object_id_to_activate = O_FLAME_EMITTER;
    } else if (random < 1024) {
        object_id_to_activate = O_FALLING_CEILING_1;
    }
    if (object_id_to_activate == NO_OBJECT
        || !Object_Get(object_id_to_activate)->loaded) {
        return;
    }

    int16_t related_item_num = Room_Get(item->room_num)->item_num;
    while (related_item_num != NO_ITEM) {
        ITEM *const earth_item = Item_Get(related_item_num);
        if (earth_item->object_id == object_id_to_activate
            && earth_item->status != IS_ACTIVE
            && earth_item->status != IS_DEACTIVATED) {
            M_ActivateRelatedItem(earth_item);
            break;
        }
        related_item_num = earth_item->next_item;
    }
}

static void M_Reset(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    p->shake_intensity = 0;
    p->target_intensity = 0;
    p->target_timer = 0;
    item->status = IS_INACTIVE;
    Item_RemoveActive(item_num);
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;

    if (!Item_IsTriggerActive(item)) {
        M_Reset(item_num);
        return;
    }

    switch (g_TRVersion) {
    case 1:
        if (Random_GetDraw() < 256) {
            g_Camera.bounce = -150;
            Sound_Effect(SFX_EARTHQUAKE_1, nullptr, SPM_NORMAL);
        } else if (Random_GetControl() < 1024) {
            g_Camera.bounce = 50;
            Sound_Effect(SFX_EARTHQUAKE_2, nullptr, SPM_NORMAL);
        }
        break;

    case 2:
        if (Random_GetDraw() < 512) {
            Sound_Effect(SFX_EARTHQUAKE_1, nullptr, SPM_NORMAL);
            g_Camera.bounce = -200;
        }
        break;

    case 3: {
        if (p->target_intensity == 0) {
            p->target_intensity = 100;
        }

        if (p->target_timer == 0
            && ABS(p->shake_intensity - p->target_intensity) < 16) {
            if (p->target_intensity == 20) {
                p->target_intensity = 100;
                p->target_timer = (Random_GetControl() & 0x7F) + 90;
            } else {
                p->target_intensity = 20;
                p->target_timer = (Random_GetControl() & 0x7F) + 30;
            }
        }

        if (p->target_timer != 0) {
            p->target_timer--;
        }

        if (p->shake_intensity > p->target_intensity) {
            p->shake_intensity -= (Random_GetControl() & 7) + 2;
        } else {
            p->shake_intensity += (Random_GetControl() & 7) + 2;
        }

        Sound_Effect(
            SFX_EARTHQUAKE_LOOP, nullptr,
            ((p->shake_intensity << 16) + 0x1000000) | SPM_PITCH);
        g_Camera.bounce = -p->shake_intensity;
        break;
    }
    }

    M_FindAndActivateRelatedItems(item);
}

static void M_Setup(OBJECT *const obj)
{
    obj->priv_size = sizeof(M_PRIV);
    obj->control_func = M_Control;
    obj->draw_func = nullptr;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_EARTHQUAKE, M_Setup)
