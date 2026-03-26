#include <trx/core/benchmark.h>
#include <trx/game/const.h>
#include <trx/game/level/format/format.h>
#include <trx/game/level/sections/read.h>
#include <trx/game/pathing.h>

#include <string.h>

void Level_Section_ReadPathingData(LEVEL_CONTEXT *const ctx, VFILE *const file)
{
    BENCHMARK benchmark = Benchmark_Start();
    const LEVEL_FORMAT_LOADER *const loader = ctx->loader;
    const int32_t num_boxes = VFile_ReadS32(file);
    Box_InitialiseBoxes(num_boxes);
    for (int32_t i = 0; i < num_boxes; i++) {
        BOX_INFO *const box = Box_GetBox(i);
        if (loader->game_version == 1) {
            box->left = VFile_ReadS32(file);
            box->right = VFile_ReadS32(file);
            box->top = VFile_ReadS32(file);
            box->bottom = VFile_ReadS32(file);
        } else {
            box->left = VFile_ReadU8(file) << WALL_SHIFT;
            box->right = (VFile_ReadU8(file) << WALL_SHIFT) - 1;
            box->top = VFile_ReadU8(file) << WALL_SHIFT;
            box->bottom = (VFile_ReadU8(file) << WALL_SHIFT) - 1;
        }
        box->height = VFile_ReadS16(file);
        box->overlap_index = VFile_ReadS16(file);
        if (loader->game_version == 3
            && (box->overlap_index & BOX_BLOCKABLE) != 0) {
            box->overlap_index |= BOX_BLOCKED;
        }
    }

    const int32_t num_overlaps = VFile_ReadS32(file);
    int16_t *const overlaps = Box_InitialiseOverlaps(num_overlaps);
    VFile_Read(file, overlaps, sizeof(int16_t) * num_overlaps);

    for (int32_t flip_status = 0; flip_status < 2; flip_status++) {
        for (int32_t zone_idx = 0; zone_idx < Box_GetZoneCount(); zone_idx++) {
            int16_t *const ground_zone =
                Box_GetGroundZone(flip_status, zone_idx);
            VFile_Read(file, ground_zone, sizeof(int16_t) * num_boxes);

            if (loader->game_version == 1 && zone_idx == 1) {
                // TODO: remove once TombEditor is updated to generate the same
                // number of zones as TR2 via injections. This allows enemies of
                // LOT_SETUP_CLIMBER type to safely be used in TR1 in the
                // meantime.
                int16_t *const duped_zone = Box_GetGroundZone(flip_status, 3);
                memcpy(duped_zone, ground_zone, sizeof(int16_t) * num_boxes);
            }
        }

        int16_t *const fly_zone = Box_GetFlyZone(flip_status);
        VFile_Read(file, fly_zone, sizeof(int16_t) * num_boxes);
    }

    Benchmark_End(&benchmark, nullptr);
}
