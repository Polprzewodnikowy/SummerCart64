#include <trx/game/camera/environment.h>

#include <trx/config.h>
#include <trx/game/camera.h>
#include <trx/game/game.h>
#include <trx/game/lara.h>
#include <trx/game/music.h>
#include <trx/game/output/vars.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>

typedef enum {
    TARGET_UNKNOWN,
    TARGET_INVALID,
    TARGET_VALID,
} M_TARGET_STATUS;

static void M_AdjustMusicVolume(const bool is_underwater)
{
    if (!Game_IsPlaying()) {
        return;
    }
    const bool is_ambient =
        Music_GetCurrentPlayingTrack() == Music_GetCurrentLoopedTrack();
    const bool is_cutscene = GF_GetCurrentLevel()->type == GFL_CUTSCENE;
    const double base_volume = is_cutscene ? g_Config.audio.cutscene_volume
        : is_ambient                       ? g_Config.audio.ambient_volume
                                           : g_Config.audio.music_volume;
    const double multiplier = !is_underwater || is_cutscene ? 1.0
        : is_ambient ? g_Config.audio.underwater_ambient_volume
                     : g_Config.audio.underwater_music_volume;
    Music_SetVolume(base_volume * multiplier);
}

static inline M_TARGET_STATUS M_HandleCameraTrigger(
    const TRIGGER_CMD *const cmd)
{
    const TRIGGER_CAMERA_DATA *const cam_data =
        (TRIGGER_CAMERA_DATA *)cmd->parameter;
    if (cam_data->camera_num != g_Camera.last) {
        return TARGET_INVALID;
    }

    g_Camera.num = cam_data->camera_num;

    const bool is_look_or_combat =
        g_Camera.type == CAM_LOOK || g_Camera.type == CAM_COMBAT;
    const bool is_locked = Camera_IsLocked(g_Camera.num);
    if (g_Camera.timer < 0 || (is_look_or_combat && !is_locked)) {
        g_Camera.timer = -1;
        return TARGET_INVALID;
    }

    g_Camera.type = CAM_FIXED;
    if (g_Config.visuals.enable_glide_cameras && cam_data->glide != 0) {
        g_Camera.speed = cam_data->glide + 1;
    }

    return TARGET_VALID;
}

static inline void M_HandleTargetTrigger(const TRIGGER_CMD *const cmd)
{
    const bool is_look_or_combat =
        g_Camera.type == CAM_LOOK || g_Camera.type == CAM_COMBAT;
    const bool is_locked = Camera_IsLocked(g_Camera.num);
    if (!is_look_or_combat || is_locked) {
        g_Camera.item = Item_Get((int16_t)(intptr_t)cmd->parameter);
    }
}

static inline void M_ValidateTriggerTarget(const M_TARGET_STATUS status)
{
    if (g_Camera.item == nullptr) {
        return;
    }

    const bool is_new_item = g_Camera.item != g_Camera.last_item;
    const bool item_was_looked_at = g_Camera.item->looked_at;

    const bool should_clear = (status == TARGET_INVALID)
        || (status == TARGET_UNKNOWN && item_was_looked_at && is_new_item);
    if (should_clear) {
        g_Camera.item = nullptr;
    }
}

void Camera_RefreshFromTrigger(const TRIGGER *const trigger)
{
    M_TARGET_STATUS status = TARGET_UNKNOWN;
    for (const TRIGGER_CMD *cmd = trigger->command; cmd != nullptr;
         cmd = cmd->next_cmd) {
        if (cmd->type == TO_CAMERA) {
            status = M_HandleCameraTrigger(cmd);
        } else if (cmd->type == TO_TARGET) {
            M_HandleTargetTrigger(cmd);
        }
    }

    M_ValidateTriggerTarget(status);

    if (g_Config.visuals.camera_mode != CAMERA_MODE_TR1
        && status != TARGET_UNKNOWN && g_Camera.num == -1
        && g_Camera.timer > 0) {
        g_Camera.timer = -1;
    }
}

void Camera_EnsureEnvironment(void)
{
    if (g_Camera.mic_pos.room_num != NO_ROOM) {
        const ROOM *const room = Room_Get(g_Camera.mic_pos.room_num);
        if (room->flags.underwater) {
            M_AdjustMusicVolume(true);
            Sound_Effect(SFX_UNDERWATER, nullptr, SPM_ALWAYS);
        } else {
            M_AdjustMusicVolume(false);
            Sound_StopEffect(SFX_UNDERWATER);
        }
    }

    if (g_Camera.pos.room_num != NO_ROOM) {
        const ROOM *const room = Room_Get(g_Camera.pos.room_num);
        g_Camera.underwater = room->flags.underwater;
    }

    uint8_t reverb_type = 0;
    if (g_Camera.mic_pos.room_num != NO_ROOM) {
        const ROOM *const room = Room_Get(g_Camera.mic_pos.room_num);
        reverb_type = room->reverb_info;
    }
    Sound_SetReverbType(reverb_type);
}

void Camera_UpdateMicPosition(void)
{
    if (g_Config.audio.enable_lara_mic) {
        const LARA_INFO *const lara_info = Lara_GetLaraInfo();
        const ITEM *const lara_item = Lara_GetItem();
        g_Camera.actual_angle =
            lara_info->torso_rot.y + lara_info->head_rot.y + lara_item->rot.y;
        g_Camera.mic_pos.room_num = lara_item->room_num;
        XYZ_32 pos = { 0, 16, 0 };
        if (lara_info->water_status == LWS_SURFACE) {
            pos.y = -36;
        }
        if (lara_info->water_surface_dist != -NO_HEIGHT
            && Lara_GetMeshPos(LM_HEAD, &pos)) {
            g_Camera.mic_pos.pos = pos;
            Room_GetSector(pos, &g_Camera.mic_pos.room_num);
        } else {
            g_Camera.mic_pos.pos = lara_item->pos;
        }
    } else {
        g_Camera.actual_angle = Math_Atan(
            g_Camera.target.z - g_Camera.pos.z,
            g_Camera.target.x - g_Camera.pos.x);
        g_Camera.mic_pos.pos.x = g_Camera.pos.x
            + ((g_PhdPersp * Math_Sin(g_Camera.actual_angle)) >> W2V_SHIFT);
        g_Camera.mic_pos.pos.z = g_Camera.pos.z
            + ((g_PhdPersp * Math_Cos(g_Camera.actual_angle)) >> W2V_SHIFT);
        g_Camera.mic_pos.pos.y = g_Camera.pos.y;
        g_Camera.mic_pos.room_num = g_Camera.pos.room_num;
    }
}
