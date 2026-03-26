#include <trx/game/inject/types.h>

void Inject_RegisterEditor(
    INJECTION_DATA_TYPE type,
    void (*handle_func)(
        const INJECTION_CONTEXT *ctx, const INJECTION *injection,
        int32_t element_count));

#define REGISTER_INJECT_EDITOR(data_type, handle_func)                         \
    __attribute__((constructor)) static void                                   \
    M_RegisterInjectEditor##data_type(void)                                    \
    {                                                                          \
        Inject_RegisterEditor(data_type, handle_func);                         \
    }
