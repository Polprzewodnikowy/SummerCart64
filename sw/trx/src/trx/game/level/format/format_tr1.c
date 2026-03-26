#include <trx/core/log.h>
#include <trx/game/inject.h>
#include <trx/game/level/context.h>
#include <trx/game/level/format/priv.h>
#include <trx/game/level/sections/read.h>
#include <trx/game/output/const.h>

#define M_SAMPLE_COUNT 256

static bool M_Probe(
    const LEVEL_FORMAT_LOADER *const loader, VFILE *const file,
    const LEVEL_FORMAT_PROBE_MODE mode)
{
    VFile_SetPos(file, 0);
    LEVEL_CONTEXT probe_ctx = {
        .loader = loader,
    };

    int32_t version;
    LEVEL_FORMAT_TRY_OR_FAIL(VFile_TryReadS32(file, &version));
    if (version != 32) {
        return false;
    }

    LEVEL_FORMAT_SKIP_ARR_S32_OR_FAIL(TEXTURE_PAGE_SIZE); // textures
    LEVEL_FORMAT_SKIP_OR_FAIL(4);

    if (mode == LEVEL_FORMAT_PROBE_MINIMAL) {
        uint16_t room_count;
        LEVEL_FORMAT_TRY_OR_FAIL(VFile_TryReadU16(file, &room_count));
        for (int32_t i = 0; i < room_count; i++) {
            LEVEL_FORMAT_SKIP_OR_FAIL(16);
            LEVEL_FORMAT_SKIP_ARR_S32_OR_FAIL(2); // meshes
            LEVEL_FORMAT_SKIP_ARR_U16_OR_FAIL(32); // portals

            int16_t size_z;
            int16_t size_x;
            LEVEL_FORMAT_TRY_OR_FAIL(VFile_TryReadS16(file, &size_z));
            LEVEL_FORMAT_TRY_OR_FAIL(VFile_TryReadS16(file, &size_x));
            LEVEL_FORMAT_SKIP_OR_FAIL(size_z * size_x * 8);
            LEVEL_FORMAT_SKIP_OR_FAIL(2);

            LEVEL_FORMAT_SKIP_ARR_U16_OR_FAIL(18); // lights
            LEVEL_FORMAT_SKIP_ARR_U16_OR_FAIL(18); // static meshes
            LEVEL_FORMAT_SKIP_OR_FAIL(4);
        }

        LEVEL_FORMAT_SKIP_ARR_S32_OR_FAIL(2); // floor data
    } else {
        Level_Section_ReadRooms(&probe_ctx, file);
    }

    LEVEL_FORMAT_SKIP_ARR_S32_OR_FAIL(2); // object meshes
    LEVEL_FORMAT_SKIP_ARR_S32_OR_FAIL(4); // object mesh pointers
    LEVEL_FORMAT_SKIP_ARR_S32_OR_FAIL(32); // animations
    LEVEL_FORMAT_SKIP_ARR_S32_OR_FAIL(6); // animation changes
    LEVEL_FORMAT_SKIP_ARR_S32_OR_FAIL(8); // animation ranges
    LEVEL_FORMAT_SKIP_ARR_S32_OR_FAIL(2); // animation commands
    LEVEL_FORMAT_SKIP_ARR_S32_OR_FAIL(4); // animation bones
    LEVEL_FORMAT_SKIP_ARR_S32_OR_FAIL(2); // animation frames

    if (mode == LEVEL_FORMAT_PROBE_MINIMAL) {
        LEVEL_FORMAT_SKIP_ARR_S32_OR_FAIL(18); // objects
    } else {
        Level_Section_ReadObjects(&probe_ctx, file);
    }

    LEVEL_FORMAT_SKIP_ARR_S32_OR_FAIL(32); // static objects
    LEVEL_FORMAT_SKIP_ARR_S32_OR_FAIL(20); // textures
    LEVEL_FORMAT_SKIP_ARR_S32_OR_FAIL(16); // sprites
    LEVEL_FORMAT_SKIP_ARR_S32_OR_FAIL(8); // sprites sequences

    if (loader->layout == LEVEL_FORMAT_LAYOUT_TR1_DEMO_PC) {
        LEVEL_FORMAT_SKIP_OR_FAIL(768); // palette
    }

    LEVEL_FORMAT_SKIP_ARR_S32_OR_FAIL(16); // cameras
    LEVEL_FORMAT_SKIP_ARR_S32_OR_FAIL(16); // sound effects

    int32_t box_count;
    LEVEL_FORMAT_TRY_OR_FAIL(VFile_TryReadS32(file, &box_count));
    LEVEL_FORMAT_SKIP_OR_FAIL(box_count * 20);
    LEVEL_FORMAT_SKIP_ARR_S32_OR_FAIL(2); // overlaps
    LEVEL_FORMAT_SKIP_OR_FAIL(box_count * 12); // zones

    LEVEL_FORMAT_SKIP_ARR_S32_OR_FAIL(2); // animated texture ranges

    if (mode == LEVEL_FORMAT_PROBE_MINIMAL) {
        LEVEL_FORMAT_SKIP_ARR_S32_OR_FAIL(22); // items
    } else {
        Level_Section_ReadItems(&probe_ctx, file);
    }

    LEVEL_FORMAT_SKIP_OR_FAIL(32 * 256); // light table

    if (loader->layout != LEVEL_FORMAT_LAYOUT_TR1_DEMO_PC) {
        LEVEL_FORMAT_SKIP_OR_FAIL(768); // palette
    }

    LEVEL_FORMAT_SKIP_ARR_U16_OR_FAIL(16); // cinematic frames
    LEVEL_FORMAT_SKIP_ARR_U16_OR_FAIL(1); // demo data

    LEVEL_FORMAT_SKIP_OR_FAIL(2 * M_SAMPLE_COUNT); // sample lut
    LEVEL_FORMAT_SKIP_ARR_S32_OR_FAIL(8); // sample infos
    LEVEL_FORMAT_SKIP_ARR_S32_OR_FAIL(1); // sample data
    LEVEL_FORMAT_SKIP_ARR_S32_OR_FAIL(4); // samples

    if (loader->layout == LEVEL_FORMAT_LAYOUT_TR1X) {
        uint32_t inj_magic;
        LEVEL_FORMAT_TRY_OR_FAIL(VFile_TryReadU32(file, &inj_magic));
        LEVEL_FORMAT_TRY_OR_FAIL(inj_magic == INJECTION_MAGIC);
    }

    return true;
}

static bool M_Load(const LEVEL_FORMAT_LOADER *const loader, VFILE *const file)
{
    LEVEL_CONTEXT *const ctx = Level_Context_Get();
    VFile_SetPos(file, 4);

    // Read texture pages once the palette is available.
    const int32_t num_pages = VFile_ReadS32(file);
    VFile_Skip(file, num_pages * TEXTURE_PAGE_SIZE * sizeof(uint8_t));

    const int32_t file_level_num = VFile_ReadS32(file);
    LOG_INFO("file level num: %d", file_level_num);

    Level_Section_ReadRooms(ctx, file);
    Level_Section_ReadObjectMeshes(ctx, file);
    Level_Section_ReadAnims(ctx, file);
    Level_Section_ReadAnimChanges(ctx, file);
    Level_Section_ReadAnimRanges(ctx, file);
    Level_Section_ReadAnimCommands(ctx, file);
    Level_Section_ReadAnimBones(ctx, file);
    Level_Section_ReadAnimFrames(ctx, file);
    Level_Section_ReadObjects(ctx, file);
    Level_Section_ReadStaticObjects(ctx, file);
    Level_Section_ReadObjectTextures(ctx, file);
    Level_Section_ReadSpriteTextures(ctx, file);
    Level_Section_ReadSpriteSequences(ctx, file);

    if (loader->layout == LEVEL_FORMAT_LAYOUT_TR1_DEMO_PC) {
        Level_Section_ReadPalettes(ctx, file);
    }

    Level_Section_ReadCamerasAndSinks(ctx, file);
    Level_Section_ReadSoundSources(ctx, file);
    Level_Section_ReadPathingData(ctx, file);
    Level_Section_ReadAnimatedTextureRanges(ctx, file);
    Level_Section_ReadItems(ctx, file);
    Level_Section_ReadLightMap(ctx, file);

    if (loader->layout != LEVEL_FORMAT_LAYOUT_TR1_DEMO_PC) {
        Level_Section_ReadPalettes(ctx, file);
    }

    Level_Section_ReadCinematicFrames(ctx, file);
    Level_Section_ReadDemoData(ctx, file);
    Level_Section_ReadSamples(ctx, file);

    if (loader->layout == LEVEL_FORMAT_LAYOUT_TR1X) {
        VFILE *const embedded_injection = VFile_CreateFromBuffer(
            file->cur_ptr, file->size - VFile_GetPos(file));
        Inject_AppendInjection(embedded_injection);
    }

    VFile_SetPos(file, 4);
    Level_Section_ReadTexturePages(ctx, file);

    return true;
}

static const LEVEL_FORMAT_LOADER m_LevelLoaderTR1 = {
    .game_version = 1,
    .layout = LEVEL_FORMAT_LAYOUT_TR1,
    .probe = M_Probe,
    .load = M_Load,
};

static const LEVEL_FORMAT_LOADER m_LevelLoaderTR1DemoPC = {
    .game_version = 1,
    .layout = LEVEL_FORMAT_LAYOUT_TR1_DEMO_PC,
    .probe = M_Probe,
    .load = M_Load,
};

static const LEVEL_FORMAT_LOADER m_LevelLoaderTR1X = {
    .game_version = 1,
    .layout = LEVEL_FORMAT_LAYOUT_TR1X,
    .probe = M_Probe,
    .load = M_Load,
};

REGISTER_LEVEL_FORMAT_LOADER(100, m_LevelLoaderTR1X)
REGISTER_LEVEL_FORMAT_LOADER(110, m_LevelLoaderTR1)
REGISTER_LEVEL_FORMAT_LOADER(120, m_LevelLoaderTR1DemoPC)
