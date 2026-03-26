#include <trx/game/inject.h>
#include <trx/game/items.h>
#include <trx/game/objects/common.h>

static bool M_TestItemMeta(
    const INJECTION_CONTEXT *const ctx, const INJECTION *const injection)
{
    const int32_t item_num = VFile_ReadS32(injection->fp);
    const INJECTION_OBJECT_INFO obj_info = Inject_ReadObjectPtr(injection);
    const XYZ_32 pos = {
        .x = VFile_ReadS32(injection->fp),
        .y = VFile_ReadS32(injection->fp),
        .z = VFile_ReadS32(injection->fp),
    };
    const int16_t room_num = VFile_ReadS16(injection->fp);
    const int16_t y_rot = VFile_ReadS16(injection->fp);

    if (item_num < 0 || item_num >= Item_GetTotalCount()) {
        return false;
    }

    const ITEM *const item = Item_Get(item_num);
    return item->object_id == obj_info.id
        && XYZ_32_AreEquivalent(item->pos, pos) && item->room_num == room_num
        && item->rot.y == y_rot;
}

REGISTER_INJECT_TESTER(ITT_ITEM_META, M_TestItemMeta)
