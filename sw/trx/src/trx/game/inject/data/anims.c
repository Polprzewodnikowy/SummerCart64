#include <trx/core/memory.h>
#include <trx/debug.h>
#include <trx/game/anims.h>
#include <trx/game/inject.h>
#include <trx/game/inject/utils.h>
#include <trx/game/level/context.h>
#include <trx/game/level/sections/append.h>
#include <trx/game/objects.h>

static void M_HandleAnimData(
    const INJECTION_CONTEXT *const ctx, const INJECTION_CHUNK chunk)
{
    LEVEL_CONTEXT_INFO *const level_info = Level_Context_GetInfo();
    const LEVEL_CONTEXT_INFO cached_info = Inject_GetCachedInfo();

    for (int32_t i = 0; i < chunk.num_blocks; i++) {
        const INJECTION_DATA_TYPE data_type =
            VFile_ReadS32(chunk.injection->fp);
        const int32_t data_count = VFile_ReadS32(chunk.injection->fp);
        const int32_t data_size = VFile_ReadS32(chunk.injection->fp);

        if (ctx->mode == INJECTION_MODE_STATS) {
            VFile_Skip(chunk.injection->fp, data_size);
            continue;
        }

        switch (data_type) {
        case IDT_ANIMS: {
            Level_Section_AppendAnims(
                level_info->anims.anim_count, data_count, chunk.injection->fp);
            level_info->anims.anim_count += data_count;

            for (int32_t j = 0; j < data_count; j++) {
                ANIM *const anim =
                    Anim_GetAnim(cached_info.anims.anim_count + j);
                anim->jump_anim_num += cached_info.anims.anim_count;
                anim->frame_ofs += cached_info.anims.frame_count * 2;
                anim->change_idx += cached_info.anims.change_count;
                anim->command_idx += cached_info.anims.command_count;
            }
            break;
        }

        case IDT_ANIM_BONES: {
            Level_Section_AppendAnimBones(
                level_info->anims.bone_count, data_count, chunk.injection->fp);
            level_info->anims.bone_count += data_count;
            break;
        }

        case IDT_ANIM_CHANGES: {
            Level_Section_AppendAnimChanges(
                level_info->anims.change_count, data_count,
                chunk.injection->fp);
            level_info->anims.change_count += data_count;

            for (int32_t j = 0; j < data_count; j++) {
                ANIM_CHANGE *const change =
                    Anim_GetChange(cached_info.anims.change_count + j);
                change->range_idx += cached_info.anims.range_count;
            }
            break;
        }

        case IDT_ANIM_COMMANDS: {
            Level_Section_AppendAnimCommands(
                level_info->anims.command_count, data_count,
                chunk.injection->fp);
            level_info->anims.command_count += data_count;
            break;
        }

        case IDT_ANIM_FRAMES: {
            Level_Section_AppendAnimFrames(
                level_info->anims.frame_count, data_count, chunk.injection->fp);
            level_info->anims.frame_count += data_count;
            break;
        }

        case IDT_ANIM_RANGES: {
            Level_Section_AppendAnimRanges(
                level_info->anims.range_count, data_count, chunk.injection->fp);
            level_info->anims.range_count += data_count;

            for (int32_t j = 0; j < data_count; j++) {
                ANIM_RANGE *const range =
                    Anim_GetRange(cached_info.anims.range_count + j);
                range->link_anim_num += cached_info.anims.anim_count;
            }
            break;
        }

        default:
            LOG_WARNING("Unknown data type: %d", data_type);
            VFile_Skip(chunk.injection->fp, data_size);
            break;
        }
    }
}

static void M_CommandEdits(
    const INJECTION_CONTEXT *const ctx, const INJECTION *const injection,
    const int32_t data_count)
{
    const LEVEL_CONTEXT_INFO cached_info = Inject_GetCachedInfo();
    int16_t cmd_idx = cached_info.anims.command_count;
    for (int32_t i = 0; i < data_count; i++) {
        const INJECTION_OBJECT_INFO obj_info = Inject_ReadObjectPtr(injection);
        const int32_t anim_idx = VFile_ReadS32(injection->fp);
        const int32_t num_raw_cmds = VFile_ReadS32(injection->fp);
        const int32_t num_anim_cmds = VFile_ReadS32(injection->fp);

        const OBJECT *const obj = Object_Get(obj_info.id);
        if (ctx->mode == INJECTION_MODE_STATS || !obj->loaded) {
            continue;
        }

        ANIM *const anim = Object_GetAnim(obj, anim_idx);
        anim->command_idx = cmd_idx;
        anim->num_commands = num_anim_cmds;
        cmd_idx += num_raw_cmds;
    }
}

REGISTER_INJECTOR(ICT_ANIMATION_DATA, M_HandleAnimData)
REGISTER_INJECT_EDITOR(IDT_ANIM_CMD_EDITS, M_CommandEdits)
