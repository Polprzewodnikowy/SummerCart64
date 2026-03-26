#include <trx/core/log.h>
#include <trx/game/inject.h>

static void (*m_Handlers[IDT_NUMBER_OF])(
    const INJECTION_CONTEXT *ctx, const INJECTION *injection,
    int32_t element_count) = {};

static void M_HandleDataEdits(
    const INJECTION_CONTEXT *const ctx, const INJECTION_CHUNK chunk)
{
    for (int32_t i = 0; i < chunk.num_blocks; i++) {
        const INJECTION_DATA_TYPE data_type =
            VFile_ReadS32(chunk.injection->fp);
        const int32_t data_count = VFile_ReadS32(chunk.injection->fp);
        const int32_t data_size = VFile_ReadS32(chunk.injection->fp);

        if (m_Handlers[data_type] == nullptr) {
            if (data_type != IDT_ROOM_EDIT_META) {
                LOG_WARNING("Unknown data type: %d", data_type);
            }
            VFile_Skip(chunk.injection->fp, data_size);
        } else {
            m_Handlers[data_type](ctx, chunk.injection, data_count);
        }
    }
}

void Inject_RegisterEditor(
    const INJECTION_DATA_TYPE type,
    void (*handle_func)(
        const INJECTION_CONTEXT *ctx, const INJECTION *injection,
        int32_t element_count))
{
    m_Handlers[type] = handle_func;
}

REGISTER_INJECTOR(ICT_DATA_EDITS, M_HandleDataEdits)
