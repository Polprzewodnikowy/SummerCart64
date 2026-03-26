#include <trx/game/inject.h>
#include <trx/game/objects.h>

static void M_ObjectTypeEdits(
    const INJECTION_CONTEXT *const ctx, const INJECTION *const injection,
    const int32_t data_count)
{
    for (int32_t i = 0; i < data_count; i++) {
        const INJECTION_OBJECT_INFO base_obj_info =
            Inject_ReadObjectPtr(injection);
        const INJECTION_OBJECT_INFO target_obj_info =
            Inject_ReadObjectPtr(injection);

        OBJECT *const base_obj = Object_TryGet(base_obj_info.id);
        const OBJECT *const target_obj = Object_TryGet(target_obj_info.id);
        if (base_obj == nullptr || target_obj == nullptr) {
            continue;
        }
        base_obj->setup_func = target_obj->setup_func;
    }
}

REGISTER_INJECT_EDITOR(IDT_OBJ_TYPE_EDITS, M_ObjectTypeEdits)
