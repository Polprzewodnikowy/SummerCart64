#include <trx/game/camera.h>
#include <trx/game/music.h>
#include <trx/game/objects.h>
#include <trx/game/rooms.h>

#define GONG_BONGER_STRIKE_FRAME 41
#define GONG_BONGER_END_FRAME 79

static void M_ActivateHeavyTriggers(const int16_t item_num)
{
    const ITEM *const item = Item_Get(item_num);
    Room_TestTriggers(item);
    Item_Kill(item_num);
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    Item_Animate(item);
    if (Item_TestFrameEqual(item, GONG_BONGER_STRIKE_FRAME)) {
        Music_Play(MX_REVEAL_1, MPM_ONCE);
        g_Camera.bounce -= 50;
    }

    if (Item_TestFrameEqual(item, GONG_BONGER_END_FRAME)) {
        M_ActivateHeavyTriggers(item_num);
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_GONG_BONGER, M_Setup)
