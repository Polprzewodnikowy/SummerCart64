#include <trx/game/rooms/floor_data.h>

#include <trx/config.h>
#include <trx/game/camera.h>
#include <trx/game/game.h>
#include <trx/game/game_buf.h>
#include <trx/game/gym.h>
#include <trx/game/items.h>
#include <trx/game/lara.h>
#include <trx/game/level/settings.h>
#include <trx/game/music.h>
#include <trx/game/objects/general/keyhole.h>
#include <trx/game/objects/general/pickup.h>
#include <trx/game/objects/general/shoal.h>
#include <trx/game/objects/general/switch.h>
#include <trx/game/pathing.h>
#include <trx/game/rooms.h>
#include <trx/game/stats.h>
#include <trx/version.h>

#define M_NULL_INDEX 0
#define M_IS_DONE(t) ((t & 0x8000) == 0x8000)

#define M_ENTRY_TYPE(t) (t & 0x1F)
#define M_TRIG_TYPE(t) ((t & 0x7F00) >> 8)
#define M_TRIG_TIMER(t) (t & 0xFF)
#define M_TRIG_ONE_SHOT(t) ((t & 0x100) == 0x100)
#define M_TRIG_MASK(t) (t & 0x3E00)
#define M_TRIG_CMD_TYPE(t) ((t & 0x7C00) >> 10)
#define M_TRIG_CMD_ARG(t) (t & 0x3FF)
#define M_TRIG_CAM_GLIDE(t) ((t & 0x3E00) >> 6)
#define M_LADDER_TYPE(t) ((t & 0x7F00) >> 8)

static bool M_IsSpeechTrack(const MUSIC_ID track_id)
{
    switch (Music_FromGameID(track_id)) {
    case MX_BALDY_SPEECH:
    case MX_COWBOY_SPEECH:
    case MX_LARSON_SPEECH:
    case MX_NATLA_SPEECH:
    case MX_PIERRE_SPEECH:
    case MX_SKATEKID_SPEECH:
        return true;
    default:
        return false;
    }
}

static const int16_t *M_ReadTrigger(
    const int16_t *data, const int16_t fd_entry, SECTOR *const sector)
{
    TRIGGER *const trigger = GameBuf_Alloc(sizeof(TRIGGER), GBUF_FLOOR_DATA);

    const int16_t trig_setup = *data++;
    trigger->enabled = true;
    trigger->type = M_TRIG_TYPE(fd_entry);
    trigger->timer = M_TRIG_TIMER(trig_setup);
    trigger->one_shot = M_TRIG_ONE_SHOT(trig_setup);
    trigger->mask = M_TRIG_MASK(trig_setup);
    trigger->item_index = NO_ITEM;

    if (trigger->type == TT_SWITCH || trigger->type == TT_KEY
        || trigger->type == TT_PICKUP) {
        const int16_t item_data = *data++;
        trigger->item_index = M_TRIG_CMD_ARG(item_data);
        if (M_IS_DONE(item_data)) {
            return data;
        }
    }

    TRIGGER_CMD *cmd;
    if (sector->trigger == nullptr) {
        sector->trigger = trigger;
        sector->trigger->command =
            GameBuf_Alloc(sizeof(TRIGGER_CMD), GBUF_FLOOR_DATA);
        cmd = sector->trigger->command;
    } else {
        // Some old TRLEs have incorrectly formatted floor data, with multiple
        // trigger entries defined where regular triggers overlap dummies. In
        // this case we link the new commands onto the old.
        cmd = sector->trigger->command;
        while (cmd->next_cmd != nullptr) {
            cmd = cmd->next_cmd;
        }
        cmd->next_cmd = GameBuf_Alloc(sizeof(TRIGGER_CMD), GBUF_FLOOR_DATA);
        cmd = cmd->next_cmd;
    }

    while (true) {
        int16_t command = *data++;
        cmd->type = M_TRIG_CMD_TYPE(command);

        if (cmd->type == TO_CAMERA) {
            TRIGGER_CAMERA_DATA *const cam_data =
                GameBuf_Alloc(sizeof(TRIGGER_CAMERA_DATA), GBUF_FLOOR_DATA);
            cmd->parameter = (void *)cam_data;
            cam_data->camera_num = M_TRIG_CMD_ARG(command);

            command = *data++;
            cam_data->timer = M_TRIG_TIMER(command);
            cam_data->glide = M_TRIG_CAM_GLIDE(command);
            cam_data->one_shot = M_TRIG_ONE_SHOT(command);
        } else {
            cmd->parameter = (void *)(intptr_t)M_TRIG_CMD_ARG(command);
        }

        if (M_IS_DONE(command)) {
            cmd->next_cmd = nullptr;
            break;
        }

        cmd->next_cmd = GameBuf_Alloc(sizeof(TRIGGER_CMD), GBUF_FLOOR_DATA);
        cmd = cmd->next_cmd;
    }

    return data;
}

static bool M_TestLava(const ITEM *const item)
{
    const LARA_INFO *const lara_info = Lara_GetLaraInfo();
    if (item->hit_points < 0 || lara_info->water_status == LWS_CHEAT
        || (lara_info->water_status == LWS_ABOVE_WATER
            && item->pos.y != item->floor)) {
        return false;
    }

    // OG fix: check if floor index has lava
    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(
        (XYZ_32) { item->pos.x, MAX_HEIGHT, item->pos.z }, &room_num);
    return sector->is_death_sector;
}

static void M_TriggerMusicTrack(MUSIC_ID track_id, const TRIGGER *const trigger)
{
    if (track_id == (MUSIC_ID)0
        && (trigger->type == TT_ANTIPAD || trigger->type == TT_ANTITRIGGER)) {
        Music_Stop();
        return;
    }

    if (track_id <= Music_ToGameID(MX_UNUSED_1) || track_id >= MAX_MUSIC_TRACKS
        || (Game_IsInGym() && !Gym_CanPlayMusicTrack(&track_id))) {
        return;
    }

    uint16_t flags = Music_GetTrackFlags(track_id);
    MUSIC_PLAY_MODE play_mode = MPM_NO_REPEAT;
    if (g_Config.audio.fix_speeches_killing_music
        && M_IsSpeechTrack(track_id)) {
        play_mode = MPM_OVERLAY;
    }

    // TODO: consolidate
    if (g_TRVersion == 1) {
        if ((flags & IF_ONE_SHOT) != 0) {
            return;
        }

        if (trigger->type == TT_SWITCH) {
            flags ^= trigger->mask;
        } else if (
            trigger->type == TT_ANTIPAD || trigger->type == TT_ANTITRIGGER) {
            flags &= -1 - trigger->mask;
        } else if (trigger->mask) {
            flags |= trigger->mask;
        }

        if ((flags & IF_CODE_BITS) == IF_CODE_BITS) {
            if (trigger->one_shot) {
                flags |= IF_ONE_SHOT;
            }
            Music_Play_Direct(track_id, play_mode);
        } else {
            Music_StopTrack_Direct(track_id);
        }
    } else {
        if (trigger->type != TT_SWITCH) {
            const int32_t code = trigger->mask;
            if ((flags & code) != 0) {
                return;
            }
            if (trigger->one_shot) {
                flags |= code;
            }
        }

        if (trigger->timer == 0 || g_TRVersion != 2) {
            Music_Play_Direct(track_id, play_mode);
            goto finish;
        }

        if (track_id != Music_GetDelayedTrack()) {
            Music_Play_Direct(track_id, MPM_DELAY);
            flags = (flags & 0xFF00) | ((LOGIC_FPS * trigger->timer) & 0xFF);
            goto finish;
        }

        int32_t timer = flags & 0xFF;
        if (timer == 0) {
            goto finish;
        }

        timer--;
        if (timer == 0) {
            Music_Play_Direct(track_id, play_mode);
        }
        flags = (flags & 0xFF00) | (timer & 0xFF);
    }

finish:
    Music_SetTrackFlags(track_id, flags);
}

void Room_ParseFloorData(const int16_t *floor_data)
{
    for (int32_t i = 0; i < Room_GetCount(); i++) {
        const ROOM *const room = Room_Get(i);
        for (int32_t j = 0; j < room->size.x * room->size.z; j++) {
            SECTOR *const sector = &room->sectors[j];
            Room_PopulateSectorData(
                sector, floor_data, sector->idx, M_NULL_INDEX);
        }
    }
}

void Room_PopulateSectorData(
    SECTOR *const sector, const int16_t *floor_data, const uint16_t start_index,
    const uint16_t null_index)
{
    sector->floor.type = SURFACE_FLOOR;
    sector->ceiling.type = SURFACE_CEILING;
    sector->floor.tilt = 0;
    sector->ceiling.tilt = 0;
    sector->floor.split.type = SPLIT_NONE;
    sector->ceiling.split.type = SPLIT_NONE;
    sector->floor.is_split = false;
    sector->ceiling.is_split = false;
    sector->portal_room.wall = NO_ROOM;
    sector->is_death_sector = false;
    sector->trigger = nullptr;
    sector->ladder = LADDER_NONE;

    if (start_index == null_index || floor_data == nullptr) {
        return;
    }

    const int16_t *data = &floor_data[start_index];
    int16_t fd_entry;
    do {
        fd_entry = *data++;

        switch (M_ENTRY_TYPE(fd_entry)) {
        case FT_TILT:
            sector->floor.tilt = *data++;
            break;

        case FT_ROOF:
            sector->ceiling.tilt = *data++;
            break;

        case FT_DOOR:
            const int16_t portal_room = *data++;
            if (sector->portal_room.wall == NO_ROOM) {
                sector->portal_room.wall = portal_room;
            }
            break;

        case FT_LAVA:
            sector->is_death_sector = true;
            break;

        case FT_TRIGGER:
            data = M_ReadTrigger(data, fd_entry, sector);
            break;

        case FT_CLIMB:
            sector->ladder |= (LADDER_DIRECTION)M_LADDER_TYPE(fd_entry);
            break;

        case FT_MONKEY:
            sector->ladder |= LADDER_CEILING;
            break;

        case FT_FLOOR_NWSE_SOLID:
        case FT_FLOOR_NESW_SOLID:
        case FT_FLOOR_NWSE_PORTAL_SW:
        case FT_FLOOR_NWSE_PORTAL_NE:
        case FT_FLOOR_NESW_PORTAL_SE:
        case FT_FLOOR_NESW_PORTAL_NW:
            Room_ReadTriangulation(&sector->floor, fd_entry, *data++);
            break;

        case FT_ROOF_NWSE_SOLID:
        case FT_ROOF_NESW_SOLID:
        case FT_ROOF_NWSE_PORTAL_SW:
        case FT_ROOF_NWSE_PORTAL_NE:
        case FT_ROOF_NESW_PORTAL_NW:
        case FT_ROOF_NESW_PORTAL_SE:
            Room_ReadTriangulation(&sector->ceiling, fd_entry, *data++);
            break;

        default:
            break;
        }
    } while (!M_IS_DONE(fd_entry));
}

void Room_ReadTriangulation(
    SURFACE *const surface, const int16_t func_data, const int16_t tilt_data)
{
    switch (M_ENTRY_TYPE(func_data)) {
    case FT_FLOOR_NWSE_SOLID:
    case FT_ROOF_NWSE_SOLID:
        surface->split.type = SPLIT_NWSE_SOLID;
        break;
    case FT_FLOOR_NESW_SOLID:
    case FT_ROOF_NESW_SOLID:
        surface->split.type = SPLIT_NESW_SOLID;
        break;
    case FT_FLOOR_NWSE_PORTAL_SW:
    case FT_ROOF_NWSE_PORTAL_SW:
        surface->split.type = SPLIT_NWSE_PORTAL_SW;
        break;
    case FT_FLOOR_NWSE_PORTAL_NE:
    case FT_ROOF_NWSE_PORTAL_NE:
        surface->split.type = SPLIT_NWSE_PORTAL_NE;
        break;
    case FT_FLOOR_NESW_PORTAL_SE:
    case FT_ROOF_NESW_PORTAL_SE:
        surface->split.type = SPLIT_NESW_PORTAL_SE;
        break;
    case FT_FLOOR_NESW_PORTAL_NW:
    case FT_ROOF_NESW_PORTAL_NW:
        surface->split.type = SPLIT_NESW_PORTAL_NW;
        break;
    default:
        return;
    }

    surface->is_split = true;
    surface->split.h1 = (func_data & 0x03E0) >> 5;
    surface->split.h2 = (func_data & 0x7C00) >> 10;
    if ((surface->split.h1 & 0x10) != 0) {
        surface->split.h1 |= 0xFFF0;
    }
    if ((surface->split.h2 & 0x10) != 0) {
        surface->split.h2 |= 0xFFF0;
    }
    surface->split.h1 <<= 8;
    surface->split.h2 <<= 8;

    for (int32_t i = 0; i < 4; i++) {
        surface->split.tilts[i] = (tilt_data >> (i * 4)) & 0xF;
        if (surface->type == SURFACE_CEILING) {
            surface->split.tilts[i] *= -1;
        }
    }
}

bool Room_TestTriggers(const ITEM *const item)
{
    int16_t room_num = item->room_num;
    const SECTOR *sector = Room_GetSector(
        (XYZ_32) { item->pos.x, MAX_HEIGHT, item->pos.z }, &room_num);

    bool result = Room_TestSectorTrigger(item, sector);
    if (item->object_id != O_TORSO) {
        return result;
    }

    for (int32_t dx = -1; dx < 2; dx++) {
        for (int32_t dz = -1; dz < 2; dz++) {
            if (dx == 0 && dz == 0) {
                continue;
            }

            room_num = item->room_num;
            sector = Room_GetSector(
                (XYZ_32) {
                    item->pos.x + dx * WALL_L,
                    MAX_HEIGHT,
                    item->pos.z + dz * WALL_L,
                },
                &room_num);
            result |= Room_TestSectorTrigger(item, sector);
        }
    }

    return result;
}

bool Room_TestSectorTrigger(const ITEM *const item, const SECTOR *const sector)
{
    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    const bool is_heavy = item->object_id != O_LARA;
    if (!is_heavy) {
        if (sector->is_death_sector && M_TestLava(item)) {
            Lara_TouchDeathSector(Level_GetDeathTile());
        }

        const LADDER_DIRECTION direction = 1 << Math_GetDirection(item->rot.y);
        lara_info->climb_status = (sector->ladder & direction) == direction;
    }

    const TRIGGER *const trigger = sector->trigger;
    if (trigger == nullptr || !trigger->enabled) {
        return false;
    }

    if (g_Camera.type != CAM_HEAVY) {
        Camera_RefreshFromTrigger(trigger);
    }

    ITEM *camera_item = nullptr;
    bool switch_off = false;
    bool flip_map = false;
    bool flip_available = false;
    int32_t new_effect = -1;
    const bool flip_status = Room_GetFlipStatus();

    if (is_heavy) {
        if (trigger->type != TT_HEAVY) {
            return false;
        }
    } else {
        switch (trigger->type) {
        case TT_PAD:
        case TT_ANTIPAD:
            if (!Gym_TrackManager_OnPadContact(GYM_TRACK_ASSAULT, false)) {
                return false;
            }
            if (item->pos.y != item->floor) {
                return false;
            }
            if (item->object_id == O_LARA
                && !Gym_TrackManager_OnPadContact(GYM_TRACK_ASSAULT, true)) {
                return false;
            }
            break;

        case TT_SWITCH: {
            if (!Switch_Trigger(trigger->item_index, trigger->timer)) {
                return false;
            }
            const ITEM *const switch_item = Item_Get(trigger->item_index);
            switch_off = switch_item->current_anim_state == SWITCH_STATE_OFF;
            break;
        }

        case TT_KEY: {
            if (!Keyhole_Trigger(trigger->item_index)) {
                return false;
            }
            break;
        }

        case TT_PICKUP: {
            if (!Pickup_Trigger(trigger->item_index)) {
                return false;
            }
            break;
        }

        case TT_HEAVY:
        case TT_DUMMY:
            return false;

        case TT_COMBAT:
            if (lara_info->gun_status != LGS_READY) {
                return false;
            }
            break;

        default:
            break;
        }
    }

    const TRIGGER_CMD *cmd = trigger->command;
    for (; cmd != nullptr; cmd = cmd->next_cmd) {
        switch (cmd->type) {
        case TO_OBJECT: {
            const int16_t item_num = (int16_t)(intptr_t)cmd->parameter;
            ITEM *const trig_item = Item_Get(item_num);

            bool one_shot = false;
            if (g_TRVersion == 3) {
                switch (trigger->type) {
                case TT_SWITCH:
                    one_shot = trig_item->flags & IF_ONE_SHOT_SWITCH;
                    break;
                case TT_ANTIPAD:
                case TT_ANTITRIGGER:
                    one_shot = trig_item->flags & IF_ONE_SHOT_ANTITRIGGER;
                    break;
                default:
                    one_shot = trig_item->flags & IF_ONE_SHOT;
                    break;
                }
            } else {
                one_shot = trig_item->flags & IF_ONE_SHOT;
            }
            if (one_shot) {
                break;
            }

            const OBJECT *const obj = Object_Get(trig_item->object_id);

            const bool is_shoal_object =
                Object_IsType(trig_item->object_id, g_ShoalObjects);

            if (is_shoal_object && trigger->type != TT_ANTIPAD
                && trigger->type != TT_ANTITRIGGER) {
                Shoal_TriggerActivate(trig_item, trigger->timer);
            } else {
                trig_item->timer = trigger->timer;
                if (trig_item->timer != 1) {
                    trig_item->timer *= LOGIC_FPS;
                }

                if (obj->trigger_func != nullptr) {
                    const bool use_default_handling =
                        obj->trigger_func(trig_item, trigger);
                    if (!use_default_handling) {
                        break;
                    }
                }
            }

            if (trigger->type == TT_SWITCH) {
                trig_item->flags ^= trigger->mask;
                if (trigger->one_shot && g_TRVersion == 3) {
                    trig_item->flags |= IF_ONE_SHOT_SWITCH;
                }
            } else if (
                trigger->type == TT_ANTIPAD
                || trigger->type == TT_ANTITRIGGER) {
                if (is_shoal_object) {
                    Shoal_TriggerDeactivate(trig_item);
                }

                // TODO investigate unifying as ~(trigger->mask | IF_REVERSE)
                if (g_TRVersion >= 3) {
                    trig_item->flags &= ~(IF_CODE_BITS | IF_REVERSE);
                } else {
                    trig_item->flags &= ~trigger->mask;
                }
                if (trigger->one_shot) {
                    if (g_TRVersion == 3) {
                        trig_item->flags |= IF_ONE_SHOT_ANTITRIGGER;
                    } else {
                        trig_item->flags |= IF_ONE_SHOT;
                    }
                }
            } else {
                trig_item->flags |= trigger->mask;
            }

            if ((trig_item->flags & IF_CODE_BITS) != IF_CODE_BITS) {
                break;
            }

            if (trigger->one_shot) {
                trig_item->flags |= IF_ONE_SHOT;
            }

            if (trig_item->active) {
                break;
            }

            if (obj->activate_func != nullptr) {
                obj->activate_func(trig_item);
            } else if (obj->intelligent) {
                if (trig_item->status == IS_INACTIVE) {
                    trig_item->touch_bits = 0;
                    trig_item->status = IS_ACTIVE;
                    Item_AddActive(item_num);
                    LOT_EnableBaddieAI(item_num, true);
                } else if (trig_item->status == IS_INVISIBLE) {
                    trig_item->touch_bits = 0;
                    if (LOT_EnableBaddieAI(item_num, false)) {
                        trig_item->status = IS_ACTIVE;
                    } else {
                        trig_item->status = IS_INVISIBLE;
                    }
                    Item_AddActive(item_num);
                }
            } else {
                trig_item->touch_bits = 0;
                trig_item->status = IS_ACTIVE;
                Item_AddActive(item_num);
            }

            break;
        }

        case TO_CAMERA: {
            const TRIGGER_CAMERA_DATA *const cam_data =
                (TRIGGER_CAMERA_DATA *)cmd->parameter;
            OBJECT_VECTOR *const camera =
                Camera_GetFixedObject(cam_data->camera_num);
            if ((camera->flags & IF_ONE_SHOT) != 0) {
                break;
            }

            g_Camera.num = cam_data->camera_num;

            if ((g_Camera.type == CAM_LOOK || g_Camera.type == CAM_COMBAT)
                && !Camera_IsLocked(g_Camera.num)) {
                break;
            }

            if (trigger->type == TT_COMBAT) {
                break;
            }

            if (trigger->type == TT_SWITCH && trigger->timer != 0
                && switch_off) {
                break;
            }

            if (g_Camera.num == g_Camera.last && trigger->type != TT_SWITCH) {
                break;
            }

            g_Camera.timer = LOGIC_FPS * cam_data->timer;

            if (cam_data->one_shot) {
                camera->flags |= IF_ONE_SHOT;
            }

            g_Camera.speed = 1;
            if (g_Config.visuals.enable_glide_cameras) {
                g_Camera.speed += cam_data->glide;
            }
            g_Camera.type = is_heavy ? CAM_HEAVY : CAM_FIXED;
            break;
        }

        case TO_SINK: {
            if (g_TRVersion == 3) {
                lara_info->current.active =
                    1 + (int16_t)(intptr_t)cmd->parameter;
            } else {
                const OBJECT_VECTOR *const sink =
                    Camera_GetFixedObject((int16_t)(intptr_t)cmd->parameter);

                if (g_TRVersion == 2
                    || lara_info->lot.required_box != sink->flags) {
                    lara_info->lot.target = sink->pos;
                    lara_info->lot.required_box = sink->flags;
                }
                lara_info->current.active = sink->data * 6;
            }
            break;
        }

        case TO_FLIPMAP: {
            const int16_t flip_slot = (int16_t)(intptr_t)cmd->parameter;
            int32_t slot_flags = Room_GetFlipSlotFlags(flip_slot);
            flip_available = true;

            if (slot_flags & IF_ONE_SHOT) {
                break;
            }

            if (trigger->type == TT_SWITCH) {
                slot_flags ^= trigger->mask;
            } else {
                slot_flags |= trigger->mask;
            }

            if ((slot_flags & IF_CODE_BITS) == IF_CODE_BITS) {
                if (trigger->one_shot) {
                    slot_flags |= IF_ONE_SHOT;
                }

                if (!flip_status) {
                    flip_map = true;
                }
            } else if (flip_status) {
                flip_map = true;
            }

            Room_SetFlipSlotFlags(flip_slot, slot_flags);
            break;
        }

        case TO_FLIPON: {
            const int16_t flip_slot = (int16_t)(intptr_t)cmd->parameter;
            const int32_t slot_flags = Room_GetFlipSlotFlags(flip_slot);
            flip_available = true;

            if ((slot_flags & IF_CODE_BITS) == IF_CODE_BITS && !flip_status) {
                flip_map = true;
            }
            break;
        }

        case TO_FLIPOFF: {
            const int16_t flip_slot = (int16_t)(intptr_t)cmd->parameter;
            const int32_t slot_flags = Room_GetFlipSlotFlags(flip_slot);
            flip_available = true;

            if ((slot_flags & IF_CODE_BITS) == IF_CODE_BITS && flip_status) {
                flip_map = true;
            }
            break;
        }

        case TO_TARGET: {
            const int16_t target_num = (int16_t)(intptr_t)cmd->parameter;
            camera_item = Item_Get(target_num);
            break;
        }

        case TO_FINISH:
            Game_SetIsLevelComplete(true);
            break;

        case TO_FLIPEFFECT:
            new_effect = (int16_t)(intptr_t)cmd->parameter;
            break;

        case TO_CD:
            M_TriggerMusicTrack((MUSIC_ID)(intptr_t)cmd->parameter, trigger);
            break;

        case TO_SECRET: {
            const int16_t secret_num = (int16_t)(intptr_t)cmd->parameter;
            if (Stats_AddSecret(secret_num)) {
                const MUSIC_PLAY_MODE mode =
                    g_Config.audio.fix_secrets_killing_music ? MPM_OVERLAY
                                                             : MPM_ONCE;
                Music_Play(MX_SECRET, mode);
            }
            break;
        }

        case TO_BODY_BAG:
            Item_ClearKilled();
            break;

        default:
            break;
        }
    }

    if (camera_item != nullptr
        && (g_Camera.type == CAM_FIXED || g_Camera.type == CAM_HEAVY)) {
        g_Camera.item = camera_item;
    }

    if (flip_map) {
        Room_FlipMap();
    }

    if (new_effect != -1 && (flip_map || !flip_available)) {
        Room_SetFlipEffect(new_effect);
        Room_SetFlipTimer(0);
    }

    return true;
}
