#include <trx/game/inject.h>
#include <trx/game/rooms.h>

static bool M_TestRoomCount(
    const INJECTION_CONTEXT *const ctx, const INJECTION *const injection)
{
    const int32_t num_rooms = VFile_ReadS32(injection->fp);
    return num_rooms == Room_GetCount();
}

static bool M_TestRoomMeta(
    const INJECTION_CONTEXT *const ctx, const INJECTION *const injection)
{
    const int32_t room_num = VFile_ReadS32(injection->fp);
    const int32_t x_pos = VFile_ReadS32(injection->fp);
    const int32_t z_pos = VFile_ReadS32(injection->fp);
    const int32_t min_floor = VFile_ReadS32(injection->fp);
    const int32_t max_ceiling = VFile_ReadS32(injection->fp);
    const uint16_t x_size = VFile_ReadU16(injection->fp);
    const uint16_t z_size = VFile_ReadU16(injection->fp);

    if (room_num < 0 || room_num >= Room_GetCount()) {
        return false;
    }

    const ROOM *const room = Room_Get(room_num);
    return room->pos.x == x_pos && room->pos.z == z_pos
        && room->min_floor == min_floor && room->max_ceiling == max_ceiling
        && room->size.x == x_size && room->size.z == z_size;
}

REGISTER_INJECT_TESTER(ITT_ROOM_COUNT, M_TestRoomCount)
REGISTER_INJECT_TESTER(ITT_ROOM_META, M_TestRoomMeta)
