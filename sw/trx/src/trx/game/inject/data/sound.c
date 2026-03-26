#include <trx/core/memory.h>
#include <trx/debug.h>
#include <trx/game/const.h>
#include <trx/game/inject.h>
#include <trx/game/sound.h>
#include <trx/version.h>

static void M_HandleSFXData(
    const INJECTION_CONTEXT *const ctx, const INJECTION_CHUNK chunk)
{
    ASSERT(chunk.num_blocks == 1);
    const INJECTION_DATA_TYPE data_type = VFile_ReadS32(chunk.injection->fp);
    ASSERT(data_type == IDT_SAMPLE_INFOS);
    const int32_t data_count = VFile_ReadS32(chunk.injection->fp);
    VFile_Skip(chunk.injection->fp, sizeof(int32_t));

    for (int32_t i = 0; i < data_count; i++) {
        const SAMPLE_ID sfx_id = VFile_ReadS16(chunk.injection->fp);

        SAMPLE_INFO *const sample_info = Sound_GetOrCreateSample(sfx_id);
        sample_info->volume = VFile_ReadS16(chunk.injection->fp);
        sample_info->randomness = VFile_ReadS16(chunk.injection->fp);
        sample_info->flags.all = VFile_ReadU16(chunk.injection->fp);
        if (chunk.injection->version >= INJ_VERSION_6) {
            sample_info->range = VFile_ReadS32(chunk.injection->fp);
            sample_info->pitch = VFile_ReadS8(chunk.injection->fp);
        } else {
            sample_info->range = 10 * WALL_L;
            sample_info->pitch = 0;
        }

        if (g_TRVersion == 1) {
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
                sample_info->mode = SAMPLE_MODE_NORMAL;
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

        const int16_t num_samples = sample_info->flags.num_samples;
        if (g_TRVersion == 1 || chunk.injection->version >= INJ_VERSION_4) {
            sample_info->number = Sound_ReserveSampleData(-1, num_samples);
            for (int32_t j = 0; j < num_samples; j++) {
                const int32_t sample_length =
                    VFile_ReadS32(chunk.injection->fp);
                char *const data = Memory_Alloc(sample_length);
                VFile_Read(chunk.injection->fp, data, sample_length);
                Sound_LoadSampleData(
                    sample_info->number + j, data, sample_length);
                Memory_Free(data);
            }
        } else if (g_TRVersion >= 2) {
            VFile_Skip(chunk.injection->fp, sizeof(int32_t));
        }
    }
}

REGISTER_INJECTOR(ICT_SFX_DATA, M_HandleSFXData)
