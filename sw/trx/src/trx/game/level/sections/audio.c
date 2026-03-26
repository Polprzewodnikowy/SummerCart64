#include <trx/core/benchmark.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/debug.h>
#include <trx/game/const.h>
#include <trx/game/game_buf.h>
#include <trx/game/inject.h>
#include <trx/game/level/format/format.h>
#include <trx/game/level/sections/read.h>
#include <trx/game/sound.h>

static size_t M_GetSampleCount(const LEVEL_FORMAT_LOADER *const loader)
{
    switch (loader->game_version) {
    case 1:
        return 256;
    case 2:
    case 3:
        return 370;
    default:
        ASSERT_FAIL();
    }
    return 0;
}

void Level_Section_ReadSamples(LEVEL_CONTEXT *const ctx, VFILE *const file)
{
    BENCHMARK benchmark = Benchmark_Start();
    const LEVEL_FORMAT_LOADER *const loader = ctx->loader;

    const int32_t sample_count = M_GetSampleCount(loader);
    int16_t *const sample_lut = Memory_Alloc(sizeof(int16_t) * sample_count);
    int16_t *const sample_lut_inv =
        Memory_Alloc(sizeof(int16_t) * sample_count);
    VFile_Read(file, sample_lut, sizeof(int16_t) * sample_count);
    for (int32_t i = 0; i < sample_count; i++) {
        if (sample_lut[i] != -1) {
            sample_lut_inv[sample_lut[i]] = i;
        }
    }

    const int32_t num_sample_infos = VFile_ReadS32(file);
    LOG_INFO("sample infos: %d", num_sample_infos);
    for (int32_t i = 0; i < num_sample_infos; i++) {
        SAMPLE_INFO *const sample_info =
            Sound_GetOrCreateSample(sample_lut_inv[i]);
        ASSERT(sample_info != nullptr);
        sample_info->number = VFile_ReadS16(file);

        if (loader->game_version >= 3) {
            sample_info->volume = VFile_ReadU8(file) << 7;
            sample_info->range = VFile_ReadU8(file) * WALL_L;
        } else {
            sample_info->volume = VFile_ReadU16(file);
            sample_info->range = 10 * WALL_L;
        }

        if (loader->game_version >= 3) {
            sample_info->randomness = VFile_ReadU8(file);
            sample_info->pitch = VFile_ReadS8(file);
        } else {
            sample_info->randomness = VFile_ReadU16(file);
            sample_info->pitch = 0;
        }

        sample_info->flags.all = VFile_ReadU16(file);

        Sound_ReserveSampleData(
            sample_info->number, sample_info->flags.num_samples);
        if (loader->game_version == 1) {
            switch (sample_info->flags.mode_bits) {
            case 0:
                sample_info->mode = SAMPLE_MODE_WAIT;
                break;
            case 1:
                sample_info->mode = SAMPLE_MODE_RESTART;
                break;
            case 2:
                sample_info->mode = SAMPLE_MODE_LOOPED;
                break;
            case 3:
                LOG_WARNING(
                    "Unexpected sample mode for sample %d. flags=%0X", i,
                    sample_info->flags);
                break;
            }
        } else {
            switch (sample_info->flags.mode_bits) {
            case 0:
                sample_info->mode = SAMPLE_MODE_NORMAL;
                break;
            case 1:
                sample_info->mode = SAMPLE_MODE_WAIT;
                break;
            case 2:
                sample_info->mode = SAMPLE_MODE_RESTART;
                break;
            case 3:
                sample_info->mode = SAMPLE_MODE_LOOPED;
                break;
            }
        }
    }

    LEVEL_CONTEXT_INFO *const info = &ctx->info;
    if (loader->game_version == 1) {
        const int32_t data_size = VFile_ReadS32(file);
        info->samples.data_size = data_size;
        LOG_INFO("%d sample data size", data_size);

        info->samples.data = GameBuf_Alloc(
            data_size + Inject_GetDataCount(IDT_SAMPLE_DATA), GBUF_SAMPLES);
        VFile_Read(file, info->samples.data, sizeof(char) * data_size);
    }

    const int32_t num_offsets = VFile_ReadS32(file);
    LOG_INFO("samples: %d", num_offsets);
    info->samples.offset_count = num_offsets;

    info->samples.offsets = Memory_Alloc(
        sizeof(int32_t)
        * (num_offsets + Inject_GetDataCount(IDT_SAMPLE_INDICES)));
    VFile_Read(file, info->samples.offsets, sizeof(int32_t) * num_offsets);

    Memory_Free(sample_lut);
    Memory_Free(sample_lut_inv);
    Benchmark_End(&benchmark, nullptr);
}
