#include <trx/core/benchmark.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/game/anims.h>
#include <trx/game/inject.h>
#include <trx/game/level/sections/append.h>
#include <trx/game/level/sections/read.h>

static void M_ReadPosition(XYZ_32 *const pos, VFILE *const file)
{
    pos->x = VFile_ReadS32(file);
    pos->y = VFile_ReadS32(file);
    pos->z = VFile_ReadS32(file);
}

void Level_Section_ReadAnims(LEVEL_CONTEXT *const ctx, VFILE *const file)
{
    BENCHMARK benchmark = Benchmark_Start();
    const int32_t num_anims = VFile_ReadS32(file);
    LEVEL_CONTEXT_INFO *const info = &ctx->info;
    info->anims.anim_count = num_anims;
    LOG_INFO("anims: %d", num_anims);
    Anim_InitialiseAnims(num_anims + Inject_GetDataCount(IDT_ANIMS));
    Level_Section_AppendAnims(0, num_anims, file);
    Benchmark_End(&benchmark, nullptr);
}

void Level_Section_AppendAnims(
    const int32_t base_idx, const int32_t num_anims, VFILE *const file)
{
    for (int32_t i = 0; i < num_anims; i++) {
        ANIM *const anim = Anim_GetAnim(base_idx + i);
        anim->frame_ofs = VFile_ReadU32(file);
        anim->frame_ptr = nullptr; // filled later by the animation frame loader
        anim->interpolation = VFile_ReadU8(file);
        anim->frame_size = VFile_ReadU8(file);
        anim->current_anim_state = VFile_ReadS16(file);
        anim->velocity = VFile_ReadS32(file);
        anim->acceleration = VFile_ReadS32(file);
        anim->frame_base = VFile_ReadS16(file);
        anim->frame_end = VFile_ReadS16(file);
        anim->jump_anim_num = VFile_ReadS16(file);
        anim->jump_frame_num = VFile_ReadS16(file);
        anim->num_changes = VFile_ReadS16(file);
        anim->change_idx = VFile_ReadS16(file);
        anim->num_commands = VFile_ReadS16(file);
        anim->command_idx = VFile_ReadS16(file);
    }
}

void Level_Section_ReadAnimChanges(LEVEL_CONTEXT *const ctx, VFILE *const file)
{
    BENCHMARK benchmark = Benchmark_Start();
    const int32_t num_anim_changes = VFile_ReadS32(file);
    LEVEL_CONTEXT_INFO *const info = &ctx->info;
    info->anims.change_count = num_anim_changes;
    LOG_INFO("anim changes: %d", num_anim_changes);
    Anim_InitialiseChanges(
        num_anim_changes + Inject_GetDataCount(IDT_ANIM_CHANGES));
    Level_Section_AppendAnimChanges(0, num_anim_changes, file);
    Benchmark_End(&benchmark, nullptr);
}

void Level_Section_AppendAnimChanges(
    const int32_t base_idx, const int32_t num_changes, VFILE *const file)
{
    for (int32_t i = 0; i < num_changes; i++) {
        ANIM_CHANGE *const anim_change = Anim_GetChange(base_idx + i);
        anim_change->goal_anim_state = VFile_ReadS16(file);
        anim_change->num_ranges = VFile_ReadS16(file);
        anim_change->range_idx = VFile_ReadS16(file);
    }
}

void Level_Section_ReadAnimRanges(LEVEL_CONTEXT *const ctx, VFILE *const file)
{
    BENCHMARK benchmark = Benchmark_Start();
    const int32_t num_anim_ranges = VFile_ReadS32(file);
    LEVEL_CONTEXT_INFO *const info = &ctx->info;
    info->anims.range_count = num_anim_ranges;
    LOG_INFO("anim ranges: %d", num_anim_ranges);
    Anim_InitialiseRanges(
        num_anim_ranges + Inject_GetDataCount(IDT_ANIM_RANGES));
    Level_Section_AppendAnimRanges(0, num_anim_ranges, file);
    Benchmark_End(&benchmark, nullptr);
}

void Level_Section_AppendAnimRanges(
    const int32_t base_idx, const int32_t num_ranges, VFILE *const file)
{
    for (int32_t i = 0; i < num_ranges; i++) {
        ANIM_RANGE *const anim_range = Anim_GetRange(base_idx + i);
        anim_range->start_frame = VFile_ReadS16(file);
        anim_range->end_frame = VFile_ReadS16(file);
        anim_range->link_anim_num = VFile_ReadS16(file);
        anim_range->link_frame_num = VFile_ReadS16(file);
    }
}

void Level_Section_ReadAnimCommands(LEVEL_CONTEXT *const ctx, VFILE *const file)
{
    BENCHMARK benchmark = Benchmark_Start();
    const int32_t num_commands = VFile_ReadS32(file);
    LEVEL_CONTEXT_INFO *const info = &ctx->info;
    info->anims.command_count = num_commands;
    LOG_INFO("anim commands: %d", num_commands);
    info->anims.commands = Memory_Alloc(
        sizeof(int16_t)
        * (num_commands + Inject_GetDataCount(IDT_ANIM_COMMANDS)));
    Level_Section_AppendAnimCommands(0, num_commands, file);
    Benchmark_End(&benchmark, nullptr);
}

void Level_Section_AppendAnimCommands(
    const int32_t base_idx, const int32_t num_commands, VFILE *const file)
{
    LEVEL_CONTEXT_INFO *const info = Level_Context_GetInfo();
    VFile_Read(
        file, &info->anims.commands[base_idx], sizeof(int16_t) * num_commands);
}

void Level_Section_ReadAnimBones(LEVEL_CONTEXT *const ctx, VFILE *const file)
{
    BENCHMARK benchmark = Benchmark_Start();
    const int32_t num_anim_bones = VFile_ReadS32(file) / ANIM_BONE_SIZE;
    LEVEL_CONTEXT_INFO *const info = &ctx->info;
    info->anims.bone_count = num_anim_bones;
    LOG_INFO("anim bones: %d", num_anim_bones);
    Anim_InitialiseBones(num_anim_bones + Inject_GetDataCount(IDT_ANIM_BONES));
    Level_Section_AppendAnimBones(0, num_anim_bones, file);
    Benchmark_End(&benchmark, nullptr);
}

void Level_Section_AppendAnimBones(
    const int32_t base_idx, const int32_t num_bones, VFILE *const file)
{
    for (int32_t i = 0; i < num_bones; i++) {
        ANIM_BONE *const bone = Anim_GetBone(base_idx + i);
        const int32_t flags = VFile_ReadS32(file);
        bone->matrix_pop = (flags & 1) != 0;
        bone->matrix_push = (flags & 2) != 0;
        bone->rot.x = false;
        bone->rot.y = false;
        bone->rot.z = false;
        M_ReadPosition(&bone->pos, file);
    }
}

void Level_Section_ReadAnimFrames(LEVEL_CONTEXT *const ctx, VFILE *const file)
{
    BENCHMARK benchmark = Benchmark_Start();
    const int32_t raw_data_count = VFile_ReadS32(file);
    LEVEL_CONTEXT_INFO *const info = &ctx->info;
    info->anims.frame_count = raw_data_count;
    LOG_INFO("raw anim frames: %d", raw_data_count);
    info->anims.frames = Memory_Alloc(
        sizeof(int16_t)
        * (raw_data_count + Inject_GetDataCount(IDT_ANIM_FRAMES)));
    Level_Section_AppendAnimFrames(0, raw_data_count, file);
    Benchmark_End(&benchmark, nullptr);
}

void Level_Section_AppendAnimFrames(
    const int32_t base_idx, const int32_t num_frames, VFILE *const file)
{
    LEVEL_CONTEXT_INFO *const info = Level_Context_GetInfo();
    VFile_Read(
        file, &info->anims.frames[base_idx], sizeof(int16_t) * num_frames);
}
