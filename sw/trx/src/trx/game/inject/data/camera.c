#include <trx/debug.h>
#include <trx/game/camera/cinematic.h>
#include <trx/game/inject.h>

static void M_ReadVertex(XYZ_16 *const vertex, VFILE *const file)
{
    vertex->x = VFile_ReadS16(file);
    vertex->y = VFile_ReadS16(file);
    vertex->z = VFile_ReadS16(file);
}

static void M_HandleCineFrames(
    const INJECTION *const injection, const int32_t data_count)
{
    for (int32_t i = 0; i < data_count; i++) {
        CINE_FRAME *const frame = Camera_GetCineFrame(i);
        M_ReadVertex(&frame->target.shift, injection->fp);
        M_ReadVertex(&frame->camera.shift, injection->fp);
        frame->fov = VFile_ReadS16(injection->fp);
        frame->roll = VFile_ReadS16(injection->fp);
    }
}

static void M_HandleCameraData(
    const INJECTION_CONTEXT *const ctx, const INJECTION_CHUNK chunk)
{
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
        case IDT_CINEMATIC_FRAMES:
            M_HandleCineFrames(chunk.injection, data_count);
            break;
        default:
            LOG_WARNING("Unknown data type: %d", data_type);
            VFile_Skip(chunk.injection->fp, data_size);
            break;
        }
    }
}

REGISTER_INJECTOR(ICT_CAMERA_DATA, M_HandleCameraData)
