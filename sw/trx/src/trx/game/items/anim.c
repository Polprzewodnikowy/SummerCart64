#include <trx/game/items/anim.h>

#include <trx/config.h>
#include <trx/game/interpolation.h>
#include <trx/game/items/actions.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/version.h>

#define M_SFX_SURF_DISTANCE ((STEP_L * 2) + 1)
#define M_FRAME_INTERP_SCALE 1024

static bool M_ShouldPlaySFXAlways(
    const ITEM *const item, const bool item_underwater)
{
    if (item == Lara_GetItem()) {
        return true;
    }

    if (item->object_id == O_LARA_HARPOON_GUN) {
        return true;
    }

    int16_t room_num = item->room_num;
    if (room_num == NO_ROOM) {
        return false;
    }

    const int32_t dist =
        item_underwater ? -M_SFX_SURF_DISTANCE : +M_SFX_SURF_DISTANCE;
    Room_GetSector(
        (XYZ_32) { item->pos.x, item->pos.y + dist, item->pos.z }, &room_num);
    const ROOM *const nearby_room = Room_Get(room_num);
    const bool near_underwater = nearby_room->flags.underwater;
    return item_underwater != near_underwater;
}

ANIM *Item_GetAnim(const ITEM *const item)
{
    return Anim_GetAnim(item->anim_num);
}

bool Item_TestAnimEqual(const ITEM *const item, const int16_t anim_idx)
{
    const OBJECT *const obj = Object_Get(item->object_id);
    return item->anim_num == obj->anim_idx + anim_idx;
}

int16_t Item_GetRelativeAnim(const ITEM *const item)
{
    return Item_GetRelativeObjAnim(item, item->object_id);
}

int16_t Item_GetRelativeObjAnim(const ITEM *const item, const OBJECT_ID obj_id)
{
    return item->anim_num - Object_Get(obj_id)->anim_idx;
}

int16_t Item_GetRelativeFrame(const ITEM *const item)
{
    return item->frame_num - Item_GetAnim(item)->frame_base;
}

void Item_SwitchToAnim(
    ITEM *const item, const int16_t anim_idx, const int16_t frame)
{
    Item_SwitchToObjAnim(item, anim_idx, frame, item->object_id);
}

void Item_SwitchToObjAnim(
    ITEM *const item, const int16_t anim_idx, const int16_t frame,
    const OBJECT_ID obj_id)
{
    const OBJECT *const obj = Object_Get(obj_id);
    if (obj->anim_idx == NO_ANIM) {
        item->anim_num = NO_ANIM;
    } else {
        item->anim_num = obj->anim_idx + anim_idx;
    }

    const ANIM *const anim = Item_GetAnim(item);
    if (frame < 0) {
        item->frame_num = anim->frame_end + frame + 1;
    } else {
        item->frame_num = anim->frame_base + frame;
    }
}

bool Item_TestFrameEqual(const ITEM *const item, const int16_t frame)
{
    const ANIM *const anim = Item_GetAnim(item);
    const int16_t base_frame =
        frame < 0 ? (anim->frame_end + 1) : anim->frame_base;
    return Anim_TestAbsFrameEqual(item->frame_num, base_frame + frame);
}

bool Item_TestFrameRange(
    const ITEM *const item, const int16_t start, const int16_t end)
{
    return Anim_TestAbsFrameRange(
        item->frame_num, Item_GetAnim(item)->frame_base + start,
        Item_GetAnim(item)->frame_base + end);
}

ANIM_FRAME *Item_GetBestFrame(const ITEM *const item)
{
    ANIM_FRAME *frames[2];
    int32_t rate = 0;
    const int32_t frac = Item_GetFrames(item, frames, &rate);
    return frames[(frac > rate / 2) ? 1 : 0];
}

int32_t Item_GetFrames(const ITEM *item, ANIM_FRAME *frames[], int32_t *rate)
{
    const ANIM *const anim = Item_GetAnim(item);
    if (anim->frame_ptr == nullptr) {
        frames[0] = nullptr;
        return 0;
    }

    const int32_t cur_frame_num = item->frame_num - anim->frame_base;
    const int32_t last_frame_num = anim->frame_end - anim->frame_base;
    const int32_t key_frame_span = anim->interpolation;
    const int32_t first_key_frame_num = cur_frame_num / key_frame_span;
    const int32_t second_key_frame_num = first_key_frame_num + 1;

    int32_t interp_frame_num = cur_frame_num;
    double interp_frame_sub = 0.0;
    const double alpha = Interpolation_GetWorldRate();
    if (alpha >= 0.0 && alpha <= 1.0) {
        const bool prev_in_anim = item->prev_frame_num >= anim->frame_base
            && item->prev_frame_num <= anim->frame_end;
        if (prev_in_anim) {
            const int32_t prev_frame_num =
                item->prev_frame_num - anim->frame_base;
            const int32_t frame_delta = cur_frame_num - prev_frame_num;
            if (frame_delta > 0) {
                const OBJECT *const obj = Object_Get(item->object_id);
                const bool allow_interp = obj->can_interpolate_func == nullptr
                    || obj->can_interpolate_func(
                        item, first_key_frame_num, second_key_frame_num);
                if (allow_interp) {
                    const double frame_pos =
                        prev_frame_num + (frame_delta * alpha);
                    if (frame_pos < last_frame_num) {
                        interp_frame_num = (int32_t)frame_pos;
                        interp_frame_sub = frame_pos - interp_frame_num;
                    }
                }
            }
        }
    }

    const int32_t key_frame_shift = interp_frame_num % key_frame_span;
    const int32_t frame_a = interp_frame_num / key_frame_span;
    const int32_t frame_b = frame_a + 1;
    frames[0] = &anim->frame_ptr[frame_a];
    frames[1] = &anim->frame_ptr[frame_b];

    int32_t denominator = key_frame_span;
    if (key_frame_shift != 0 || interp_frame_sub > 0.0) {
        const int32_t second_key_frame_num2 =
            (interp_frame_num / key_frame_span + 1) * key_frame_span;
        if (second_key_frame_num2 > anim->frame_end) {
            denominator += anim->frame_end - second_key_frame_num2;
        }
    }

    const double numerator = key_frame_shift + interp_frame_sub;
    *rate = denominator * M_FRAME_INTERP_SCALE;
    return (int32_t)((numerator * M_FRAME_INTERP_SCALE) + 0.5);
}

void Item_Animate(ITEM *const item)
{
    item->hit_status = false;
    item->touch_bits = 0;
    item->prev_frame_num = item->frame_num;
    item->frame_num++;

    const ANIM *anim = Item_GetAnim(item);
    if (anim->num_changes > 0 && Item_GetAnimChange(item, anim)) {
        anim = Item_GetAnim(item);
        item->current_anim_state = anim->current_anim_state;

        if (item->required_anim_state == anim->current_anim_state) {
            item->required_anim_state = 0;
        }
    }

    if (item->frame_num > anim->frame_end) {
        for (int32_t i = 0; i < anim->num_commands; i++) {
            const ANIM_COMMAND *const command = &anim->commands[i];
            switch (command->type) {
            case AC_MOVE_ORIGIN: {
                const XYZ_16 *const pos = (XYZ_16 *)command->data;
                Item_Translate(item, pos->x, pos->y, pos->z);
                break;
            }

            case AC_JUMP_VELOCITY: {
                const ANIM_COMMAND_VELOCITY_DATA *const data =
                    (ANIM_COMMAND_VELOCITY_DATA *)command->data;
                item->fall_speed = data->fall_speed;
                item->speed = data->speed;
                item->gravity = true;
                break;
            }

            case AC_DEACTIVATE:
                const OBJECT *const obj = Object_Get(item->object_id);
                item->after_death = obj->intelligent ? 1 : 64;
                item->status = IS_DEACTIVATED;
                break;

            default:
                break;
            }
        }

        item->anim_num = anim->jump_anim_num;
        item->frame_num = anim->jump_frame_num;
        anim = Item_GetAnim(item);

        if (item->current_anim_state != anim->current_anim_state) {
            item->current_anim_state = anim->current_anim_state;
            item->goal_anim_state = anim->current_anim_state;
        }

        if (item->required_anim_state == item->current_anim_state) {
            item->required_anim_state = 0;
        }
    }

    for (int32_t i = 0; i < anim->num_commands; i++) {
        const ANIM_COMMAND *const command = &anim->commands[i];
        switch (command->type) {
        case AC_SOUND_FX: {
            const ANIM_COMMAND_EFFECT_DATA *const data =
                (ANIM_COMMAND_EFFECT_DATA *)command->data;
            Item_PlayAnimSFX(item, data);
            break;
        }

        case AC_EFFECT:
            const ANIM_COMMAND_EFFECT_DATA *const data =
                (ANIM_COMMAND_EFFECT_DATA *)command->data;
            if (item->frame_num == data->frame_num) {
                ItemAction_RunDirectWithFX(
                    data->effect_num, item, data->fx_type);
            }
            break;

        default:
            break;
        }
    }

    if (item->gravity) {
        item->fall_speed += item->fall_speed < FAST_FALL_SPEED ? GRAVITY : 1;
        item->pos.y += item->fall_speed;
    } else {
        int32_t speed = anim->velocity;
        if (anim->acceleration != 0) {
            speed += anim->acceleration * (item->frame_num - anim->frame_base);
        }
        item->speed = speed >> 16;
    }

    item->pos.x += (item->speed * Math_Sin(item->rot.y)) >> W2V_SHIFT;
    item->pos.z += (item->speed * Math_Cos(item->rot.y)) >> W2V_SHIFT;
}

bool Item_GetAnimChange(ITEM *const item, const ANIM *const anim)
{
    if (item->current_anim_state == item->goal_anim_state) {
        return false;
    }

    for (int32_t i = 0; i < anim->num_changes; i++) {
        const ANIM_CHANGE *const change = Anim_GetChange(anim->change_idx + i);
        if (change->goal_anim_state != item->goal_anim_state) {
            continue;
        }

        for (int32_t j = 0; j < change->num_ranges; j++) {
            const ANIM_RANGE *const range =
                Anim_GetRange(change->range_idx + j);

            if (Anim_TestAbsFrameRange(
                    item->frame_num, range->start_frame, range->end_frame)) {
                item->anim_num = range->link_anim_num;
                item->frame_num = range->link_frame_num;
                return true;
            }
        }
    }

    return false;
}

void Item_PlayAnimSFX(
    const ITEM *const item, const ANIM_COMMAND_EFFECT_DATA *const data)
{
    if (item->frame_num != data->frame_num) {
        return;
    }

    const bool is_lara = item == Lara_GetItem();
    const bool item_underwater =
        item->room_num != NO_ROOM && Room_Get(item->room_num)->flags.underwater;
    const ANIM_COMMAND_ENVIRONMENT mode = data->environment;

    if (mode != ACE_ALL && item->room_num != NO_ROOM) {
        int32_t height = NO_HEIGHT;
        if (is_lara) {
            height = Lara_GetLaraInfo()->water_surface_dist;
        } else if (item_underwater) {
            height = -STEP_L;
        }

        const bool in_water = height < 0 && height != NO_HEIGHT;
        if ((mode == ACE_WATER && !in_water)
            || (mode == ACE_LAND && in_water)) {
            return;
        }
    }

    const bool play_always = M_ShouldPlaySFXAlways(item, item_underwater);
    SOUND_PLAY_MODE play_mode = SPM_NORMAL;
    if (play_always) {
        play_mode = SPM_ALWAYS;
    } else if (
        Object_IsType(item->object_id, g_WaterObjects)
        || (g_Config.audio.enable_underwater_anim_sfx && item_underwater)) {
        play_mode = SPM_UNDERWATER;
    }

    const SAMPLE_ID sfx_num =
        is_lara ? Lara_Skin_GetAnimSFX(data->effect_num) : data->effect_num;
    Sound_Effect_Direct(sfx_num, &item->pos, play_mode);
}
