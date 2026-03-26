#include <trx/core/benchmark.h>
#include <trx/core/log.h>
#include <trx/core/utils.h>
#include <trx/game/camera.h>
#include <trx/game/demo.h>
#include <trx/game/inject.h>
#include <trx/game/level/sections/read.h>
#include <trx/game/sound.h>

static void M_ReadPosition(XYZ_32 *const pos, VFILE *const file)
{
    pos->x = VFile_ReadS32(file);
    pos->y = VFile_ReadS32(file);
    pos->z = VFile_ReadS32(file);
}

static void M_ReadVertex(XYZ_16 *const vertex, VFILE *const file)
{
    vertex->x = VFile_ReadS16(file);
    vertex->y = VFile_ReadS16(file);
    vertex->z = VFile_ReadS16(file);
}

static void M_ReadObjectVector(OBJECT_VECTOR *const obj, VFILE *const file)
{
    M_ReadPosition(&obj->pos, file);
    obj->data = VFile_ReadS16(file);
    obj->flags = VFile_ReadS16(file);
}

void Level_Section_ReadCinematicFrames(
    LEVEL_CONTEXT *const ctx, VFILE *const file)
{
    BENCHMARK benchmark = Benchmark_Start();
    const int16_t num_frames = VFile_ReadS16(file);
    const int32_t inj_frames = Inject_GetDataCount(IDT_CINEMATIC_FRAMES);
    LOG_INFO("cinematic frames: %d", num_frames);
    Camera_InitialiseCineFrames(MAX(num_frames, inj_frames));
    for (int32_t i = 0; i < num_frames; i++) {
        CINE_FRAME *const frame = Camera_GetCineFrame(i);
        M_ReadVertex(&frame->target.shift, file);
        M_ReadVertex(&frame->camera.shift, file);
        frame->fov = VFile_ReadS16(file);
        frame->roll = VFile_ReadS16(file);
    }

    Benchmark_End(&benchmark, nullptr);
}

void Level_Section_ReadCamerasAndSinks(
    LEVEL_CONTEXT *const ctx, VFILE *const file)
{
    BENCHMARK benchmark = Benchmark_Start();
    const int32_t num_objects = VFile_ReadS32(file);
    LOG_DEBUG("fixed cameras/sinks: %d", num_objects);
    Camera_InitialiseFixedObjects(num_objects);
    for (int32_t i = 0; i < num_objects; i++) {
        M_ReadObjectVector(Camera_GetFixedObject(i), file);
    }

    Benchmark_End(&benchmark, nullptr);
}

void Level_Section_ReadDemoData(LEVEL_CONTEXT *const ctx, VFILE *const file)
{
    BENCHMARK benchmark = Benchmark_Start();
    const uint16_t size = VFile_ReadU16(file);
    LOG_INFO("demo buffer size: %d", size);
    Demo_LoadData(file, size);
    Benchmark_End(&benchmark, nullptr);
}

void Level_Section_ReadSoundSources(LEVEL_CONTEXT *const ctx, VFILE *const file)
{
    BENCHMARK benchmark = Benchmark_Start();
    const int32_t num_sources = VFile_ReadS32(file);
    LOG_INFO("sound sources: %d", num_sources);
    Sound_InitialiseSources(num_sources);
    for (int32_t i = 0; i < num_sources; i++) {
        M_ReadObjectVector(Sound_GetSource(i), file);
    }

    Benchmark_End(&benchmark, nullptr);
}
