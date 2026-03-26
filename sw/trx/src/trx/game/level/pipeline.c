#include <trx/config.h>
#include <trx/core/benchmark.h>
#include <trx/core/filesystem.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/game/inject.h>
#include <trx/game/items/carrier.h>
#include <trx/game/level.h>
#include <trx/game/level/format/format.h>
#include <trx/game/objects.h>
#include <trx/game/objects/creatures/mutant.h>
#include <trx/game/rooms.h>
#include <trx/game/shell/paths.h>
#include <trx/game/sound.h>
#include <trx/game/stats.h>
#include <trx/version.h>

#include <stdlib.h>
#include <string.h>

typedef struct {
    int32_t game_index;
    int32_t file_index;
} M_SAMPLE_ENTRY;

static int32_t M_CompareSampleOffsets(const void *const a, const void *const b)
{
    const M_SAMPLE_ENTRY *const entry_a = (M_SAMPLE_ENTRY *)a;
    const M_SAMPLE_ENTRY *const entry_b = (M_SAMPLE_ENTRY *)b;
    return entry_a->file_index - entry_b->file_index;
}

static void M_InitialiseSamplesFromFile(
    LEVEL_CONTEXT *const ctx, const char *file_name)
{
    BENCHMARK benchmark = Benchmark_Start();
    M_SAMPLE_ENTRY *entries = nullptr;
    LEVEL_CONTEXT_INFO *const info = &ctx->info;

    if (file_name == nullptr) {
        file_name = g_GameFlow.settings.sfx_path;
    }
    if (file_name == nullptr) {
        file_name = "main.sfx";
    }
    MYFILE *fp = nullptr;
    const char *const full_path =
        TRXPath_PeekResolve(TRX_DYNAMIC_PATH_SFX_FILE, file_name);
    if (full_path == nullptr) {
        goto finish;
    }

    fp = File_Open(full_path, FILE_OPEN_READ);
    if (fp == nullptr) {
        LOG_ERROR("Could not open %s samples file", full_path);
        goto finish;
    }
    LOG_DEBUG("Loading samples from %s", full_path);

    const int32_t sample_count = info->samples.offset_count;
    entries = Memory_Alloc(sizeof(M_SAMPLE_ENTRY) * sample_count);
    for (int32_t i = 0; i < sample_count; i++) {
        entries[i].game_index = i;
        entries[i].file_index = info->samples.offsets[i];
    }
    qsort(
        entries, sample_count, sizeof(M_SAMPLE_ENTRY), M_CompareSampleOffsets);

    for (int32_t i = 0, current_sample = 0; current_sample < sample_count;
         i++) {
        uint32_t header[11] = {};
        File_ReadData(fp, header, 11 * sizeof(uint32_t));
        if (header[0] != MKTAG('R', 'I', 'F', 'F')
            || header[2] != MKTAG('W', 'A', 'V', 'E')
            || header[9] != MKTAG('d', 'a', 't', 'a')) {
            LOG_ERROR("Unexpected sample header for sample %d", i);
            goto finish;
        }

        const size_t header_size = 11 * sizeof(uint32_t);
        const size_t aligned_size = (header[10] + 1) & ~1;
        const size_t size = aligned_size + header_size;
        const M_SAMPLE_ENTRY *const entry = &entries[current_sample];
        if (entry->file_index != i) {
            File_Seek(fp, aligned_size, FILE_SEEK_CUR);
            continue;
        }

        char *sample_data = Memory_Alloc(size);
        memcpy(sample_data, header, header_size);
        File_ReadData(fp, sample_data + header_size, aligned_size);
        Sound_LoadSampleData(entry->game_index, sample_data, size);
        Memory_FreePointer(&sample_data);

        current_sample++;
    }

finish:
    if (fp != nullptr) {
        File_Close(fp);
    }
    Memory_FreePointer(&entries);
    Memory_FreePointer(&info->samples.offsets);
    Benchmark_End(&benchmark, nullptr);
}

static void M_InitialiseSamplesFromLevelInfo(LEVEL_CONTEXT *const ctx)
{
    BENCHMARK benchmark = Benchmark_Start();
    LEVEL_CONTEXT_INFO *const info = &ctx->info;
    const int32_t sample_count = info->samples.offset_count;

    // TODO: this assumes that sample pointers are sorted - adopt TR2's approach
    // of sorting by index, verifying WAV headers and using WAV sample size.
    for (int32_t i = 0; i < sample_count; i++) {
        const int32_t current_offset = info->samples.offsets[i];
        const int32_t next_offset = i + 1 >= sample_count
            ? info->samples.data_size
            : info->samples.offsets[i + 1];

        const char *const sample_data = &info->samples.data[current_offset];
        const size_t sample_size = next_offset - current_offset;
        Sound_LoadSampleData(i, sample_data, sample_size);
    }

    Memory_FreePointer(&info->samples.offsets);
    Benchmark_End(&benchmark, nullptr);
}

static void M_MarkWaterEdgeVertices(void)
{
    if (!g_Config.visuals.fix_texture_issues) {
        return;
    }

    BENCHMARK benchmark = Benchmark_Start();
    for (int32_t i = 0; i < Room_GetCount(); i++) {
        const ROOM *const room = Room_Get(i);
        const int32_t y_test =
            room->flags.underwater ? room->max_ceiling : room->min_floor;
        for (int32_t j = 0; j < room->mesh.num_vertices; j++) {
            ROOM_VERTEX *const vertex = &room->mesh.vertices[j];
            if (vertex->pos.y == y_test) {
                vertex->flags.disable_wibble = true;
            }
        }
    }

    Benchmark_End(&benchmark, nullptr);
}

static void M_CompleteSetup(
    const LEVEL_FORMAT_LOADER *const loader, const GF_LEVEL *const level)
{
    BENCHMARK benchmark = Benchmark_Start();
    LEVEL_CONTEXT *const ctx = Level_Context_Get();

    // We inject explosions sprites and sounds, although in the original game,
    // some levels lack them, resulting in no audio or visual effects when
    // killing mutants. This is to maintain that feature.
    Mutant_ToggleExplosions(Object_Get(O_EXPLOSION_1)->loaded);

    Inject_AllInjections();

    Level_Finalize_LoadAnimFrames(ctx);
    Level_Finalize_LoadAnimCommands(ctx);
    if (g_TRVersion == 1) {
        M_MarkWaterEdgeVertices();
    }
    Level_Finalize_LoadObjectsAndItems(ctx);

    // Configure enemies who carry and drop items
    Carrier_InitialiseLevel(level);

    Level_Finalize_LoadRooms(ctx);
    Level_Finalize_LoadTextures(ctx);
    Level_Finalize_LoadTexturePages(ctx);
    Level_Finalize_LoadPalettes(ctx);

    if (loader->game_version == 1) {
        M_InitialiseSamplesFromLevelInfo(ctx);
    } else {
        M_InitialiseSamplesFromFile(ctx, level->settings.sfx_path);
    }

    Benchmark_End(&benchmark, nullptr);
}

void Level_Pipeline_Load(const GF_LEVEL *const level)
{
    LOG_INFO("%d (%s)", level->num, level->path);
    BENCHMARK benchmark = Benchmark_Start();

    Inject_InitLevel(level, INJECTION_MODE_FULL);
    const LEVEL_FORMAT_LOADER *const loader = Level_Format_LoadFromFile(level);
    M_CompleteSetup(loader, level);
    Inject_Cleanup();

    Benchmark_End(&benchmark, nullptr);
}
