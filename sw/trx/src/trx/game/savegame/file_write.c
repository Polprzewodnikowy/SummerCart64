#include <trx/config.h>
#include <trx/core/json/util/write_io.h>
#include <trx/debug.h>
#include <trx/game/camera.h>
#include <trx/game/effects.h>
#include <trx/game/fx/explosion_ring.h>
#include <trx/game/fx/weather.h>
#include <trx/game/game.h>
#include <trx/game/inventory.h>
#include <trx/game/items.h>
#include <trx/game/items/carrier.h>
#include <trx/game/lara.h>
#include <trx/game/music.h>
#include <trx/game/objects.h>
#include <trx/game/objects/general/flare_item.h>
#include <trx/game/output.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/savegame.h>
#include <trx/game/savegame/file.h>
#include <trx/version.h>

typedef struct {
    int16_t count;
    int16_t id_map[MAX_EFFECTS];
} M_FX_ORDER;

static void M_WriteXYZ32(
    JSON_WRITE_IO *const io, const char *const key, const XYZ_32 source)
{
    JSONW_PUSH_OBJECT(io);
    JSONW_WRITE(io, "x", source.x);
    JSONW_WRITE(io, "y", source.y);
    JSONW_WRITE(io, "z", source.z);
    JSONW_POP_AND_SET(io, key);
}

static void M_WriteXYZ16(
    JSON_WRITE_IO *const io, const char *const key, const XYZ_16 source)
{
    JSONW_PUSH_OBJECT(io);
    JSONW_WRITE(io, "x", source.x);
    JSONW_WRITE(io, "y", source.y);
    JSONW_WRITE(io, "z", source.z);
    JSONW_POP_AND_SET(io, key);
}

static void M_GetFXOrder(M_FX_ORDER *const order)
{
    order->count = 0;
    for (int32_t i = 0; i < MAX_EFFECTS; i++) {
        order->id_map[i] = -1;
    }

    for (int16_t link_num = Effect_GetActiveNum(); link_num != NO_ITEM;
         link_num = Effect_Get(link_num)->next_active) {
        order->id_map[link_num] = order->count;
        order->count++;
    }
}

static void M_WriteItem(
    JSON_WRITE_IO *const io, const ITEM *const item,
    const M_FX_ORDER *const fx_order)
{
    const OBJECT *const obj = Object_Get(item->object_id);
    JSONW_WRITE(io, "object_id", Object_ToGameID(item->object_id));

    if (obj->save_position) {
        M_WriteXYZ32(io, "pos", item->pos);
        M_WriteXYZ16(io, "rot", item->rot);
        JSONW_WRITE(io, "room_num", item->room_num);
        JSONW_WRITE(io, "speed", item->speed);
        JSONW_WRITE(io, "fall_speed", item->fall_speed);
    }

    if (obj->save_anim) {
        JSONW_WRITE(io, "current_anim", item->current_anim_state);
        JSONW_WRITE(io, "goal_anim", item->goal_anim_state);
        JSONW_WRITE(io, "required_anim", item->required_anim_state);
        JSONW_WRITE(io, "anim_num", item->anim_num);
        JSONW_WRITE(io, "frame_num", item->frame_num);
        JSONW_WRITE(io, "prev_frame_num", item->prev_frame_num);
    }

    if (obj->save_hitpoints) {
        JSONW_WRITE(io, "hitpoints", item->hit_points);
        JSONW_WRITE(io, "max_hitpoints", item->max_hit_points);
    }

    if (obj->save_flags) {
        JSONW_WRITE(io, "flags", item->flags);
        JSONW_WRITE(io, "status", item->status);
        JSONW_WRITE(io, "active", item->active);
        JSONW_WRITE(io, "gravity", item->gravity);
        JSONW_WRITE(io, "collidable", item->collidable);
        const bool intelligent =
            obj->intelligent && item->creature_data != nullptr;
        JSONW_WRITE(io, "intelligent", intelligent);
        JSONW_WRITE(io, "timer", item->timer);
        JSONW_WRITE_NZ(io, "ai_bits", item->ai_bits);
        JSONW_WRITE_NZ(io, "ai_tag", item->ai_tag);
        if (intelligent) {
            const CREATURE *const creature = item->creature_data;
            JSONW_WRITE(io, "head_rot", creature->head_rotation);
            JSONW_WRITE(io, "neck_rot", creature->neck_rotation);
            JSONW_WRITE(io, "max_turn", creature->maximum_turn);
            JSONW_WRITE(io, "creature_flags", creature->flags);
            JSONW_WRITE(io, "creature_mood", creature->mood);
            JSONW_PUSH_OBJECT(io);
            JSONW_WRITE(io, "alerted", creature->alerted);
            JSONW_WRITE(io, "head_left", creature->head_left);
            JSONW_WRITE(io, "head_right", creature->head_right);
            JSONW_WRITE(io, "reached_goal", creature->reached_goal);
            JSONW_WRITE(io, "patrol_2", creature->patrol_2);
            JSONW_WRITE(io, "hurt_by_lara", creature->hurt_by_lara);
            JSONW_WRITE(io, "damage_from_lara", creature->damage_from_lara);
            JSONW_PUSH_ARRAY(io);
            for (int32_t i = 0; i < 4; i++) {
                JSONW_PUSH_VALUE(io, creature->joint_rotation[i]);
                JSONW_POP_AND_APPEND(io);
            }
            JSONW_POP_AND_SET(io, "joint_rotations");
            JSONW_POP_AND_SET(io, "creature");
        }
    }

    JSONW_PUSH_ARRAY(io);
    const CARRIED_ITEM *drop_item = item->carried_item;
    while (drop_item != nullptr) {
        XYZ_32 drop_pos = drop_item->pos;
        int16_t drop_rot_y = drop_item->rot.y;
        int16_t drop_room_num = drop_item->room_num;
        int16_t drop_fall_speed = drop_item->fall_speed;
        const DROP_STATUS save_status = Carrier_GetSaveStatus(drop_item);

        if ((save_status == DS_FALLING || save_status == DS_DROPPED)
            && drop_item->spawn_num != NO_ITEM) {
            const ITEM *const pickup = Item_Get(drop_item->spawn_num);
            if (pickup != nullptr) {
                drop_pos = pickup->pos;
                drop_rot_y = pickup->rot.y;
                drop_room_num = pickup->room_num;
                drop_fall_speed = pickup->fall_speed;
            }
        }

        JSONW_PUSH_OBJECT(io);
        JSONW_WRITE(io, "object_id", Object_ToGameID(drop_item->object_id));
        M_WriteXYZ32(io, "pos", drop_pos);
        JSONW_WRITE(io, "y_rot", drop_rot_y);
        JSONW_WRITE(io, "room_num", drop_room_num);
        JSONW_WRITE(io, "fall_speed", drop_fall_speed);
        JSONW_WRITE(io, "spawn_num", drop_item->spawn_num);
        JSONW_WRITE(io, "status", (int32_t)save_status);
        JSONW_POP_AND_APPEND(io);
        drop_item = drop_item->next_item;
    }
    JSONW_POP_AND_SET(io, "carried_items");

    if (obj->priv_size > 0 && obj->priv_save_func != nullptr) {
        JSONW_PUSH_OBJECT(io);
        obj->priv_save_func(item, io);
        JSONW_POP_AND_SET(io, "priv");
    }
}

static void M_WriteArm(
    JSON_WRITE_IO *const io, const char *const key, const LARA_ARM *const arm)
{
    ASSERT(arm != nullptr);
    JSONW_PUSH_OBJECT(io);
    JSONW_WRITE(io, "anim_num", arm->anim_num);
    JSONW_WRITE(io, "frame_num", arm->frame_num);
    JSONW_WRITE(io, "lock", arm->lock);
    JSONW_WRITE(io, "flash_gun", arm->flash_gun);
    M_WriteXYZ16(io, "rot", arm->rot);
    JSONW_POP_AND_SET(io, key);
}

static void M_WriteAmmo(
    JSON_WRITE_IO *const io, const char *const key, const AMMO_INFO *const ammo)
{
    ASSERT(ammo != nullptr);
    JSONW_PUSH_OBJECT(io);
    JSONW_WRITE(io, "ammo", ammo->ammo);
    JSONW_POP_AND_SET(io, key);
}

static void M_WriteLOT(JSON_WRITE_IO *const io, const LOT_INFO *const lot)
{
    ASSERT(lot != nullptr);
    JSONW_WRITE(io, "head", lot->head);
    JSONW_WRITE(io, "tail", lot->tail);
    JSONW_WRITE(io, "search_num", lot->search_num);
    JSONW_WRITE(io, "block_mask", lot->setup.block_mask);
    JSONW_WRITE(io, "step", lot->setup.step);
    JSONW_WRITE(io, "drop", lot->setup.drop);
    JSONW_WRITE(io, "fly", lot->setup.fly);
    JSONW_WRITE(io, "zone_count", lot->zone_count);
    JSONW_WRITE(io, "target_box", lot->target_box);
    JSONW_WRITE(io, "required_box", lot->required_box);
    JSONW_WRITE(io, "x", lot->target.x);
    JSONW_WRITE(io, "y", lot->target.y);
    JSONW_WRITE(io, "z", lot->target.z);
}

static void M_WriteResumeInfo(
    JSON_WRITE_IO *const io, const RESUME_INFO *const resume)
{
    JSONW_WRITE(io, "available", resume->flags.available);
    JSONW_WRITE(io, "level_completed", resume->level_completed);
    JSONW_WRITE(io, "prev_level", resume->prev_level);

    JSONW_WRITE(io, "hurt_allies", resume->hurt_allies);

    JSONW_WRITE(io, "lara_hitpoints", resume->lara_hitpoints);
    JSONW_WRITE(io, "pistol_ammo", resume->pistol_ammo);
    JSONW_WRITE(io, "shotgun_ammo", resume->shotgun_ammo);
    JSONW_WRITE(io, "magnum_ammo", resume->magnum_ammo);
    JSONW_WRITE(io, "autos_ammo", resume->autos_ammo);
    JSONW_WRITE(io, "desert_eagle_ammo", resume->desert_eagle_ammo);
    JSONW_WRITE(io, "uzi_ammo", resume->uzi_ammo);
    JSONW_WRITE(io, "m16_ammo", resume->m16_ammo);
    JSONW_WRITE(io, "mp5_ammo", resume->mp5_ammo);
    JSONW_WRITE(io, "grenade_ammo", resume->grenade_ammo);
    JSONW_WRITE(io, "rocket_ammo", resume->rocket_ammo);
    JSONW_WRITE(io, "harpoon_ammo", resume->harpoon_ammo);
    JSONW_WRITE(io, "num_medis", resume->small_medipacks);
    JSONW_WRITE(io, "num_big_medis", resume->large_medipacks);
    JSONW_WRITE(io, "num_flares", resume->flares);
    JSONW_WRITE(io, "num_scions", resume->num_scions);
    JSONW_WRITE(io, "num_quest_item_1", resume->num_quest_item_1);
    JSONW_WRITE(io, "num_quest_item_2", resume->num_quest_item_2);
    JSONW_WRITE(io, "num_quest_item_3", resume->num_quest_item_3);
    JSONW_WRITE(io, "num_quest_item_4", resume->num_quest_item_4);
    JSONW_WRITE(io, "gun_status", resume->gun_status);
    JSONW_WRITE(io, "gun_type", resume->equipped_gun_type);
    JSONW_WRITE(io, "holsters_gun_type", resume->holsters_gun_type);
    JSONW_WRITE(io, "back_gun_type", resume->back_gun_type);

    JSONW_WRITE(io, "has_pistols", resume->flags.has_pistols);
    JSONW_WRITE(io, "has_shotgun", resume->flags.has_shotgun);
    JSONW_WRITE(io, "has_magnums", resume->flags.has_magnums);
    JSONW_WRITE(io, "has_autos", resume->flags.has_autos);
    JSONW_WRITE(io, "has_desert_eagle", resume->flags.has_desert_eagle);
    JSONW_WRITE(io, "has_uzis", resume->flags.has_uzis);
    JSONW_WRITE(io, "has_m16", resume->flags.has_m16);
    JSONW_WRITE(io, "has_mp5", resume->flags.has_mp5);
    JSONW_WRITE(io, "has_grenade", resume->flags.has_grenade);
    JSONW_WRITE(io, "has_rocket", resume->flags.has_rocket);
    JSONW_WRITE(io, "has_harpoon", resume->flags.has_harpoon);

    JSONW_WRITE(io, "costume", resume->flags.costume);
    JSONW_WRITE(io, "timer", resume->stats.timer);
    JSONW_WRITE(io, "kills", resume->stats.kill_count);
    JSONW_WRITE(io, "secrets", resume->stats.secret_flags);
    JSONW_WRITE(io, "pickups", resume->stats.pickup_count);
    JSONW_WRITE(io, "ammo_hits", resume->stats.ammo_hits);
    JSONW_WRITE(io, "ammo_used", resume->stats.ammo_used);
    JSONW_WRITE(io, "distance_travelled", resume->stats.distance_travelled);
    JSONW_WRITE(io, "medipacks_used", resume->stats.medipacks_used);
    JSONW_WRITE(io, "death_count", resume->stats.death_count);
}

static int32_t M_GetMusicTrackFlagsCount(void)
{
    int32_t last_index = -1;
    for (int32_t i = 0; i < MAX_MUSIC_TRACKS; i++) {
        const uint16_t flags = Music_GetTrackFlags(i);
        if (flags != 0) {
            last_index = i;
        }
    }
    return last_index + 1;
}

static void M_WriteFXRings(
    JSON_WRITE_IO *const io, const FX_RING_TYPE type, const char *const key)
{
    if (!FX_Ring_IsRingActive(type)) {
        return;
    }
    JSONW_PUSH_ARRAY(io);
    for (int32_t i = 0;; i++) {
        const FX_RING *const ring = FX_Ring_PeekRing(type, i);
        if (ring == nullptr) {
            break;
        }

        JSONW_PUSH_OBJECT(io);
        JSONW_WRITE(io, "on", ring->on);
        JSONW_WRITE(io, "life", ring->life);
        JSONW_WRITE(io, "speed", ring->speed);
        JSONW_WRITE(io, "radius", ring->radius);
        JSONW_WRITE(io, "prev_radius", ring->prev_radius);
        M_WriteXYZ16(io, "rot", (XYZ_16) { ring->rot.x, 0, ring->rot.z });
        M_WriteXYZ16(
            io, "prev_rot", (XYZ_16) { ring->prev_rot.x, 0, ring->prev_rot.z });
        M_WriteXYZ32(io, "pos", ring->pos);
        M_WriteXYZ32(io, "prev_pos", ring->prev_pos);
        JSONW_POP_AND_APPEND(io);
    }
    JSONW_POP_AND_SET_NZ(io, key);
}

void SG_File_DumpFlares(JSON_WRITE_IO *const io)
{
    JSONW_PUSH_ARRAY(io);
    for (int32_t i = 0; i < Item_GetTotalCount(); i++) {
        const ITEM *const item = Item_Get(i);
        if (!item->active || item->object_id != O_FLARE_ITEM) {
            continue;
        }
        JSONW_PUSH_OBJECT(io);
        M_WriteXYZ32(io, "pos", item->pos);
        M_WriteXYZ16(io, "rot", item->rot);
        JSONW_WRITE(io, "room_num", item->room_num);
        JSONW_WRITE(io, "speed", item->speed);
        JSONW_WRITE(io, "fall_speed", item->fall_speed);
        const int32_t flare_age = FlareItem_GetAge(item);
        const int32_t active = FlareItem_IsActive(item) ? 0x8000 : 0;
        JSONW_WRITE(io, "age", flare_age | active);
        JSONW_POP_AND_APPEND(io);
    }
    JSONW_POP_AND_SET(io, "flares");
}

void SG_File_DumpEffects(JSON_WRITE_IO *const io)
{
    M_FX_ORDER fx_order;
    M_GetFXOrder(&fx_order);

    JSONW_PUSH_ARRAY(io);
    for (int16_t link_num = Effect_GetActiveNum(); link_num != NO_ITEM;
         link_num = Effect_Get(link_num)->next_active) {
        EFFECT *const effect = Effect_Get(link_num);
        if (Object_ToGameID(effect->object_id) == -1) {
            continue;
        }
        JSONW_PUSH_OBJECT(io);
        M_WriteXYZ32(io, "pos", effect->pos);
        M_WriteXYZ16(io, "rot", effect->rot);
        JSONW_WRITE(io, "room_num", effect->room_num);
        JSONW_WRITE(io, "object_id", Object_ToGameID(effect->object_id));
        JSONW_WRITE(io, "speed", effect->speed);
        JSONW_WRITE(io, "fall_speed", effect->fall_speed);
        // Introduced in TRX 1.2
        JSONW_WRITE(io, "frame_num", effect->frame_num);
        JSONW_WRITE(io, "frame_number", effect->frame_num);
        JSONW_WRITE(io, "counter", effect->counter);
        JSONW_WRITE(io, "shade", effect->shade);
        JSONW_POP_AND_APPEND(io);
    }
    JSONW_POP_AND_SET(io, "effects");
}

void SG_File_DumpFX(JSON_WRITE_IO *const io)
{
    JSONW_PUSH_OBJECT(io);
    JSONW_PUSH_OBJECT(io);
    M_WriteFXRings(io, FX_RING_TYPE_BLAST, "blast");
    M_WriteFXRings(io, FX_RING_TYPE_KNOCKBACK, "knockback");
    M_WriteFXRings(io, FX_RING_TYPE_SUMMON, "summon");
    JSONW_POP_AND_SET_NZ(io, "rings");
    JSONW_POP_AND_SET_NZ(io, "vfx");
}

void SG_File_DumpInventory(JSON_WRITE_IO *const io)
{
    JSONW_PUSH_OBJECT(io);
    JSONW_WRITE(io, "pickup1", Inv_RequestItem(O_PICKUP_ITEM_1));
    JSONW_WRITE(io, "pickup2", Inv_RequestItem(O_PICKUP_ITEM_2));
    JSONW_WRITE(io, "quest1", Inv_RequestItem(O_QUEST_ITEM_1));
    JSONW_WRITE(io, "quest2", Inv_RequestItem(O_QUEST_ITEM_2));
    JSONW_WRITE(io, "quest3", Inv_RequestItem(O_QUEST_ITEM_3));
    JSONW_WRITE(io, "quest4", Inv_RequestItem(O_QUEST_ITEM_4));
    JSONW_WRITE(io, "puzzle1", Inv_RequestItem(O_PUZZLE_ITEM_1));
    JSONW_WRITE(io, "puzzle2", Inv_RequestItem(O_PUZZLE_ITEM_2));
    JSONW_WRITE(io, "puzzle3", Inv_RequestItem(O_PUZZLE_ITEM_3));
    JSONW_WRITE(io, "puzzle4", Inv_RequestItem(O_PUZZLE_ITEM_4));
    JSONW_WRITE(io, "key1", Inv_RequestItem(O_KEY_ITEM_1));
    JSONW_WRITE(io, "key2", Inv_RequestItem(O_KEY_ITEM_2));
    JSONW_WRITE(io, "key3", Inv_RequestItem(O_KEY_ITEM_3));
    JSONW_WRITE(io, "key4", Inv_RequestItem(O_KEY_ITEM_4));
    JSONW_WRITE(io, "leadbar", Inv_RequestItem(O_LEADBAR_ITEM));
    JSONW_POP_AND_SET(io, "inventory");
}

void SG_File_DumpFlipmaps(JSON_WRITE_IO *const io)
{
    JSONW_PUSH_OBJECT(io);
    JSONW_WRITE(io, "status", Room_GetFlipStatus());
    JSONW_WRITE(io, "effect", Room_GetFlipEffect());
    JSONW_WRITE(io, "timer", Room_GetFlipTimer());
    JSONW_PUSH_ARRAY(io);
    for (int32_t i = 0; i < MAX_FLIP_MAPS; i++) {
        JSONW_PUSH_VALUE(io, Room_GetFlipSlotFlags(i) >> 8);
        JSONW_POP_AND_APPEND(io);
    }
    JSONW_POP_AND_SET(io, "table");
    JSONW_POP_AND_SET(io, "flipmap");
}

void SG_File_DumpCameras(JSON_WRITE_IO *const io)
{
    JSONW_PUSH_ARRAY(io);
    for (int32_t i = 0; i < Camera_GetFixedObjectCount(); i++) {
        const OBJECT_VECTOR *const object = Camera_GetFixedObject(i);
        JSONW_PUSH_VALUE(io, object->flags);
        JSONW_POP_AND_APPEND(io);
    }
    JSONW_POP_AND_SET(io, "cameras");
}

void SG_File_DumpMusic(JSON_WRITE_IO *const io)
{
    const int32_t track_flag_count = M_GetMusicTrackFlagsCount();
    JSONW_PUSH_OBJECT(io);
    JSONW_PUSH_ARRAY(io);
    for (int32_t i = 0; i < track_flag_count; i++) {
        JSONW_PUSH_VALUE(io, Music_GetTrackFlags(i));
        JSONW_POP_AND_APPEND(io);
    }
    JSONW_POP_AND_SET(io, "flags");

    const MUSIC_ID current_ambient = Music_GetCurrentLoopedTrack();
    JSONW_PUSH_OBJECT(io);
    JSONW_WRITE(io, "current_ambient", current_ambient);
    JSONW_PUSH_ARRAY(io);
    const int32_t stream_count = Music_GetStreamCount();
    for (int32_t i = 0; i < stream_count; i++) {
        MUSIC_STREAM_STATE state = {};
        if (!Music_GetStreamState(i, &state)) {
            continue;
        }

        JSONW_PUSH_OBJECT(io);
        JSONW_WRITE(io, "track", state.track_id);
        JSONW_WRITE(io, "mode", state.mode);
        JSONW_WRITE(io, "timestamp", state.timestamp);
        JSONW_POP_AND_APPEND(io);
    }
    JSONW_POP_AND_SET(io, "streams");
    JSONW_POP_AND_SET(io, "current");
    JSONW_POP_AND_SET(io, "music");
}

void SG_File_DumpItems(JSON_WRITE_IO *const io)
{
    Savegame_ProcessItemsBeforeSave();
    M_FX_ORDER fx_order;
    M_GetFXOrder(&fx_order);

    JSONW_PUSH_ARRAY(io);
    for (int32_t i = 0; i < Item_GetLevelCount(); i++) {
        JSONW_PUSH_OBJECT(io);
        M_WriteItem(io, Item_Get(i), &fx_order);
        JSONW_POP_AND_APPEND(io);
    }
    JSONW_POP_AND_SET(io, "items");
}

void SG_File_DumpLara(JSON_WRITE_IO *const io)
{
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    ASSERT(lara != nullptr);

    JSONW_PUSH_OBJECT(io);

    // Introduced in TRX 1.2
    JSONW_WRITE(io, "item_num", lara->item_num);
    JSONW_WRITE(io, "item_number", lara->item_num);
    JSONW_WRITE(io, "gun_status", lara->gun_status);
    JSONW_WRITE(io, "gun_type", lara->gun_type);
    JSONW_WRITE(io, "request_gun_type", lara->request_gun_type);
    JSONW_WRITE(io, "last_gun_type", lara->last_gun_type);

    JSONW_WRITE(io, "calc_fall_speed", lara->calc_fall_speed);
    JSONW_WRITE(io, "water_status", lara->water_status);
    JSONW_WRITE(io, "climb_status", lara->climb_status);
    JSONW_WRITE(io, "is_crouched", lara->is_crouched);
    JSONW_WRITE(io, "keep_crouched", lara->keep_crouched);
    JSONW_WRITE(io, "sprinting", lara->sprinting);

    JSONW_WRITE(io, "pose_count", lara->pose_count);
    JSONW_WRITE(io, "hit_frame", lara->hit_frame);
    JSONW_WRITE(io, "hit_direction", lara->hit_direction);
    JSONW_WRITE(io, "hit_effect_count", lara->hit_effect_count);
    JSONW_WRITE(
        io, "hit_effect",
        lara->hit_effect ? Effect_GetIndex(lara->hit_effect) : 0);

    JSONW_WRITE(io, "air", lara->air);
    JSONW_WRITE(io, "sprint_timer", lara->sprint_timer);
    JSONW_WRITE(io, "exposure_timer", lara->exposure_timer);
    JSONW_WRITE(io, "poison_timer", lara->poison_timer);
    JSONW_WRITE(io, "dive_count", lara->dive_timer);
    JSONW_WRITE(io, "death_count", lara->death_timer);

    JSONW_WRITE(io, "current_active", lara->current.active);
    JSONW_WRITE(io, "current_vel_x", lara->current.vel.x);
    JSONW_WRITE(io, "current_vel_z", lara->current.vel.z);
    JSONW_WRITE(io, "burn", lara->burn);
    JSONW_WRITE(io, "electric", lara->electric);
    JSONW_WRITE(io, "water_surface_dist", lara->water_surface_dist);

    JSONW_WRITE(io, "flare_age", lara->flare.age);
    JSONW_WRITE(io, "flare_frame", lara->flare.frame_num);
    JSONW_WRITE(io, "flare_control_left", lara->flare.control);
    JSONW_WRITE(io, "extra_anim", lara->extra_anim);
    // Introduced in TRX 1.2
    JSONW_WRITE(io, "vehicle_item_num", Lara_Vehicle_GetIndex());
    JSONW_WRITE(io, "vehicle_item_number", Lara_Vehicle_GetIndex());

    JSONW_WRITE(io, "mesh_effects", lara->mesh_effects);

    JSONW_PUSH_OBJECT(io);
    JSONW_WRITE(io, "skin_type", Lara_Skin_GetType());
    JSONW_WRITE(io, "skin_is_default", Lara_Skin_IsDefaultType());
    JSONW_WRITE(io, "holsters_visible", Lara_Skin_AreHolstersVisible());
    JSONW_PUSH_ARRAY(io);
    for (int32_t i = 0; i < LM_NUMBER_OF; i++) {
        const LARA_SKIN_EQUIPMENT *const equipment = Lara_Skin_GetEquipment(i);
        JSONW_PUSH_OBJECT(io);
        JSONW_WRITE(io, "type", equipment->type);
        JSONW_WRITE(io, "data", equipment->data);
        JSONW_POP_AND_APPEND(io);
    }
    JSONW_POP_AND_SET(io, "equipment");
    JSONW_POP_AND_SET(io, "skin");

    JSONW_WRITE(io, "target_angle1", lara->target_angles[0]);
    JSONW_WRITE(io, "target_angle2", lara->target_angles[1]);
    JSONW_WRITE(io, "turn_rate", lara->turn_rate);
    JSONW_WRITE(io, "move_angle", lara->move_angle);
    M_WriteXYZ16(io, "head_rot", lara->head_rot);
    M_WriteXYZ16(io, "torso_rot", lara->torso_rot);
    M_WriteXYZ32(io, "last_pos", lara->last_pos);
    M_WriteArm(io, "left_arm", &lara->left_arm);
    M_WriteArm(io, "right_arm", &lara->right_arm);
    M_WriteAmmo(io, "pistols", &lara->pistol_ammo);
    M_WriteAmmo(io, "shotgun", &lara->shotgun_ammo);
    M_WriteAmmo(io, "magnums", &lara->magnum_ammo);
    M_WriteAmmo(io, "autos", &lara->autos_ammo);
    M_WriteAmmo(io, "desert_eagle", &lara->desert_eagle_ammo);
    M_WriteAmmo(io, "uzis", &lara->uzi_ammo);
    M_WriteAmmo(io, "harpoon", &lara->harpoon_ammo);
    M_WriteAmmo(io, "grenade", &lara->grenade_ammo);
    M_WriteAmmo(io, "rocket", &lara->rocket_ammo);
    M_WriteAmmo(io, "m16", &lara->m16_ammo);
    M_WriteAmmo(io, "mp5", &lara->mp5_ammo);

    if (lara->gun_item_num != NO_ITEM) {
        JSONW_PUSH_OBJECT(io);
        const ITEM *const weapon_item = Item_Get(lara->gun_item_num);
        JSONW_WRITE(io, "object_id", Object_ToGameID(weapon_item->object_id));
        JSONW_WRITE(io, "anim_num", weapon_item->anim_num);
        JSONW_WRITE(io, "frame_num", weapon_item->frame_num);
        JSONW_WRITE(io, "current_anim_state", weapon_item->current_anim_state);
        JSONW_WRITE(io, "goal_anim_state", weapon_item->goal_anim_state);
        JSONW_POP_AND_SET(io, "weapon");
    }

    JSONW_PUSH_OBJECT(io);
    JSONW_WRITE(io, "item_num", lara->interact_target.item_num);
    JSONW_WRITE(io, "move_count", lara->interact_target.move_count);
    JSONW_WRITE(io, "is_moving", lara->interact_target.is_moving);
    JSONW_POP_AND_SET(io, "interact_target");

    JSONW_POP_AND_SET(io, "lara");
}

void SG_File_DumpResumeInfoList(JSON_WRITE_IO *const io)
{
    const int32_t count = GF_GetLevelTable(GFLT_MAIN)->count;
    JSONW_PUSH_ARRAY(io);
    for (int32_t i = 0; i < count; i++) {
        const GF_LEVEL *const level = GF_GetLevel(GFLT_MAIN, i);
        const RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);
        JSONW_PUSH_OBJECT(io);
        M_WriteResumeInfo(io, resume);
        JSONW_POP_AND_APPEND(io);
    }
    JSONW_POP_AND_SET(io, "resume_info");
}

void SG_File_DumpMisc(JSON_WRITE_IO *const io)
{
    const GF_LEVEL *const level = Game_GetCurrentLevel();
    const RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);

    JSONW_PUSH_OBJECT(io);
    JSONW_WRITE(io, "game_version", g_TRXVersion);
    JSONW_WRITE(io, "bonus_flag", Game_GetBonusFlag());
    JSONW_WRITE(io, "death_count", resume->stats.death_count);
    JSONW_WRITE(io, "are_monks_angry", Creature_AreAlliesHostile());
    JSONW_WRITE(io, "sunset_timer", Output_GetTimeInGame());
    JSONW_WRITE(io, "rng_control_seed", Random_GetControlSeed());
    JSONW_WRITE(io, "rng_draw_seed", Random_GetDrawSeed());
    JSONW_WRITE(io, "weather_type", FX_Weather_GetWeather());
    JSONW_POP_AND_SET(io, "misc");

    JSONW_WRITE(io, "level_title", level->title != nullptr ? level->title : "");
    JSONW_WRITE(io, "save_counter", Savegame_GetCounter());
    JSONW_WRITE(io, "level_num", level->num);
}
