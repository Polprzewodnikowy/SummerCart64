#include <trx/config.h>
#include <trx/game/camera.h>
#include <trx/game/game_buf.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/objects/traps/common.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/game/spawn.h>
#include <trx/version.h>

#define M_DAMAGE_AIR 100
#define M_SHAKE_RANGE (WALL_L * 10) // = 10240

static void M_Roll(ITEM *const item)
{
    item->gravity = false;
    item->fall_speed = 0;
    item->pos.y = item->floor;

    if (g_TRVersion > 1) {
        if (item->object_id == O_ROLLING_BALL_1) {
            Sound_Effect(SFX_ROLLING_BALL_1_ROLL, &item->pos, SPM_NORMAL);
        } else if (item->object_id == O_ROLLING_BALL_2) {
            Sound_Effect(SFX_ROLLING_BALL_2_ROLL, &item->pos, SPM_NORMAL);
        } else if (item->object_id == O_ROLLING_BALL_3) {
            Sound_Effect(SFX_ROLLING_BALL_3_ROLL, &item->pos, SPM_NORMAL);
        } else if (item->object_id == O_ROLLING_BALL_4) {
            Sound_Effect(SFX_ROLLING_BALL_4_ROLL, &item->pos, SPM_NORMAL);
        }
    }

    if (g_Config.gameplay.enable_boulder_shake) {
        XYZ_32 mic_pos = g_Camera.mic_pos.pos;
        mic_pos.y = item->pos.y; // Ignore vertical component
        const int32_t dist = XYZ_32_GetDistance(mic_pos, item->pos);
        if (dist < M_SHAKE_RANGE) {
            g_Camera.bounce = 40 * (dist - M_SHAKE_RANGE) / M_SHAKE_RANGE;
        }
    }
}

static bool M_TestStop(const ITEM *const item)
{
    int32_t dist;
    switch (item->object_id) {
    case O_ROLLING_BALL_1:
        if (g_TRVersion == 1) {
            dist = WALL_L / 2;
        } else if (g_TRVersion == 2) {
            dist = STEP_L * 3 / 2;
        } else {
            dist = STEP_L * 5 / 4;
        }
        break;
    case O_ROLLING_BALL_4:
        dist = WALL_L * 17 / 16;
        break;
    default:
        dist = WALL_L;
        break;
    }

    const ANIM_FRAME *const frame = Item_GetBestFrame(item);
    const BOUNDS_16 *const bounds = &frame->bounds;
    int16_t item_height =
        ROUND_TO_HALF_CLICK(ABS(bounds->max.y - bounds->min.y));

    int16_t room_num = item->room_num;
    XYZ_32 sample_pos = XYZ_32_OffsetYaw(item->pos, item->rot.y, dist);
    const SECTOR *sector = Room_GetSector(sample_pos, &room_num);
    const int32_t height = Room_GetHeight(sector, sample_pos);
    if (item->object_id != O_ROLLING_BALL_4) {
        // Allow a more lenient height check if regular boulders fall onto
        // slopes with a ceiling in front.
        const int32_t height_scale = Room_GetHeightType() != HT_WALL ? 2 : 1;
        CLAMPG(item_height, STEP_L * 3 / height_scale);
    }

    sample_pos.y -= item_height;
    sector = Room_GetSector(sample_pos, &room_num);
    const int32_t ceiling = Room_GetCeiling(sector, sample_pos);

    return height < item->pos.y
        || (!item->gravity && ceiling > item->pos.y - item_height);
}

static void M_Stop(ITEM *const item, const XYZ_32 old_pos)
{
    if (item->object_id == O_ROLLING_BALL_1) {
        Sound_Effect(SFX_ROLLING_BALL_1_STOP, &item->pos, SPM_NORMAL);
        item->status = IS_DEACTIVATED;
    } else if (item->object_id == O_ROLLING_BALL_2) {
        Sound_Effect(SFX_ROLLING_BALL_2_STOP, &item->pos, SPM_NORMAL);
        item->goal_anim_state = TRAP_WORKING;
    } else if (item->object_id == O_ROLLING_BALL_3) {
        Sound_Effect(SFX_ROLLING_BALL_3_STOP, &item->pos, SPM_NORMAL);
        item->goal_anim_state = TRAP_WORKING;
    } else if (item->object_id == O_ROLLING_BALL_4) {
        Sound_Effect(SFX_ROLLING_BALL_4_STOP, &item->pos, SPM_NORMAL);
        item->status = IS_DEACTIVATED;
    }

    item->pos.x = old_pos.x;
    item->pos.y = item->floor;
    item->pos.z = old_pos.z;
    item->speed = 0;
    item->fall_speed = 0;
    item->touch_bits = 0;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    item->enable_interpolation = item->status == IS_ACTIVE;

    if (item->status == IS_DEACTIVATED && !Item_IsTriggerActive(item)) {
        Trap_Reset(item);
        return;
    }

    if (item->status != IS_ACTIVE) {
        int16_t room_num = item->room_num;
        const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
        const int32_t height = Room_GetHeight(sector, item->pos);
        if (item->floor < height) {
            item->status = IS_ACTIVE;
            item->floor = height;
        }
        return;
    }

    if (item->goal_anim_state == TRAP_WORKING) {
        Item_Animate(item);
        return;
    }

    if (item->pos.y < item->floor) {
        if (!item->gravity) {
            item->gravity = true;
            item->fall_speed = -10;
        }
    } else if (item->current_anim_state == TRAP_SET) {
        item->goal_anim_state = TRAP_ACTIVATE;
    }

    const XYZ_32 old_pos = item->pos;
    Item_Animate(item);

    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
    Item_UpdateRoom(item_num, room_num);

    item->floor = Room_GetHeight(sector, item->pos);
    Room_TestTriggers(item);

    if (item->pos.y >= item->floor - STEP_L) {
        M_Roll(item);
    }

    if (M_TestStop(item)) {
        M_Stop(item, old_pos);
    }
}

static void M_Collision(
    const int16_t item_num, ITEM *const lara_item, COLL_INFO *const coll)
{
    ITEM *const item = Item_Get(item_num);
    const LARA_INFO *const lara = Lara_GetLaraInfo();

    if (item->status != IS_ACTIVE) {
        if (item->status != IS_INVISIBLE) {
            Object_Collision(item_num, lara_item, coll);
        }
        return;
    }

    if (!Lara_TestBoundsCollide(item, coll->radius)) {
        return;
    }
    if (!Collide_TestCollision(item, lara_item)) {
        return;
    }

    if (lara_item->gravity || g_Config.debug.enable_invulnerability) {
        if (coll->enable_baddie_push) {
            Lara_Col_ItemPush(item, coll, coll->enable_hit, true);
        }
        if (!g_Config.debug.enable_invulnerability) {
            lara_item->hit_points -= M_DAMAGE_AIR;
        }

        // TODO: handle overflows
        const int32_t dx = lara_item->pos.x - item->pos.x;
        const int32_t dy =
            (lara_item->pos.y - 350) - (item->pos.y - WALL_L / 2);
        const int32_t dz = lara_item->pos.z - item->pos.z;
        int32_t dist = Math_Sqrt(SQUARE(dx) + SQUARE(dy) + SQUARE(dz));
        CLAMPL(dist, WALL_L / 2);

        Spawn_Blood(
            item->pos.x + (dx * WALL_L / 2) / dist,
            item->pos.y + (dy * WALL_L / 2) / dist - WALL_L / 2,
            item->pos.z + (dz * WALL_L / 2) / dist, item->speed, item->rot.y,
            item->room_num);
    } else {
        lara_item->hit_status = true;
        if (lara_item->hit_points > 0) {
            lara_item->hit_points = -1;
            Item_UpdateRoom(lara->item_num, item->room_num);

            lara_item->rot.x = 0;
            lara_item->rot.y = item->rot.y;
            lara_item->rot.z = 0;

            Item_SwitchToAnim(lara_item, LA(LA_BOULDER_DEATH), 0);
            lara_item->goal_anim_state =
                Item_GetAnim(lara_item)->current_anim_state;
            lara_item->current_anim_state = lara_item->goal_anim_state;

            g_Camera.flags = CF_FOLLOW_CENTRE;
            g_Camera.target_angle = 170 * DEG_1;
            g_Camera.target_elevation = -25 * DEG_1;

            for (int32_t i = 0; i < 15; i++) {
                Spawn_Blood(
                    lara_item->pos.x + (Random_GetControl() - 0x4000) / 256,
                    lara_item->pos.y - Random_GetControl() / 64,
                    lara_item->pos.z + (Random_GetControl() - 0x4000) / 256,
                    2 * item->speed,
                    item->rot.y + (Random_GetControl() - 0x4000) / 8,
                    item->room_num);
            }
        }
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = Trap_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = M_Collision;
    obj->save_position = true;
    obj->save_flags = true;
    obj->save_anim = true;
    obj->load_floor = true;
}

REGISTER_OBJECT(O_ROLLING_BALL_1, M_Setup)
REGISTER_OBJECT(O_ROLLING_BALL_2, M_Setup)
REGISTER_OBJECT(O_ROLLING_BALL_3, M_Setup)
REGISTER_OBJECT(O_ROLLING_BALL_4, M_Setup)
