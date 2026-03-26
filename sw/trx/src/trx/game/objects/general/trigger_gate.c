#include <trx/config.h>
#include <trx/game/matrix.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/rooms.h>

// clang-format off
#define M_COLOR_ON  ((RGBA_8888) { 0, 255, 0, 255 })
#define M_COLOR_OFF ((RGBA_8888) { 0, 255, 255, 255 })
#define M_RADIUS    (STEP_L * 3 / 8)
// clang-format on

static void M_UpdateTrigger(const ITEM *const item, bool enabled)
{
    int16_t room_num = item->room_num;
    SECTOR *const sector = Room_GetSector(item->pos, &room_num);
    if (sector->trigger != nullptr) {
        sector->trigger->enabled = enabled;
    }
}

static void M_Initialise(const int16_t item_num)
{
    const ITEM *const item = Item_Get(item_num);
    const bool enabled = Item_IsTriggerActiveRO(item);
    M_UpdateTrigger(item, enabled);
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    const bool enabled = Item_IsTriggerActive(item);
    M_UpdateTrigger(item, enabled);
}

static bool M_Draw(const ITEM *const item)
{
    if (!g_Config.debug.enable_debug_triggers) {
        return false;
    }

    const RGBA_8888 color =
        Item_IsTriggerActiveRO(item) ? M_COLOR_ON : M_COLOR_OFF;

    Matrix_Push();
    Matrix_TranslateAbs32(item->pos);
    Output_DrawSphereEx((XYZ_16) { 0, -M_RADIUS, 0 }, M_RADIUS, color);
    Matrix_Pop();

    return true;
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->draw_func = M_Draw;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_TRIGGER_GATE, M_Setup)
