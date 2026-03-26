#include <trx/game/objects/general/bridge_common.h>

#include <trx/config.h>
#include <trx/core/utils.h>
#include <trx/game/rooms.h>

bool Bridge_IsSameSector(
    const int32_t x, const int32_t z, const ITEM *const item)
{
    const int32_t sector_x = x / WALL_L;
    const int32_t sector_z = z / WALL_L;
    const int32_t item_sector_x = item->pos.x / WALL_L;
    const int32_t item_sector_z = item->pos.z / WALL_L;

    return sector_x == item_sector_x && sector_z == item_sector_z;
}

int32_t Bridge_GetOffset(
    const ITEM *const item, int32_t x, int32_t y, int32_t z)
{
    // Set the offset to the max value of 1023 if Lara is outside of the
    // bridge x/z position depending on its angle. This makes sure
    // the height is calculated properly for the front collision since
    // the low end of tilted bridges have a lower height.
    int32_t offset = 0;
    if (item->rot.y == 0) {
        if (g_Config.gameplay.fix_bridge_collision
            && x <= item->pos.x - WALL_L / 2) {
            offset = WALL_L - 1;
        } else {
            offset = (WALL_L - x) & (WALL_L - 1);
        }
    } else if (item->rot.y == -DEG_180) {
        if (g_Config.gameplay.fix_bridge_collision
            && x >= item->pos.x + WALL_L / 2) {
            offset = 0;
        } else {
            offset = x & (WALL_L - 1);
        }
    } else if (item->rot.y == DEG_90) {
        if (g_Config.gameplay.fix_bridge_collision
            && z >= item->pos.z + WALL_L / 2) {
            offset = WALL_L - 1;
        } else {
            offset = z & (WALL_L - 1);
        }
    } else {
        if (g_Config.gameplay.fix_bridge_collision
            && z <= item->pos.z - WALL_L / 2) {
            offset = 0;
        } else {
            offset = (WALL_L - z) & (WALL_L - 1);
        }
        // Fixes an edge case of an invisible wall on the tilt2 bridge floor.
        // The offset would get set to 0 on a specific z pos at the bottom of a
        // slope. The game would then set an invisible wall because it thought
        // Lara was at the high end of the tilt2 slope which is higher than a
        // step. This fix sets the offset to the max value (1023) when Lara's at
        // the bottom of the slope.
        if (g_Config.gameplay.fix_bridge_collision && offset == 0
            && y < item->pos.y) {
            offset = (WALL_L - 1 - z) & (WALL_L - 1);
        }
    }
    return offset;
}

void Bridge_FixEmbeddedPosition(int16_t item_num)
{
    // Some bridges at floor level are embedded into the floor.
    // This checks if bridges are below a room's floor level
    // and moves them up.
    ITEM *const item = Item_Get(item_num);

    const BOUNDS_16 *const bounds = Item_GetBoundsAccurate(item);
    const int16_t bridge_height = ABS(bounds->max.y) - ABS(bounds->min.y);

    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(
        (XYZ_32) { item->pos.x, item->pos.y - bridge_height, item->pos.z },
        &room_num);
    const int32_t floor_height = Room_GetHeight(sector, item->pos);

    // Only move the bridge up if it's at floor level and there
    // isn't a room portal below.
    if (item->pos.y != floor_height || sector->portal_room.pit != NO_ROOM) {
        return;
    }

    item->pos.y = floor_height - bridge_height;
}

void Bridge_AddWalkable(const int16_t item_num)
{
    const ITEM *const item = Item_Get(item_num);
    Walkable_Add(item_num, item->pos);
}
