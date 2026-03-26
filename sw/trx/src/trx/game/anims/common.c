#include <trx/game/anims/common.h>

#include <trx/debug.h>
#include <trx/game/game_buf.h>

static int32_t m_AnimCount = 0;
static int32_t m_ChangeCount = 0;
static int32_t m_RangeCount = 0;
static int32_t m_BoneCount = 0;
static ANIM *m_Anims = nullptr;
static ANIM_CHANGE *m_Changes = nullptr;
static ANIM_RANGE *m_Ranges = nullptr;
static ANIM_BONE *m_Bones = nullptr;
static ANIM m_NullAnim = {};

void Anim_InitialiseAnims(const int32_t num_anims)
{
    m_AnimCount = num_anims;
    m_Anims = GameBuf_Alloc(sizeof(ANIM) * num_anims, GBUF_ANIMS);
}

void Anim_InitialiseChanges(const int32_t num_changes)
{
    m_ChangeCount = num_changes;
    m_Changes =
        GameBuf_Alloc(sizeof(ANIM_CHANGE) * num_changes, GBUF_ANIM_CHANGES);
}

void Anim_InitialiseRanges(const int32_t num_ranges)
{
    m_RangeCount = num_ranges;
    m_Ranges = GameBuf_Alloc(sizeof(ANIM_RANGE) * num_ranges, GBUF_ANIM_RANGES);
}

void Anim_InitialiseBones(const int32_t num_bones)
{
    m_BoneCount = num_bones;
    m_Bones = GameBuf_Alloc(sizeof(ANIM_BONE) * num_bones, GBUF_ANIM_BONES);
}

int32_t Anim_GetTotalCount(void)
{
    return m_AnimCount;
}

ANIM *Anim_GetAnim(const int32_t anim_idx)
{
    if (anim_idx < 0 || anim_idx >= m_AnimCount) {
        return &m_NullAnim;
    }
    return &m_Anims[anim_idx];
}

ANIM_CHANGE *Anim_GetChange(const int32_t change_idx)
{
    ASSERT(change_idx >= 0 && change_idx < m_ChangeCount);
    return &m_Changes[change_idx];
}

ANIM_RANGE *Anim_GetRange(const int32_t range_idx)
{
    ASSERT(range_idx >= 0 && range_idx < m_RangeCount);
    return &m_Ranges[range_idx];
}

ANIM_BONE *Anim_GetBone(const int32_t bone_idx)
{
    ANIM_BONE *const result = Anim_TryGetBone(bone_idx);
    ASSERT(result != nullptr);
    return result;
}

ANIM_BONE *Anim_TryGetBone(const int32_t bone_idx)
{
    if (bone_idx >= 0 && bone_idx < m_BoneCount) {
        return &m_Bones[bone_idx];
    }
    return nullptr;
}

bool Anim_TestAbsFrameEqual(const int16_t abs_frame, const int16_t frame)
{
    return abs_frame == frame;
}

bool Anim_TestAbsFrameRange(
    const int16_t abs_frame, const int16_t start, const int16_t end)
{
    return abs_frame >= start && abs_frame <= end;
}

bool Anim_HasChange(const ANIM *const anim, const int16_t goal_state_id)
{
    for (int32_t i = 0; i < anim->num_changes; i++) {
        const ANIM_CHANGE *const change = Anim_GetChange(anim->change_idx + i);
        if (change->goal_anim_state == goal_state_id) {
            return true;
        }
    }
    return false;
}

bool Anim_HasFXCommand(const ANIM *const anim, const int16_t fx_num)
{
    return Anim_HasFXCommandBetween(
        anim, fx_num, 0, anim->frame_end - anim->frame_base);
}

bool Anim_HasFXCommandBetween(
    const ANIM *const anim, const int16_t fx_num, const int32_t frame_a,
    const int32_t frame_b)
{
    for (int32_t i = 0; i < anim->num_commands; i++) {
        const ANIM_COMMAND *const cmd = &anim->commands[i];
        if (cmd->type != AC_EFFECT) {
            continue;
        }

        const ANIM_COMMAND_EFFECT_DATA *const data =
            (ANIM_COMMAND_EFFECT_DATA *)cmd->data;
        if (data->effect_num != fx_num) {
            continue;
        }

        const int32_t frame_num = data->frame_num - anim->frame_base;
        if (frame_num >= frame_a && frame_num <= frame_b) {
            return true;
        }
    }

    return false;
}
