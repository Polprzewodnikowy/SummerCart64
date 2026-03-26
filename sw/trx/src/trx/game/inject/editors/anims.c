#include <trx/game/inject.h>
#include <trx/game/objects/common.h>

#include <string.h>

static void M_FrameEdits(
    const INJECTION_CONTEXT *const ctx, const INJECTION *const injection,
    const int32_t data_count)
{
    LEVEL_CONTEXT_INFO *const level_info = Level_Context_GetInfo();
    for (int32_t i = 0; i < data_count; i++) {
        const INJECTION_OBJECT_INFO obj_info = Inject_ReadObjectPtr(injection);
        const int32_t anim_idx = VFile_ReadS32(injection->fp);
        const int32_t packed_rot = VFile_ReadS32(injection->fp);

        const OBJECT *const obj = Object_Get(obj_info.id);
        if (ctx->mode == INJECTION_MODE_STATS || !obj->loaded) {
            continue;
        }

        const ANIM *const anim = Object_GetAnim(obj, anim_idx);
        int16_t *data_ptr =
            &level_info->anims.frames[anim->frame_ofs / sizeof(int16_t)];
        data_ptr += 10;
        memcpy(data_ptr, &packed_rot, sizeof(int32_t));
    }
}

static void M_FrameReplacements(
    const INJECTION_CONTEXT *const ctx, const INJECTION *const injection,
    const int32_t data_count)
{
    LEVEL_CONTEXT_INFO *const level_info = Level_Context_GetInfo();
    for (int32_t i = 0; i < data_count; i++) {
        const INJECTION_OBJECT_INFO obj_info = Inject_ReadObjectPtr(injection);
        const int32_t num_anims = VFile_ReadS32(injection->fp);

        const OBJECT *const obj = Object_Get(obj_info.id);
        for (int32_t j = 0; j < num_anims; j++) {
            const int32_t anim_idx = VFile_ReadS32(injection->fp);
            const int32_t num_frames = VFile_ReadS32(injection->fp);

            if (ctx->mode == INJECTION_MODE_STATS) {
                VFile_Skip(injection->fp, num_frames * sizeof(int16_t));
            } else {
                const ANIM *const anim = Object_GetAnim(obj, anim_idx);
                int16_t *const data_ptr =
                    &level_info->anims
                         .frames[anim->frame_ofs / sizeof(int16_t)];
                VFile_Read(
                    injection->fp, data_ptr, num_frames * sizeof(int16_t));
            }
        }
    }
}

static void M_AnimEdits(
    const INJECTION_CONTEXT *const ctx, const INJECTION *const injection,
    const int32_t data_count)
{
    for (int32_t i = 0; i < data_count; i++) {
        const INJECTION_OBJECT_INFO obj_info = Inject_ReadObjectPtr(injection);
        const OBJECT *const obj = Object_Get(obj_info.id);
        const int32_t anim_idx = VFile_ReadS32(injection->fp);
        const int32_t velocity = VFile_ReadS32(injection->fp);
        if (ctx->mode == INJECTION_MODE_STATS) {
            continue;
        }

        ANIM *const anim = Object_GetAnim(obj, anim_idx);
        anim->velocity = velocity;
    }
}

REGISTER_INJECT_EDITOR(IDT_FRAME_EDITS, M_FrameEdits)
REGISTER_INJECT_EDITOR(IDT_FRAME_REPLACE, M_FrameReplacements)
REGISTER_INJECT_EDITOR(IDT_ANIM_EDITS, M_AnimEdits)
