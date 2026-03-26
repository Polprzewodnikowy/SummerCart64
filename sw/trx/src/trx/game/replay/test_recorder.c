#include <trx/game/replay/test_recorder.h>

#include <trx/config.h>
#include <trx/core/enum_map.h>
#include <trx/core/filesystem.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/debug.h>
#include <trx/game/console/common.h>
#include <trx/game/events.h>
#include <trx/game/input/backends/controller.h>
#include <trx/game/input/backends/keyboard.h>
#include <trx/game/input/common.h>
#include <trx/game/lara.h>
#include <trx/game/random.h>

#include <stdlib.h>
#include <string.h>

#define M_DEBUG 0
#define M_MAX_EVENTS 64 // Maximum SDL or custom events per frame

// Internal event codes for recorder swimlane
typedef enum {
    M_CUSTOM_EVENT_SCREENSHOT,
    M_CUSTOM_EVENT_COMMAND,
} M_CUSTOM_EVENT;

typedef struct {
    MYFILE *file;
    int32_t prev_frame_idx;
    int32_t frame_idx;
    SDL_Event queue[M_MAX_EVENTS];
    int32_t queue_size;
    int32_t listeners[2];
} M_PRIV;

static const struct {
    const char *arg;
    bool takes_value;
} m_SkipArgs[] = {
    { "--debug-render-performance", false },
    { "--test-record", true },
    { "--test-replay", true },
    { "--test-play", true },
    { "--headless-fps", true },
    { nullptr, false },
};

static M_PRIV m_Priv = {};

static int M_CompareConfigOption(const void *a, const void *b)
{
    const CONFIG_OPTION *const *opt_a = a;
    const CONFIG_OPTION *const *opt_b = b;
    return strcmp((*opt_a)->name, (*opt_b)->name);
}

static const char *M_DumpEvent(const SDL_Event *const event)
{
    switch (event->type) {
    case SDL_USEREVENT:
        const char *result = nullptr;
        if (event->user.code == M_CUSTOM_EVENT_SCREENSHOT) {
            char *path = event->user.data1;
            result = String_FormatStatic("noop  # cmd { screenshot %s }", path);
            Memory_FreePointer(&path);
        } else if (event->user.code == M_CUSTOM_EVENT_COMMAND) {
            char *cmd = event->user.data1;
            result = String_FormatStatic("noop  # cmd { %s }", cmd);
            Memory_FreePointer(&cmd);
        }
        return result;

    case SDL_KEYDOWN:
        // NOTE: we do not serialize the modifiers to avoid noise, as currently
        // they are unused by the engine. In the future, once we add support
        // for compound keybindings, it may become necessary to either
        // serialize them, or simulate them in the replay module.
        return String_FormatStatic(
            "● \"%s\"", Input_KeyDescFromSDL(event->key.keysym.scancode, 0));

    case SDL_KEYUP:
        return String_FormatStatic(
            "○ \"%s\"", Input_KeyDescFromSDL(event->key.keysym.scancode, 0));

    case SDL_TEXTINPUT:
        return String_FormatStatic("text-input \"%s\"", event->text.text);

    case SDL_QUIT:
        return String_FormatStatic("quit");
    }

    return nullptr;
}

static void M_DumpQueue(M_PRIV *const p)
{
#if !M_DEBUG
    if (p->queue_size == 0) {
        return;
    }
#endif
    const size_t indent = 8;
    File_WriteString(
        p->file, "%-*s", indent,
        String_FormatStatic("@+%d:", p->frame_idx - p->prev_frame_idx));
    for (int32_t i = 0; i < p->queue_size; i++) {
        const SDL_Event *const event = &p->queue[i];
        const char *const event_str = M_DumpEvent(event);
        if (event_str == nullptr) {
            continue;
        }
        File_WriteString(p->file, event_str);
        if (i < p->queue_size - 1) {
            File_WriteString(p->file, "\n%*s", indent, "");
        }
    }
#if M_DEBUG
    if (p->queue_size == 0) {
        File_WriteString(p->file, "noop");
    }
    const ITEM *const lara_item = Lara_GetItem();
    const OBJECT_ID obj_id = Lara_GetAnimationObject();
    const ITEM *const vehicle_item = Lara_Vehicle_GetItem();
    if (lara_item != nullptr) {
        File_WriteString(p->file, "\n%*s", indent, "");
        File_WriteString(
            p->file, "assert lara.pos=%d,%d,%d", lara_item->pos.x,
            lara_item->pos.y, lara_item->pos.z);
        File_WriteString(p->file, "\n%*s", indent, "");
        File_WriteString(
            p->file, "assert lara.rot=%d,%d,%d", lara_item->rot.x,
            lara_item->rot.y, lara_item->rot.z);
        File_WriteString(p->file, "\n%*s", indent, "");
        File_WriteString(
            p->file, "assert lara.anim=%d,%d,%d", obj_id,
            Item_GetRelativeObjAnim(lara_item, obj_id),
            Item_GetRelativeFrame(lara_item));
        File_WriteString(p->file, "\n%*s", indent, "");
        File_WriteString(
            p->file, "assert lara.speed=%d,%d",
            (vehicle_item != nullptr ? vehicle_item : lara_item)->speed,
            (vehicle_item != nullptr ? vehicle_item : lara_item)->fall_speed);
    }
#endif
    File_WriteString(p->file, "\n");
    p->prev_frame_idx = p->frame_idx;
}

static void M_DumpHeader(MYFILE *const fp)
{
    File_WriteString(fp, "seed_control %d\n", Random_GetControlSeed());
    File_WriteString(fp, "seed_draw %d\n", Random_GetDrawSeed());
}

static void M_DumpArguments(MYFILE *const fp, VECTOR *const original_args)
{
    // Record original arguments passed to the game
    if (original_args->count <= 0) {
        return;
    }

    // Skip tracking irrelevant arguments.
    VECTOR *const filtered_args = Vector_Create(sizeof(char *));
    for (int32_t i = 0; i < original_args->count; i++) {
        const char *const arg = *(char **)Vector_Get(original_args, i);
        int32_t skip = 0;
        for (size_t j = 0; m_SkipArgs[j].arg != nullptr; j++) {
            if (strcmp(arg, m_SkipArgs[j].arg) == 0) {
                skip = 1 + m_SkipArgs[j].takes_value;
                break;
            }
        }
        if (skip) {
            i += skip - 1;
        } else {
            Vector_Add(filtered_args, &arg);
        }
    }

    if (filtered_args->count > 0) {
        File_WriteString(fp, "args");
        for (int32_t i = 0; i < filtered_args->count; i++) {
            const char *const arg = *(char **)Vector_Get(filtered_args, i);
            File_WriteString(fp, " \"%s\"", arg);
        }
        File_WriteString(fp, "\n");
    }
    Vector_Free(filtered_args);
}

static void M_DumpConfig(MYFILE *const fp)
{
    // Record any non-default config options for later replay
    const CONFIG_OPTION *const map = Config_GetOptionMap();
    VECTOR *opts = Vector_Create(sizeof(CONFIG_OPTION *));

    for (const CONFIG_OPTION *opt = map; opt->name != nullptr; opt++) {
        if (Config_IsOptionAtDefault(opt->target)) {
            continue;
        }
        Vector_Add(opts, &opt);
    }

    CONFIG_OPTION **raw_opts = Vector_GetData(opts);
    qsort(
        raw_opts, opts->count, sizeof(CONFIG_OPTION *), M_CompareConfigOption);
    for (int32_t i = 0; i < opts->count; i++) {
        const CONFIG_OPTION *opt = raw_opts[i];
        const char *const fmt = opt->type == COT_ENUM || opt->type == COT_STRING
                || opt->type == COT_DYNAMIC_ENUM
            ? "config %s \"%s\"\n"
            : "config %s %s\n";
        File_WriteString(
            fp, fmt, opt->name, Config_GetOptionValueAsString(opt, false));
    }
    Vector_Free(opts);
}

static void M_DumpBindings(MYFILE *const fp)
{
    // Record any non-default key/controller bindings for later replay.
    // Keyboard binds
    for (INPUT_ROLE role = 0; role < INPUT_ROLE_NUMBER_OF; role++) {
        JSON_OBJECT *bind = JSON_ObjectNew();
        if (g_Input_Keyboard.assign_to_json_object(
                g_Config.input.keyboard_layout, role, bind)) {
            const SDL_Scancode sc =
                JSON_ObjectGetInt(bind, "scancode", SDL_SCANCODE_UNKNOWN);
            File_WriteString(
                fp, "bind keyboard %s \"%s\"\n",
                ENUM_MAP_TO_STRING(INPUT_ROLE, role),
                Input_KeyDescFromSDL(sc, 0));
        }
        JSON_ObjectFree(bind);
    }
    // Controller binds
    for (INPUT_ROLE role = 0; role < INPUT_ROLE_NUMBER_OF; role++) {
        JSON_OBJECT *bind = JSON_ObjectNew();
        if (g_Input_Controller.assign_to_json_object(
                g_Config.input.controller_layout, role, bind)) {
            const int32_t bt = JSON_ObjectGetInt(bind, "button_type", 0);
            const int32_t b = JSON_ObjectGetInt(bind, "bind", 0);
            const int32_t ad = JSON_ObjectGetInt(bind, "axis_dir", 0);
            File_WriteString(
                fp, "bind controller %s %d %d %d\n",
                ENUM_MAP_TO_STRING(INPUT_ROLE, role), bt, b, ad);
        }
        JSON_ObjectFree(bind);
    }
    File_WriteString(fp, "\n");
}

// Callback for game events: inject synthetic SDL_USEREVENT into queue
static void M_HandleGameEvent(const EVENT *const event, void *const user_data)
{
    M_PRIV *const p = &m_Priv;
    if (p->file == nullptr || p->queue_size >= M_MAX_EVENTS) {
        return;
    }
    SDL_Event ev = { .type = SDL_USEREVENT };
    ev.user.code = (strcmp(event->name, GAME_EVENT_SCREENSHOT) == 0)
        ? M_CUSTOM_EVENT_SCREENSHOT
        : M_CUSTOM_EVENT_COMMAND;
    ev.user.data1 = Memory_DupStr(event->data);
    ev.user.data2 = nullptr;
    p->queue[p->queue_size++] = ev;
}

void TestRecorder_Open(const char *path, VECTOR *const original_args)
{
    M_PRIV *const p = &m_Priv;
    p->file = File_Open(path, FILE_OPEN_WRITE);
    if (p->file == nullptr) {
        LOG_ERROR("Cannot open record file '%s'", path);
        return;
    }

    M_DumpHeader(p->file);
    M_DumpArguments(p->file, original_args);
    M_DumpConfig(p->file);
    M_DumpBindings(p->file);

    p->listeners[0] = GameEvent_Subscribe(
        GAME_EVENT_SCREENSHOT, nullptr, M_HandleGameEvent, nullptr);
    p->listeners[1] = GameEvent_Subscribe(
        GAME_EVENT_COMMAND, nullptr, M_HandleGameEvent, nullptr);

    LOG_INFO("Starting recording");
}

bool TestRecorder_IsOpened(void)
{
    M_PRIV *const p = &m_Priv;
    return p->file != nullptr;
}

void TestRecorder_Close(void)
{
    M_PRIV *const p = &m_Priv;
    if (p->file != nullptr) {
        File_Close(p->file);
        p->file = nullptr;
    }

    GameEvent_Unsubscribe(p->listeners[0]);
    GameEvent_Unsubscribe(p->listeners[1]);
}

void TestRecorder_BeginFrame(void)
{
    M_PRIV *const p = &m_Priv;
    if (p->file != nullptr) {
        p->queue_size = 0;
    }
}

void TestRecorder_EndFrame(void)
{
    M_PRIV *const p = &m_Priv;
    if (p->file != nullptr) {
        M_DumpQueue(p);
    }
    p->frame_idx++;
}

void TestRecorder_RecordEvent(const SDL_Event *const event)
{
    M_PRIV *const p = &m_Priv;
    if (p->file == nullptr) {
        return;
    }

    // Only record eligible events
    if (event->type != SDL_KEYDOWN && event->type != SDL_KEYUP
        && event->type != SDL_QUIT && event->type != SDL_TEXTINPUT
        && event->type != SDL_USEREVENT) {
        return;
    }
    if (event->type == SDL_KEYDOWN && event->key.repeat) {
        return;
    }
    if (event->type == SDL_TEXTINPUT && !Console_IsOpened()) {
        return;
    }

    if (p->queue_size < M_MAX_EVENTS) {
        p->queue[p->queue_size++] = *event;
    }
}
