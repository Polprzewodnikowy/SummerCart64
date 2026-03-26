#include <trx/game/game_flow/reader.h>

#include <trx/core/enum_map.h>
#include <trx/core/filesystem.h>
#include <trx/core/json/util/file.h>
#include <trx/core/json/util/read_io.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/debug.h>
#include <trx/game/game_flow/common.h>
#include <trx/game/game_flow/types.h>
#include <trx/game/game_flow/vars.h>
#include <trx/game/inventory_ring/types.h>
#include <trx/game/lara/skin/storage.h>
#include <trx/game/objects/names.h>
#include <trx/game/shell.h>
#include <trx/version.h>

#include <string.h>

typedef struct {
    GAME_FLOW *gf;
    const char *script_path;
    JSON_READ_IO *io;
    bool validation_mode;
} M_CONTEXT;

#define M_DECLARE_SEQUENCE_EVENT_HANDLER_FUNC(name)                            \
    int32_t name(                                                              \
        const M_CONTEXT *ctx, GF_SEQUENCE_EVENT *event, void *extra_data,      \
        void *user_arg)

typedef int32_t (*M_SEQUENCE_EVENT_HANDLER_FUNC)(
    const M_CONTEXT *ctx, GF_SEQUENCE_EVENT *event, void *extra_data,
    void *user_arg);

typedef struct {
    GF_SEQUENCE_EVENT_TYPE event_type;
    M_SEQUENCE_EVENT_HANDLER_FUNC handler_func;
    void *handler_func_arg;
} M_SEQUENCE_EVENT_HANDLER;

typedef bool (*M_LOAD_ARRAY_FUNC)(
    const M_CONTEXT *ctx, void *target_elem, size_t target_elem_idx,
    void *user_arg);

static M_DECLARE_SEQUENCE_EVENT_HANDLER_FUNC(M_HandleIntEvent);
static M_DECLARE_SEQUENCE_EVENT_HANDLER_FUNC(M_HandlePictureEvent);
static M_DECLARE_SEQUENCE_EVENT_HANDLER_FUNC(M_HandleTotalStatsEvent);
static M_DECLARE_SEQUENCE_EVENT_HANDLER_FUNC(M_HandleAddItemEvent);
static M_DECLARE_SEQUENCE_EVENT_HANDLER_FUNC(M_HandleGlobeSelectEvent);

static M_SEQUENCE_EVENT_HANDLER m_SequenceEventHandlers[] = {
    // clang-format off
    // Events without arguments
    { GFS_ENABLE_SUNSET,     nullptr, nullptr },
    { GFS_REMOVE_WEAPONS,    nullptr, nullptr },
    { GFS_REMOVE_SCIONS,     nullptr, nullptr },
    { GFS_REMOVE_AMMO,       nullptr, nullptr },
    { GFS_REMOVE_FLARES,     nullptr, nullptr },
    { GFS_REMOVE_MEDIPACKS,  nullptr, nullptr },
    { GFS_LEVEL_COMPLETE,    nullptr, nullptr },
    { GFS_LEVEL_STATS,       nullptr, nullptr },
    { GFS_EXIT_TO_TITLE,     nullptr, nullptr },
    { GFS_GLOBE_SELECT,      M_HandleGlobeSelectEvent, nullptr },

    // Events with integer arguments
    { GFS_SET_START_ANIM,    M_HandleIntEvent, "anim" },
    { GFS_LOOP_GAME,         M_HandleIntEvent, "level_id" },
    { GFS_PLAY_CUTSCENE,     M_HandleIntEvent, "cutscene_id" },
    { GFS_PLAY_FMV,          M_HandleIntEvent, "fmv_id" },
    { GFS_PLAY_MUSIC,        M_HandleIntEvent, "music_track" },
    { GFS_SETUP_BACON_LARA,  M_HandleIntEvent, "anchor_room" },
    { GFS_DISABLE_FLOOR,     M_HandleIntEvent, "height" },

    // Special cases with custom handlers
    { GFS_LOADING_SCREEN,    M_HandlePictureEvent, nullptr },
    { GFS_DISPLAY_PICTURE,   M_HandlePictureEvent, nullptr },
    { GFS_TOTAL_STATS,       M_HandleTotalStatsEvent, nullptr },
    { GFS_ADD_ITEM,          M_HandleAddItemEvent, nullptr },
    { GFS_ADD_SECRET_REWARD, M_HandleAddItemEvent, nullptr },

    // Sentinel to mark the end of the table
    { (GF_SEQUENCE_EVENT_TYPE)-1, nullptr, nullptr },
    // clang-format on
};

static void M_ExitWithJSONError(const M_CONTEXT *const ctx)
{
    JSONFile_ExitWithReadIOError(
        ctx->io,
        String_FormatStatic("%s: gameflow parse error", ctx->script_path));
}

static bool M_ReadObjectID(
    const M_CONTEXT *const ctx, OBJECT_ID *const object_id_out)
{
    int32_t game_id;
    if (JSON_OPTIONAL(JSON_READ_CURRENT(ctx->io, &game_id))) {
        *object_id_out = Object_FromGameID(game_id);
    } else {
        const char *object_key;
        JSON_MUST(JSON_READ_CURRENT(ctx->io, &object_key));
        *object_id_out = Object_IdFromKey(object_key);
    }
    if (!ctx->validation_mode && *object_id_out == NO_OBJECT) {
        JSON_ReadIO_SetError(ctx->io, "'object_id' must be a valid object id");
        JSON_FAIL();
    }
    JSON_FINISH();
}

static M_SEQUENCE_EVENT_HANDLER *M_GetSequenceEventHandlers(void)
{
    return m_SequenceEventHandlers;
}

static bool M_LoadSettings(
    const M_CONTEXT *const ctx, GF_LEVEL_SETTINGS *const settings)
{
    JSON_READ_IO *const io = ctx->io;

    if (JSON_OPTIONAL(JSON_PUSH(io, "fog_start"))) {
        JSON_MUST(JSON_READ_CURRENT(io, &settings->fog_start.value));
        settings->fog_start.is_present = true;
        JSON_MUST(JSON_POP(io));
    }

    if (JSON_OPTIONAL(JSON_PUSH(io, "fog_end"))) {
        JSON_MUST(JSON_READ_CURRENT(io, &settings->fog_end.value));
        settings->fog_end.is_present = true;
        JSON_MUST(JSON_POP(io));
    }

    if (JSON_OPTIONAL(JSON_PUSH(io, "fog_transparency"))) {
        JSON_MUST(JSON_READ_CURRENT(io, &settings->fog_transparency.value));
        settings->fog_transparency.is_present = true;
        JSON_MUST(JSON_POP(io));
    }

    if (JSON_OPTIONAL(JSON_PUSH(io, "fog_color"))) {
        JSON_MUST(JSON_READ_CURRENT(io, &settings->fog_color.value));
        settings->fog_color.is_present = true;
        JSON_MUST(JSON_POP(io));
    }

    if (JSON_OPTIONAL(JSON_PUSH(io, "water_color"))) {
        JSON_MUST(JSON_READ_CURRENT(io, &settings->water_color.value));
        settings->water_color.is_present = true;
        JSON_MUST(JSON_POP(io));
    }

    if (JSON_OPTIONAL(JSON_PUSH(io, "cold_water"))) {
        JSON_MUST(JSON_READ_CURRENT(io, &settings->cold_water.value));
        settings->cold_water.is_present = true;
        JSON_MUST(JSON_POP(io));
    }

    if (JSON_OPTIONAL(JSON_PUSH(io, "death_tile"))) {
        const char *tmp_s = nullptr;
        JSON_MUST(JSON_READ_CURRENT(io, &tmp_s));
        const int32_t value =
            EnumMap_Get(ENUM_MAP_NAME(GF_DEATH_TILE), tmp_s, -1);
        if (value < 0) {
            JSON_ReadIO_SetError(io, "Invalid death_tile value '%s'", tmp_s);
            JSON_FAIL();
        }
        settings->death_tile.is_present = true;
        settings->death_tile.value = value;
        JSON_MUST(JSON_POP(io));
    }

    if (JSON_OPTIONAL(JSON_PUSH(io, "sfx_path"))) {
        const char *tmp_s = nullptr;
        JSON_MUST(JSON_READ_CURRENT(io, &tmp_s));
        settings->sfx_path = Memory_DupStr(tmp_s);
        JSON_MUST(JSON_POP(io));
    }
    JSON_FINISH();
}

// Read a "path" value that may be either a plain string or an array of
// candidate strings tried in order.
// Pass optional=true to allow the key to be absent (out_path is set to nullptr
// in that case).
// Pass path_type=(TRX_DYNAMIC_PATH)-1 to skip resolution and return the first
// candidate as-is.
static bool M_ReadPath(
    JSON_READ_IO *const io, const char *const key, const bool optional,
    const TRX_DYNAMIC_PATH path_type, char **const out_path)
{
    ASSERT(key != nullptr);
    *out_path = nullptr;
    const bool resolve = path_type != (TRX_DYNAMIC_PATH)-1;

    if (!JSON_PUSH(io, key)) {
        if (optional) {
            return true;
        }
        return false;
    }

    // All failure paths below must pop before returning to keep the
    // push/pop depth balanced and avoid corrupting the parser state.
    bool ok = false;
    JSON_ARRAY *const path_array =
        JSON_ValueAsArray(JSON_ReadIO_GetCurrentValue(io));
    const int32_t count = path_array != nullptr ? JSON_ARRAY_LEN(io) : 1;
    if (count <= 0) {
        JSON_ReadIO_SetError(io, "path array must contain at least one entry");
    } else {
        for (int32_t i = 0; i < count; i++) {
            const char *path = nullptr;
            const bool read_ok = path_array != nullptr
                ? JSON_READ_A(io, i, &path)
                : JSON_READ_CURRENT(io, &path);
            if (!read_ok) {
                break;
            }
            if (!resolve) {
                *out_path = Memory_DupStr(path);
                ok = true;
                break;
            }
            const char *const resolved = TRXPath_PeekResolve(path_type, path);
            if (resolved != nullptr) {
                *out_path = Memory_DupStr(resolved);
                ok = true;
                break;
            }
        }
        if (!ok && resolve) {
            if (optional) {
                ok = true; // leave *out_path as nullptr
            } else {
                JSON_ReadIO_SetError(
                    io, "failed to resolve any path candidate");
            }
        }
    }

    if (!JSON_POP(io)) {
        return false;
    }
    return ok;
}

static bool M_LoadLevelItemDrops(
    const M_CONTEXT *const ctx, GF_LEVEL *const level)
{
    JSON_READ_IO *const io = ctx->io;
    level->item_drops.count = 0;

    if (!JSON_OPTIONAL(JSON_PUSH(io, "item_drops"))) {
        return true;
    }

    if (ctx->gf->enable_tr2_item_drops) {
        LOG_WARNING(
            "TR2 item drops are enabled: gameflow-defined drops for level "
            "%d will be ignored",
            level->num);
        JSON_MUST(JSON_POP(io));
        return true;
    }

    level->item_drops.count = JSON_ARRAY_LEN(io);
    if (level->item_drops.count < 0) {
        JSON_FAIL();
    }
    level->item_drops.data = Memory_Alloc(
        sizeof(GF_DROP_ITEM_DATA) * (size_t)level->item_drops.count);

    for (int32_t i = 0; i < level->item_drops.count; i++) {
        JSON_MUST(JSON_PUSH_INDEX(io, i));
        GF_DROP_ITEM_DATA *data = &level->item_drops.data[i];

        JSON_MUST(JSON_READ(io, "enemy_num", &data->enemy_num));

        JSON_MUST(JSON_PUSH(io, "object_ids"));
        const int32_t object_count = JSON_ARRAY_LEN(io);
        if (object_count < 0) {
            JSON_FAIL();
        }
        data->count = object_count;
        data->object_ids = Memory_Alloc(sizeof(int16_t) * data->count);
        for (int32_t j = 0; j < data->count; j++) {
            JSON_MUST(JSON_PUSH_INDEX(io, j));
            OBJECT_ID id = NO_OBJECT;
            JSON_MUST(M_ReadObjectID(ctx, &id));
            data->object_ids[j] = (int16_t)id;
            JSON_MUST(JSON_POP(io));
        }
        JSON_MUST(JSON_POP(io));
        JSON_MUST(JSON_POP(io));
    }
    JSON_MUST(JSON_POP(io));
    JSON_FINISH();
}

static void M_CopyRootSettingsIntoLevel(
    GF_LEVEL_SETTINGS *const dst, const GF_LEVEL_SETTINGS *const src)
{
    *dst = *src;
    dst->sfx_path = nullptr;
}

static void M_ReadModMeta(JSON_READ_IO *const io, GF_MOD_META *const meta)
{
    meta->name = nullptr;
    meta->engine = 0;
    meta->extends = nullptr;

    const char *tmp_s = nullptr;
    if (JSON_OPTIONAL(JSON_READ(io, "name", &tmp_s)) && tmp_s != nullptr) {
        meta->name = Memory_DupStr(tmp_s);
    }

    JSON_OPTIONAL(JSON_READ(io, "engine", &meta->engine));

    tmp_s = nullptr;
    if (JSON_OPTIONAL(JSON_READ(io, "extends", &tmp_s)) && tmp_s != nullptr) {
        meta->extends = Memory_DupStr(tmp_s);
    }
}

static bool M_LoadRoot(const M_CONTEXT *const ctx)
{
    JSON_READ_IO *const io = ctx->io;
    const char *tmp_s = nullptr;

    M_ReadModMeta(io, &ctx->gf->meta);

    JSON_MUST(JSON_READ(io, "main_menu_picture", &tmp_s));
    ctx->gf->main_menu_background_path =
        Memory_DupStr(TRXPath_TryResolve(TRX_DYNAMIC_PATH_IMAGE_FILE, tmp_s));

    if (!JSON_READ(io, "savegame_file_fmt", &tmp_s) || tmp_s == nullptr) {
        if (!JSON_READ(io, "savegame_fmt_bson", &tmp_s) || tmp_s == nullptr) {
            JSON_FAIL();
        }
        // TODO: remove in TRX 1.5.
        LOG_WARNING(
            "%s: 'savegame_fmt_bson' is deprecated; use 'savegame_file_fmt'",
            ctx->script_path);
    }
    ctx->gf->savegame_file_fmt = Memory_DupStr(tmp_s);

    tmp_s = nullptr;
    if (JSON_OPTIONAL(JSON_READ(io, "main_script", &tmp_s))
        && tmp_s != nullptr) {
        ctx->gf->main_script_path = Memory_DupStr(
            TRXPath_TryResolve(TRX_DYNAMIC_PATH_SCRIPT_FILE, tmp_s));
    }

    if (JSON_PUSH(io, "ambient_tracks")) {
        const int32_t count = JSON_ARRAY_LEN(io);
        if (count < 0) {
            JSON_FAIL();
        }
        if (count > 0) {
            ctx->gf->ambient_tracks.is_present = true;
            ctx->gf->ambient_tracks.count = count;
            ctx->gf->ambient_tracks.ids =
                Memory_Alloc(sizeof(MUSIC_ID) * (size_t)count);
            for (int32_t i = 0; i < count; i++) {
                int32_t track = MX_INACTIVE;
                JSON_SHOULD(JSON_READ_A(io, i, &track));
                ctx->gf->ambient_tracks.ids[i] = track;
            }
        }
        JSON_MUST(JSON_POP(io));
    }

    ctx->gf->enable_tr2_item_drops = g_TRVersion > 1;
    JSON_READ_D(
        io, "enable_tr2_item_drops", &ctx->gf->enable_tr2_item_drops,
        g_TRVersion > 1);
    JSON_READ_D(
        io, "convert_dropped_guns", &ctx->gf->convert_dropped_guns,
        g_TRVersion > 1);
    JSON_FINISH();
}

static bool M_LoadGlobeEntry(
    const M_CONTEXT *const ctx, void *const target_elem, size_t idx,
    void *const user_arg)
{
    GF_GLOBE_ENTRY *const e = target_elem;
    ASSERT(user_arg == nullptr);
    JSON_READ_IO *const io = ctx->io;
    JSON_MUST(JSON_READ(io, "rot", &e->rot));

    JSON_MUST(JSON_READ(io, "start_level_ordinal", &e->start_level_ordinal));
    JSON_MUST(JSON_READ(
        io, "completion_level_ordinal", &e->completion_level_ordinal));

    JSON_MUST(JSON_PUSH(io, "prereq_zones"));
    const int32_t prereq_count = JSON_ARRAY_LEN(io);
    if (prereq_count < 0) {
        JSON_FAIL();
    }
    uint32_t computed_prereq_mask = 0;
    for (int32_t i = 0; i < prereq_count; i++) {
        int32_t zone = JSON_INVALID_NUMBER;
        JSON_MUST(JSON_READ_A(io, i, &zone));
        if (zone < 0 || zone >= MAX_GLOBE_ZONES) {
            JSON_ReadIO_SetError(
                io, "'prereq_zones' entries must be in range 0..%d",
                MAX_GLOBE_ZONES - 1);
            JSON_FAIL();
        }
        computed_prereq_mask |= 1u << zone;
    }
    e->prereq_mask = computed_prereq_mask;
    JSON_MUST(JSON_POP(io));

    int32_t mesh_idx = JSON_INVALID_NUMBER;
    JSON_MUST(JSON_READ(io, "mesh_idx", &mesh_idx));
    e->mesh_idx = (uint8_t)mesh_idx;
    JSON_FINISH();
}

static M_DECLARE_SEQUENCE_EVENT_HANDLER_FUNC(M_HandleIntEvent)
{
    JSON_READ_IO *const io = ctx->io;
    if (event != nullptr) {
        int32_t value;
        JSON_READ_D(io, user_arg, &value, -1);
        event->data = (void *)(intptr_t)value;
    }
    return 0;
}

static M_DECLARE_SEQUENCE_EVENT_HANDLER_FUNC(M_HandlePictureEvent)
{
    JSON_READ_IO *const io = ctx->io;
    char *expanded_path = nullptr;
    JSON_SHOULD(M_ReadPath(
        io, "path", false, TRX_DYNAMIC_PATH_IMAGE_FILE, &expanded_path));

    if (event != nullptr) {
        GF_DISPLAY_PICTURE_DATA *const event_data = extra_data;
        JSON_READ_D(io, "legal", &event_data->is_legal, false);
        JSON_READ_D(io, "credit", &event_data->is_credit, false);
        JSON_READ_D(io, "display_time", &event_data->display_time, 5.0f);
        JSON_READ_D(io, "fade_in_time", &event_data->fade_in_time, 1.0f);
        JSON_READ_D(
            io, "fade_out_time", &event_data->fade_out_time, 1.0f / 3.0f);
        if (expanded_path != nullptr) {
            event_data->path =
                (char *)extra_data + sizeof(GF_DISPLAY_PICTURE_DATA);
            strcpy(event_data->path, expanded_path);
        } else {
            event_data->path = nullptr;
        }
        event->data = event_data;
    }

    const int32_t out_size = sizeof(GF_DISPLAY_PICTURE_DATA)
        + (expanded_path == nullptr ? 0 : strlen(expanded_path) + 1);
    Memory_FreePointer(&expanded_path);
    return out_size;

fail:
    Memory_FreePointer(&expanded_path);
    return 0;
}

static M_DECLARE_SEQUENCE_EVENT_HANDLER_FUNC(M_HandleTotalStatsEvent)
{
    JSON_READ_IO *const io = ctx->io;
    char *expanded_path = nullptr;
    JSON_SHOULD(M_ReadPath(
        io, "background_path", false, TRX_DYNAMIC_PATH_IMAGE_FILE,
        &expanded_path));
    if (expanded_path == nullptr) {
        if (event != nullptr) {
            event->data = nullptr;
        }
        return 0;
    }
    if (event != nullptr) {
        char *const event_data = extra_data;
        strcpy(event_data, expanded_path);
        event->data = event_data;
    }
    const int32_t out_size = strlen(expanded_path) + 1;
    Memory_FreePointer(&expanded_path);
    return out_size;
}

static M_DECLARE_SEQUENCE_EVENT_HANDLER_FUNC(M_HandleAddItemEvent)
{
    JSON_READ_IO *const io = ctx->io;
    OBJECT_ID obj_id = NO_OBJECT;
    JSON_MUST(JSON_PUSH(io, "object_id"));
    JSON_MUST(M_ReadObjectID(ctx, &obj_id));
    JSON_MUST(JSON_POP(io));
    if (event != nullptr) {
        GF_ADD_ITEM_DATA *const event_data = extra_data;
        event_data->object_id = obj_id;
        JSON_READ_D(io, "quantity", &event_data->quantity, 1);
        event_data->inv_type =
            event->type == GFS_ADD_ITEM ? GF_INV_REGULAR : GF_INV_SECRET;
        event->data = event_data;
    }
    return sizeof(GF_ADD_ITEM_DATA);
fail:
    return -1;
}

static M_DECLARE_SEQUENCE_EVENT_HANDLER_FUNC(M_HandleGlobeSelectEvent)
{
    JSON_READ_IO *const io = ctx->io;
    const char *image;
    JSON_READ_D(io, "image", &image, nullptr);
    char *expanded_image =
        Memory_DupStr(TRXPath_TryResolve(TRX_DYNAMIC_PATH_IMAGE_FILE, image));
    if (expanded_image == nullptr) {
        if (event != nullptr) {
            event->data = nullptr;
        }
        return 0;
    }

    if (event != nullptr) {
        GF_GLOBE_SELECT_DATA *const event_data = extra_data;
        event_data->image_path =
            (char *)extra_data + sizeof(GF_GLOBE_SELECT_DATA);
        strcpy(event_data->image_path, expanded_image);
        event->data = event_data;
    }

    const int32_t out_size =
        sizeof(GF_GLOBE_SELECT_DATA) + strlen(expanded_image) + 1;
    Memory_FreePointer(&expanded_image);
    return out_size;
}

static bool M_LoadArray(
    const M_CONTEXT *const ctx, const char *const key, int32_t *const count,
    void **const elements, const size_t element_size,
    const M_LOAD_ARRAY_FUNC load_func, void *const load_func_arg)
{
    if (!JSON_OPTIONAL(JSON_PUSH(ctx->io, key))) {
        return true;
    }
    const int32_t elem_count = JSON_ARRAY_LEN(ctx->io);
    if (elem_count < 0) {
        JSON_FAIL();
    }
    *count = elem_count;
    *elements = Memory_Alloc(element_size * (size_t)(*count));

    for (size_t i = 0; i < (size_t)elem_count; i++) {
        void *const element = (char *)*elements + i * element_size;
        JSON_MUST(JSON_PUSH_INDEX(ctx->io, i));
        JSON_MUST(load_func(ctx, element, i, load_func_arg));
        JSON_MUST(JSON_POP(ctx->io));
    }
    JSON_MUST(JSON_POP(ctx->io));
    JSON_FINISH();
}

static bool M_LoadGlobeSelectEntries(const M_CONTEXT *const ctx)
{
    ctx->gf->globe.count = 0;
    ctx->gf->globe.entries = nullptr;
    return M_LoadArray(
        ctx, "globe_select_entries", &ctx->gf->globe.count,
        (void **)&ctx->gf->globe.entries, sizeof(GF_GLOBE_ENTRY),
        M_LoadGlobeEntry, nullptr);
}

static int32_t M_LoadSequenceEvent(
    const M_CONTEXT *const ctx, GF_SEQUENCE_EVENT *const event,
    void *const extra_data)
{
    const char *type_str = nullptr;
    JSON_MUST(JSON_READ(ctx->io, "type", &type_str));
    const GF_SEQUENCE_EVENT_TYPE type =
        ENUM_MAP_GET(GF_SEQUENCE_EVENT_TYPE, type_str, -1);
    if (type == (GF_SEQUENCE_EVENT_TYPE)-1) {
        JSON_ReadIO_SetError(
            ctx->io, "unknown game flow sequence event type: '%s'", type_str);
        JSON_FAIL();
    }

    const M_SEQUENCE_EVENT_HANDLER *handler = M_GetSequenceEventHandlers();
    while (handler->event_type != (GF_SEQUENCE_EVENT_TYPE)-1
           && handler->event_type != type) {
        handler++;
    }

    int32_t extra_data_size = 0;
    if (handler->handler_func != nullptr) {
        extra_data_size = handler->handler_func(
            ctx, nullptr, nullptr, handler->handler_func_arg);
    }
    if (extra_data_size >= 0 && event != nullptr) {
        event->type = handler->event_type;
        if (handler->handler_func != nullptr) {
            handler->handler_func(
                ctx, event, extra_data, handler->handler_func_arg);
        } else {
            event->data = nullptr;
        }
    }
    return extra_data_size;
fail:
    return -1;
}

static bool M_LoadSequence(
    const M_CONTEXT *const ctx, GF_SEQUENCE *const sequence)
{
    JSON_READ_IO *const io = ctx->io;
    sequence->length = 0;
    const int32_t seq_count = JSON_ARRAY_LEN(io);
    if (seq_count < 0) {
        JSON_FAIL();
    }

    const size_t event_base_size = sizeof(GF_SEQUENCE_EVENT);
    size_t total_data_size = 0;
    for (int32_t i = 0; i < seq_count; i++) {
        JSON_MUST(JSON_PUSH_INDEX(io, i));
        const int32_t event_extra_size =
            M_LoadSequenceEvent(ctx, nullptr, nullptr);
        JSON_MUST(JSON_POP(io));
        if (event_extra_size < 0) {
            JSON_FAIL();
        }
        sequence->length++;
        total_data_size += Memory_Align((size_t)event_extra_size);
    }

    const size_t events_block_size =
        Memory_Align((size_t)sequence->length * event_base_size);
    total_data_size += events_block_size;

    char *const data = Memory_Alloc(total_data_size);
    char *extra_data_ptr = data + events_block_size;
    sequence->events = (GF_SEQUENCE_EVENT *)data;

    int32_t j = 0;
    for (int32_t i = 0; i < seq_count; i++) {
        JSON_MUST(JSON_PUSH_INDEX(io, i));
        const int32_t event_extra_size =
            M_LoadSequenceEvent(ctx, &sequence->events[j], extra_data_ptr);
        JSON_MUST(JSON_POP(io));
        if (event_extra_size < 0) {
            // Parsing this event failed - discard it
            continue;
        }
        extra_data_ptr += Memory_Align((size_t)event_extra_size);
        j++;
    }
    JSON_FINISH();
}

static bool M_LoadLevelInjections(
    const M_CONTEXT *const ctx, GF_LEVEL *const level)
{
    JSON_READ_IO *const io = ctx->io;
    bool inherit;
    JSON_READ_D(io, "inherit_injections", &inherit, true);
    int32_t local_count = 0;
    if (JSON_PUSH(io, "injections")) {
        local_count = JSON_ARRAY_LEN(io);
        if (local_count < 0) {
            JSON_FAIL();
        }
        JSON_MUST(JSON_POP(io));
    }

    level->injections.count = 0;
    if (local_count == 0 && !inherit) {
        return true;
    }

    if (inherit) {
        level->injections.count += ctx->gf->injections.count;
    }
    level->injections.count += local_count;

    level->injections.data_paths =
        Memory_Alloc(sizeof(char *) * level->injections.count);

    int32_t base_index = 0;
    if (inherit) {
        for (int32_t i = 0; i < ctx->gf->injections.count; i++) {
            level->injections.data_paths[i] =
                Memory_DupStr(ctx->gf->injections.data_paths[i]);
        }
        base_index = ctx->gf->injections.count;
    }

    if (local_count == 0) {
        return true;
    }

    JSON_MUST(JSON_PUSH(io, "injections"));
    for (int32_t i = 0; i < local_count; i++) {
        const char *str = nullptr;
        JSON_MUST(JSON_READ_A(io, i, &str));
        level->injections.data_paths[base_index + i] = Memory_DupStr(
            TRXPath_TryResolve(TRX_DYNAMIC_PATH_INJECTION_FILE, str));
    }
    JSON_MUST(JSON_POP(io));
    JSON_FINISH();
}

static bool M_LoadLevelSequence(
    const M_CONTEXT *const ctx, GF_LEVEL *const level)
{
    JSON_MUST(JSON_PUSH(ctx->io, "sequence"));
    JSON_MUST(M_LoadSequence(ctx, &level->sequence));
    JSON_MUST(JSON_POP(ctx->io));

    for (int32_t i = 0; i < level->sequence.length; i++) {
        GF_SEQUENCE_EVENT *const event = &level->sequence.events[i];
        if (event->type == GFS_LOOP_GAME) {
            event->data = (void *)(intptr_t)level->num;
        }
    }
    JSON_FINISH();
}

static bool M_LoadLevel(
    const M_CONTEXT *const ctx, void *const target_elem, const size_t idx,
    void *const user_arg)
{
    GF_LEVEL *const level = target_elem;
    JSON_READ_IO *const io = ctx->io;
    level->num = idx;

    {
        level->type = (GF_LEVEL_TYPE)(intptr_t)user_arg;
        if (JSON_OPTIONAL(JSON_PUSH(io, "type"))) {
            const char *tmp = nullptr;
            JSON_MUST(JSON_READ_CURRENT(io, &tmp));
            JSON_MUST(JSON_POP(io));
            const GF_LEVEL_TYPE user_type =
                ENUM_MAP_GET(GF_LEVEL_TYPE, tmp, -1);
            if (user_type == (GF_LEVEL_TYPE)-1) {
                JSON_ReadIO_SetError(io, "unrecognized type '%s'", tmp);
                JSON_FAIL();
            }

            if (level->type != GFL_NORMAL
                && GF_GetLevelTableType(user_type) != GFLT_MAIN) {
                JSON_ReadIO_SetError(
                    io, "cannot override level type=%s to %s",
                    ENUM_MAP_TO_STRING(GF_LEVEL_TYPE, level->type),
                    ENUM_MAP_TO_STRING(GF_LEVEL_TYPE, user_type));
                JSON_FAIL();
            }
            level->type = user_type;
        }
    }

    if (level->type == GFL_DUMMY) {
        return true;
    }

    {
        const TRX_DYNAMIC_PATH path_type =
            (level->type == GFL_DUMMY || level->type == GFL_CURRENT)
            ? (TRX_DYNAMIC_PATH)-1
            : (level->type == GFL_TITLE || level->type == GFL_GYM)
            ? TRX_DYNAMIC_PATH_SHARED_LEVEL_FILE
            : TRX_DYNAMIC_PATH_LEVEL_FILE;
        JSON_MUST(M_ReadPath(io, "path", false, path_type, &level->path));
    }
    {
        const char *tmp_script = nullptr;
        if (JSON_OPTIONAL(JSON_READ(io, "script", &tmp_script))
            && tmp_script != nullptr) {
            level->script_path = Memory_DupStr(
                TRXPath_TryResolve(TRX_DYNAMIC_PATH_SCRIPT_FILE, tmp_script));
        } else {
            level->script_path = nullptr;
        }
    }

    {
        level->music_track = MX_INACTIVE;
        if (JSON_OPTIONAL(JSON_PUSH(io, "music_track"))) {
            JSON_MUST(JSON_READ_CURRENT(io, &level->music_track));
            JSON_MUST(JSON_POP(io));
        }
    }

    {
        const bool outfit_optional = level->type == GFL_TITLE
            || level->type == GFL_DUMMY || level->type == GFL_CURRENT;
        if (!outfit_optional) {
            const char *tmp = nullptr;
            JSON_MUST(JSON_READ(io, "lara_outfit", &tmp));
            if (!ctx->validation_mode
                && !Lara_Skin_IsOutfitAvailable(
                    Lara_Skin_FindOutfitByName(tmp))) {
                JSON_ReadIO_SetError(
                    io, "invalid 'lara_outfit' value (%s)", tmp);
                JSON_FAIL();
            }
            level->lara_outfit = Memory_DupStr(tmp);
        }
    }

    level->weather_type = WEATHER_NONE;
    {
        if (JSON_OPTIONAL(JSON_PUSH(io, "weather_type"))) {
            const char *tmp = nullptr;
            JSON_MUST(JSON_READ_CURRENT(io, &tmp));
            JSON_MUST(JSON_POP(io));
            level->weather_type = ENUM_MAP_GET(WEATHER_TYPE, tmp, WEATHER_NONE);
        }
    }
    JSON_READ_D(io, "water_particles", &level->water_particles, false);

    JSON_READ_D(io, "unobtainable_pickups", &level->unobtainable.pickups, 0);
    JSON_READ_D(io, "unobtainable_kills", &level->unobtainable.kills, 0);
    JSON_READ_D(
        io, "unobtainable_ally_kills", &level->unobtainable.ally_kills, 0);
    JSON_READ_D(io, "unobtainable_secrets", &level->unobtainable.secrets, 0);

    M_CopyRootSettingsIntoLevel(&level->settings, &ctx->gf->settings);

    JSON_MUST(M_LoadSettings(ctx, &level->settings));
    JSON_MUST(M_LoadLevelItemDrops(ctx, level));
    JSON_MUST(M_LoadLevelSequence(ctx, level));
    JSON_MUST(M_LoadLevelInjections(ctx, level));
    JSON_FINISH();
}

static bool M_LoadLevelTable(
    const M_CONTEXT *const ctx, const char *const key,
    GF_LEVEL_TABLE *const level_table, const GF_LEVEL_TYPE default_level_type)
{
    JSON_MUST(M_LoadArray(
        ctx, key, &level_table->count, (void **)&level_table->levels,
        sizeof(GF_LEVEL), M_LoadLevel, (void *)(intptr_t)default_level_type));
    JSON_FINISH();
}

static bool M_LoadLevels(const M_CONTEXT *const ctx)
{
    JSON_MUST(JSON_PUSH(ctx->io, "levels"));
    JSON_MUST(JSON_POP(ctx->io));
    JSON_MUST(M_LoadLevelTable(
        ctx, "levels", &ctx->gf->level_tables[GFLT_MAIN], GFL_NORMAL));
    JSON_FINISH();
}

static bool M_LoadCutscenes(const M_CONTEXT *const ctx)
{
    return M_LoadLevelTable(
        ctx, "cutscenes", &ctx->gf->level_tables[GFLT_CUTSCENES], GFL_CUTSCENE);
}

static bool M_LoadDemos(const M_CONTEXT *const ctx)
{
    return M_LoadLevelTable(
        ctx, "demos", &ctx->gf->level_tables[GFLT_DEMOS], GFL_DEMO);
}

static bool M_LoadTitleLevel(const M_CONTEXT *const ctx)
{
    if (!JSON_OPTIONAL(JSON_PUSH(ctx->io, "title"))) {
        return true;
    }
    ctx->gf->title_level = Memory_Alloc(sizeof(GF_LEVEL));
    JSON_MUST(
        M_LoadLevel(ctx, ctx->gf->title_level, 0, (void *)(intptr_t)GFL_TITLE));
    JSON_MUST(JSON_POP(ctx->io));
    JSON_FINISH();
}

static bool M_LoadFMV(
    const M_CONTEXT *const ctx, void *const target_elem, size_t idx,
    void *const user_arg)
{
    GF_FMV *const fmv = target_elem;
    ASSERT(user_arg == nullptr);
    JSON_READ_IO *const io = ctx->io;
    char *path = nullptr;
    JSON_SHOULD(
        M_ReadPath(io, "path", false, TRX_DYNAMIC_PATH_FMV_FILE, &path));
    fmv->path = path;
    JSON_READ_D(io, "legal", &fmv->is_legal, false);
    JSON_READ_D(io, "credit", &fmv->is_credit, false);
    JSON_FINISH();
}

static bool M_LoadFMVs(const M_CONTEXT *const ctx)
{
    JSON_MUST(M_LoadArray(
        ctx, "fmvs", &ctx->gf->fmv_count, (void **)&ctx->gf->fmvs,
        sizeof(GF_FMV), M_LoadFMV, nullptr));
    JSON_FINISH();
}

static bool M_LoadGlobalInjections(const M_CONTEXT *const ctx)
{
    JSON_READ_IO *const io = ctx->io;
    ctx->gf->injections.count = 0;
    if (!JSON_PUSH(io, "injections")) {
        return true;
    }
    ctx->gf->injections.count = JSON_ARRAY_LEN(io);
    if (ctx->gf->injections.count < 0) {
        JSON_FAIL();
    }
    ctx->gf->injections.data_paths =
        Memory_Alloc(sizeof(char *) * (size_t)ctx->gf->injections.count);
    for (int32_t i = 0; i < ctx->gf->injections.count; i++) {
        const char *str = nullptr;
        JSON_MUST(JSON_READ_A(io, i, &str));
        ctx->gf->injections.data_paths[i] = Memory_DupStr(
            TRXPath_TryResolve(TRX_DYNAMIC_PATH_INJECTION_FILE, str));
    }
    JSON_MUST(JSON_POP(io));
    JSON_FINISH();
}

static bool M_LoadGameFlowDoc(
    JSON_VALUE *const doc, const char *const path, const bool exit_on_error,
    const bool validation_mode)
{
    GF_Shutdown();

    M_CONTEXT ctx = { .gf = &g_GameFlow, .validation_mode = validation_mode };
    ctx.gf->main_script_path = nullptr;
    ctx.gf->path = Memory_DupStr(path);
    ctx.script_path = g_GameFlow.path;
    ctx.io = nullptr;

    ctx.io = JSON_ReadIO_Create(doc, 0, path);

    JSON_MUST(M_LoadRoot(&ctx));
    JSON_MUST(M_LoadSettings(&ctx, &ctx.gf->settings));
    JSON_MUST(M_LoadGlobeSelectEntries(&ctx));
    JSON_MUST(M_LoadGlobalInjections(&ctx));
    JSON_MUST(M_LoadLevels(&ctx));
    JSON_MUST(M_LoadCutscenes(&ctx));
    JSON_MUST(M_LoadDemos(&ctx));
    JSON_MUST(M_LoadFMVs(&ctx));
    JSON_MUST(M_LoadTitleLevel(&ctx));

    JSON_ReadIO_Destroy(ctx.io);
    JSON_ValueFree(doc);
    return true;
fail:
    if (exit_on_error) {
        M_ExitWithJSONError(&ctx);
    } else {
        LOG_WARNING("%s", JSON_ReadIO_GetError(ctx.io));
        JSON_ReadIO_Destroy(ctx.io);
        JSON_ValueFree(doc);
        GF_Shutdown();
    }
    return false;
}

static bool M_LoadGameFlowEx(
    const char *const path, const bool exit_on_error,
    const bool validation_mode)
{
    JSON_VALUE *const doc = JSONFile_ReadEx(path, exit_on_error);
    if (doc == nullptr) {
        if (exit_on_error) {
            Shell_ExitSystemFmt("Failed to open script file %s", path);
        }
        return false;
    }
    return M_LoadGameFlowDoc(doc, path, exit_on_error, validation_mode);
}

void GF_LoadFromFile(const char *const path)
{
    M_LoadGameFlowEx(path, true, false);
}

bool GF_TryLoadFromFile(const char *const path)
{
    return M_LoadGameFlowEx(path, false, false);
}

bool GF_ValidateMod(const char *const mod_name, const char *const path)
{
    if (!M_LoadGameFlowEx(path, false, true)) {
        LOG_WARNING("Mod '%s' has invalid gameflow data", mod_name);
        return false;
    }
    GF_Shutdown();
    return true;
}

bool GF_ReadModMeta(const char *const path, GF_MOD_META *const meta)
{
    ASSERT(meta != nullptr);

    JSON_VALUE *const doc = JSONFile_Read(path);
    if (doc == nullptr) {
        return false;
    }

    JSON_READ_IO *const io = JSON_ReadIO_Create(doc, 0, path);
    M_ReadModMeta(io, meta);
    JSON_ReadIO_Destroy(io);
    JSON_ValueFree(doc);
    return true;
}
