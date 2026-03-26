#include <trx/game/items/draw.h>

#include <trx/game/items/utils.h>
#include <trx/game/random.h>
#include <trx/game/spawn.h>
#include <trx/version.h>

void Item_ControlDraw(ITEM *const item)
{
    if (g_TRVersion == 3 && item->status != IS_INVISIBLE
        && item->after_death < 32 && item->after_death > 0) {
        item->after_death++;

        if (!Item_ShouldSpawnBlood(item)) {
            return;
        }

        if (item->after_death == 2 || item->after_death == 5
            || item->after_death == 11 || item->after_death == 20
            || item->after_death == 27 || item->after_death == 32
            || (Random_GetDraw() & 7) == 0) {
            Spawn_BloodBathD(
                item->pos.x, item->pos.y - 64, item->pos.z, 0,
                Random_GetDraw() << 1, item->room_num, 1);
        }
    }
}
