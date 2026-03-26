#include <trx/game/objects/creatures/xian_common.h>

#include <trx/game/objects/common.h>
#include <trx/game/objects/draw.h>

bool XianWarrior_Draw(const ITEM *item)
{
    const OBJECT *swap;
    if (item->object_id == O_XIAN_SPEARMAN) {
        swap = Object_Get(O_XIAN_SPEARMAN_STATUE);
    } else {
        swap = Object_Get(O_XIAN_KNIGHT_STATUE);
    }
    return Object_DrawAnimatingItemWithSwap(item, swap);
}
