#include <trx/core/memory.h>
#include <trx/debug.h>
#include <trx/game/inject.h>
#include <trx/game/level/context.h>
#include <trx/game/level/sections/append.h>

static void M_HandleMeshData(
    const INJECTION_CONTEXT *const ctx, const INJECTION_CHUNK chunk)
{
    int32_t mesh_ptr_count = 0;
    int32_t *mesh_indices = nullptr;
    const size_t chunk_start_pos = VFile_GetPos(chunk.injection->fp);

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
        case IDT_MESH_POINTERS: {
            const int32_t alloc_size = data_count * sizeof(int32_t);
            mesh_indices = Memory_Alloc(alloc_size);
            VFile_Read(chunk.injection->fp, mesh_indices, alloc_size);
            mesh_ptr_count = data_count;
            break;
        }

        case IDT_OBJECT_MESHES: {
            ASSERT(mesh_indices != nullptr);
            Level_Section_AppendObjectMeshes(
                mesh_ptr_count, mesh_indices, chunk.injection->fp);
            LEVEL_CONTEXT_INFO *const info = Level_Context_GetInfo();
            info->mesh_ptr_count += mesh_ptr_count;
            break;
        }

        default:
            LOG_WARNING("Unknown data type: %d", data_type);
            VFile_Skip(chunk.injection->fp, data_size);
            break;
        }
    }

    Memory_FreePointer(&mesh_indices);

    // Not all mesh data is necessarily read, so ensure to move to the end.
    VFile_SetPos(chunk.injection->fp, chunk_start_pos + chunk.total_size);
}

REGISTER_INJECTOR(ICT_MESH_DATA, M_HandleMeshData)
