#include <trx/game/replay/test_replay.h>

#include <trx/config.h>
#include <trx/core/enum_map.h>
#include <trx/core/filesystem.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/core/vector.h>
#include <trx/debug.h>
#include <trx/game/console/common.h>
#include <trx/game/input/backends/controller.h>
#include <trx/game/input/backends/keyboard.h>
#include <trx/game/input/common.h>
#include <trx/game/lara.h>
#include <trx/game/lua.h>
#include <trx/game/random.h>
#include <trx/game/shell.h>
#include <trx/game/shell/events.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define M_DEBUG 0

typedef struct {
    SHELL_ARGS *args;
} M_PARSE_CTX;

typedef struct {
    bool collecting;
    int32_t brace_depth;
    bool in_quote;
    bool escaped;
} M_BLOCK_EVENT_CTX;

// Parsed frame events
typedef struct {
    int32_t frame_idx;
    VECTOR *events; // vector of char*
} M_FRAME;

// Replay private state
typedef struct {
    char *data; // Replay file data buffer
    size_t size; // Size of data buffer
    VECTOR *headers; // Vector of char* header lines
    VECTOR *frames; // Vector of M_FRAME frames to play
    int32_t frame_idx; // Current playback frame index
    int32_t next_frame_idx; // Next frame to process
    bool replay_quiet;
    struct {
        bool seen;
        bool quiet_applied;
        LOG_LEVEL log_level_before_quiet;
        bool summary_printed;
        bool case_active;
        char *case_name;
        int32_t case_checks;
        int32_t case_fails;
        int32_t cases_passed;
        int32_t cases_failed;
        int32_t checks_passed;
        int32_t checks_failed;
        int32_t exit_code_override;
        bool use_ansi_colors;
    } test_mode;
} M_PRIV;

static M_PRIV m_Priv = {};

typedef bool (*M_EVENT_HANDLER)(const char *token);
typedef bool (*M_HEADER_HANDLER)(const char *line, M_PARSE_CTX *ctx);

// Event parsers
static bool M_ParseQuitEvent(const char *event_str);
static bool M_ParseKeyDownEvent(const char *event_str);
static bool M_ParseKeyUpEvent(const char *event_str);
static bool M_ParseTextInputEvent(const char *event_str);
static bool M_ParseCommandEvent(const char *event_str);
static bool M_ParseNoopEvent(const char *event_str);
static bool M_ParseLuaEvent(const char *event_str);
static bool M_ParseTestCaseEvent(const char *event_str);
static bool M_ParseExpectEvent(const char *event_str);

// Header parsers
static bool M_ParseSeedControl(const char *line, M_PARSE_CTX *ctx);
static bool M_ParseSeedDraw(const char *line, M_PARSE_CTX *ctx);
static bool M_ParseBindKeyboard(const char *line, M_PARSE_CTX *ctx);
static bool M_ParseBindController(const char *line, M_PARSE_CTX *ctx);
static bool M_ParseArgs(const char *line, M_PARSE_CTX *ctx);
static bool M_ParseConfig(const char *line, M_PARSE_CTX *ctx);
static bool M_ParseTestCaseHeader(const char *line, M_PARSE_CTX *ctx);

static const M_HEADER_HANDLER m_HeaderHandlers[] = {
    M_ParseSeedControl,    M_ParseSeedDraw, M_ParseBindKeyboard,
    M_ParseBindController, M_ParseArgs,     M_ParseConfig,
    M_ParseTestCaseHeader, nullptr,
};

static const M_EVENT_HANDLER m_EventHandlers[] = {
    M_ParseQuitEvent,   M_ParseTestCaseEvent,
    M_ParseExpectEvent, M_ParseKeyDownEvent,
    M_ParseKeyUpEvent,  M_ParseTextInputEvent,
    M_ParseNoopEvent,   M_ParseCommandEvent,
    M_ParseLuaEvent,    nullptr,
};

static void M_TestPrint(const char *const fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    vprintf(fmt, va);
    printf("\n");
    fflush(stdout);
    va_end(va);
}

static const char *M_TestColor(const char *const color)
{
    return m_Priv.test_mode.use_ansi_colors ? color : "";
}

static const char *M_TestColorReset(void)
{
    return m_Priv.test_mode.use_ansi_colors ? LOG_ANSI_COLOR_RESET : "";
}

static void M_EndTestCase(void)
{
    M_PRIV *const p = &m_Priv;
    if (!p->test_mode.case_active) {
        return;
    }

    if (p->test_mode.case_fails == 0) {
        p->test_mode.cases_passed++;
        M_TestPrint(
            "%sPASS%s | %s", M_TestColor(LOG_ANSI_COLOR_GREEN),
            M_TestColorReset(), p->test_mode.case_name);
    } else {
        p->test_mode.cases_failed++;
        M_TestPrint(
            "%sFAIL%s | %s (%d/%d checks failed)",
            M_TestColor(LOG_ANSI_COLOR_RED), M_TestColorReset(),
            p->test_mode.case_name, p->test_mode.case_fails,
            p->test_mode.case_checks);
    }

    p->test_mode.case_active = false;
    Memory_FreePointer(&p->test_mode.case_name);
    p->test_mode.case_checks = 0;
    p->test_mode.case_fails = 0;
}

static void M_TestReportSummary(void)
{
    M_PRIV *const p = &m_Priv;
    if (!p->test_mode.seen || p->test_mode.summary_printed) {
        return;
    }

    if (p->test_mode.case_active) {
        M_EndTestCase();
    }

    M_TestPrint("\n=== TEST_SUMMARY ===");
    const int32_t total_cases =
        p->test_mode.cases_passed + p->test_mode.cases_failed;
    if (p->test_mode.cases_passed > 0) {
        M_TestPrint(
            "%sPASSED: %d of %d%s", M_TestColor(LOG_ANSI_COLOR_GREEN),
            p->test_mode.cases_passed, total_cases, M_TestColorReset());
    }
    if (p->test_mode.cases_failed > 0) {
        M_TestPrint(
            "%sFAILED: %d of %d%s", M_TestColor(LOG_ANSI_COLOR_RED),
            p->test_mode.cases_failed, total_cases, M_TestColorReset());
    }
    p->test_mode.summary_printed = true;
}

static void M_ApplyQuietInTestMode(void)
{
    M_PRIV *const p = &m_Priv;
    if (!p->replay_quiet || !p->test_mode.seen || p->test_mode.quiet_applied) {
        return;
    }
    p->test_mode.log_level_before_quiet = Log_GetMinLevel();
    Log_SetMinLevel((LOG_LEVEL)100);
    p->test_mode.quiet_applied = true;
}

static void M_TerminateFromTestResult(void)
{
    M_PRIV *const p = &m_Priv;
    if (p->test_mode.cases_failed > 0 || p->test_mode.checks_failed > 0) {
        p->test_mode.exit_code_override = 1;
    } else {
        p->test_mode.exit_code_override = 0;
    }

    SDL_Event event = { .type = SDL_QUIT };
    Shell_ProcessEvent(&event);
}

static bool M_ParseQuotedPayload(
    const char *const event_str, const char *const prefix,
    const char **const out_start, size_t *const out_len)
{
    if (strncmp(event_str, prefix, strlen(prefix)) != 0) {
        return false;
    }

    const char *const start = strchr(event_str + strlen(prefix), '"');
    const char *const end = start ? strrchr(start + 1, '"') : nullptr;
    if (start == nullptr || end == nullptr || end <= start + 1) {
        return false;
    }

    *out_start = start + 1;
    *out_len = (size_t)(end - (start + 1));
    return true;
}

static const char *M_SkipWhitespaceConst(const char *const s)
{
    const char *p = s;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return p;
}

static char *M_TrimWhitespaceInPlace(char *const s)
{
    char *start = s;
    while (*start == ' ' || *start == '\t' || *start == '\n'
           || *start == '\r') {
        start++;
    }
    char *end = start + strlen(start);
    while (end > start
           && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n'
               || end[-1] == '\r')) {
        end--;
    }
    *end = '\0';
    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
    return s;
}

static void M_ScanBraceState(
    const char *const s, int32_t *const io_brace_depth, bool *const io_in_quote,
    bool *const io_escaped)
{
    for (const char *p = s; *p != '\0'; p++) {
        if (*io_in_quote) {
            if (*io_escaped) {
                *io_escaped = false;
                continue;
            }
            if (*p == '\\') {
                *io_escaped = true;
                continue;
            }
            if (*p == '"') {
                *io_in_quote = false;
            }
            continue;
        }

        if (*p == '"') {
            *io_in_quote = true;
            continue;
        }
        if (*p == '{') {
            (*io_brace_depth)++;
            continue;
        }
        if (*p == '}') {
            (*io_brace_depth)--;
            continue;
        }
    }
}

static const char *M_GetBlockPayloadStartIfAny(const char *const evt)
{
    if (strncmp(evt, "expect ", strlen("expect ")) == 0) {
        return M_SkipWhitespaceConst(evt + strlen("expect "));
    }
    if (strncmp(evt, "lua ", strlen("lua ")) == 0) {
        return M_SkipWhitespaceConst(evt + strlen("lua "));
    }
    if (strncmp(evt, "cmd ", strlen("cmd ")) == 0) {
        return M_SkipWhitespaceConst(evt + strlen("cmd "));
    }
    return nullptr;
}

static bool M_TryStartBlockEvent(
    M_BLOCK_EVENT_CTX *const ctx, const char *const evt)
{
    const char *const payload_start = M_GetBlockPayloadStartIfAny(evt);
    if (payload_start == nullptr || *payload_start != '{') {
        return false;
    }

    ctx->collecting = true;
    ctx->brace_depth = 0;
    ctx->in_quote = false;
    ctx->escaped = false;
    M_ScanBraceState(
        payload_start, &ctx->brace_depth, &ctx->in_quote, &ctx->escaped);
    if (ctx->brace_depth == 0) {
        ctx->collecting = false;
    }
    return true;
}

static bool M_GetBracedPayload(
    const char *const event_str, const char *const prefix,
    const char **const out_start, size_t *const out_len)
{
    if (strncmp(event_str, prefix, strlen(prefix)) != 0) {
        return false;
    }

    const char *p = M_SkipWhitespaceConst(event_str + strlen(prefix));
    if (*p != '{') {
        return false;
    }

    const char *payload_start = p + 1;
    int32_t depth = 1;
    bool in_quote = false;
    bool escaped = false;
    p++;
    for (; *p != '\0'; p++) {
        if (in_quote) {
            if (escaped) {
                escaped = false;
                continue;
            }
            if (*p == '\\') {
                escaped = true;
                continue;
            }
            if (*p == '"') {
                in_quote = false;
            }
            continue;
        }

        if (*p == '"') {
            in_quote = true;
            continue;
        }
        if (*p == '{') {
            depth++;
            continue;
        }
        if (*p == '}') {
            depth--;
            if (depth == 0) {
                const char *trail = M_SkipWhitespaceConst(p + 1);
                if (*trail != '\0') {
                    return false;
                }
                *out_start = payload_start;
                *out_len = (size_t)(p - payload_start);
                return true;
            }
        }
    }
    return false;
}

static bool M_ParseQuitEvent(const char *const event_str)
{
    if (strcmp(event_str, "quit") != 0) {
        return false;
    }

    if (m_Priv.test_mode.seen) {
        M_TestReportSummary();
        M_TerminateFromTestResult();
    }

    SDL_Event event = { .type = SDL_QUIT };
    Shell_ProcessEvent(&event);
    return true;
}

// Consolidate keydown/keyup parsing into a single helper
static bool M_ParseKeyEvent(
    const char *event_str, SDL_EventType type, const char *prefix)
{
    if (strncmp(event_str, prefix, strlen(prefix)) != 0) {
        return false;
    }
    SDL_Event event = { .type = type };
    const char *p = event_str + strlen(prefix);
    const char *start = strchr(p, '"');
    const char *end = start ? strrchr(start + 1, '"') : nullptr;
    if (!start || !end || end <= start + 1) {
        LOG_WARNING("Malformed %s instruction: %s", prefix, event_str);
        return false;
    }
    const size_t slen = end - (start + 1);
    const char *desc = String_FormatStatic("%.*s", slen, start + 1);
    SDL_Keymod mod;
    if (!Input_ParseKeyDesc(desc, &event.key.keysym.scancode, &mod)) {
        return false;
    }
    event.key.keysym.mod = mod;
    event.key.keysym.sym = SDL_GetKeyFromScancode(event.key.keysym.scancode);
    Shell_ProcessEvent(&event);
    return true;
}

static bool M_ParseKeyDownEvent(const char *event_str)
{
    return M_ParseKeyEvent(event_str, SDL_KEYDOWN, "●");
}

static bool M_ParseKeyUpEvent(const char *event_str)
{
    return M_ParseKeyEvent(event_str, SDL_KEYUP, "○");
}

static bool M_ParseTextInputEvent(const char *const event_str)
{
    SDL_Event event = { .type = SDL_TEXTINPUT };
    const char *const fmt = String_FormatStatic(
        "text-input \"%%%d[^\"]\"", SDL_TEXTEDITINGEVENT_TEXT_SIZE - 1);
    if (sscanf(event_str, fmt, &event.text.text) != 1) {
        return false;
    }
    Shell_ProcessEvent(&event);
    return true;
}

static bool M_ParseNoopEvent(const char *const event_str)
{
    // No-op event for inline comments and empty frame markers
    if (strncmp(event_str, "noop", 4) != 0) {
        return false;
    }
    return true;
}

static bool M_ParseTestCaseEvent(const char *const event_str)
{
    M_PRIV *const p = &m_Priv;
    const char *name_start = nullptr;
    size_t name_len = 0;
    if (!M_ParseQuotedPayload(event_str, "testcase ", &name_start, &name_len)) {
        return false;
    }

    p->test_mode.seen = true;
    M_ApplyQuietInTestMode();

    if (p->test_mode.case_active) {
        M_EndTestCase();
    }

    Memory_FreePointer(&p->test_mode.case_name);
    p->test_mode.case_name = String_Format("%.*s", (int)name_len, name_start);
    p->test_mode.case_checks = 0;
    p->test_mode.case_fails = 0;
    p->test_mode.case_active = true;
    return true;
}

static bool M_ParseTestCaseHeader(const char *const line, M_PARSE_CTX *const)
{
    return M_ParseTestCaseEvent(line);
}

static bool M_ParseExpectEvent(const char *const event_str)
{
    M_PRIV *const p = &m_Priv;
    const char *const prefix = "expect ";
    if (strncmp(event_str, prefix, strlen(prefix)) != 0) {
        return false;
    }
    const char *expr_start = nullptr;
    size_t expr_len = 0;
    if (!M_GetBracedPayload(event_str, prefix, &expr_start, &expr_len)) {
        expr_start = M_SkipWhitespaceConst(event_str + strlen(prefix));
        if (*expr_start == '\0') {
            return false;
        }
        expr_len = strlen(expr_start);
    }

    p->test_mode.seen = true;
    M_ApplyQuietInTestMode();

    if (!p->test_mode.case_active) {
        M_TestPrint(
            "%sFAIL%s | expect outside test case",
            M_TestColor(LOG_ANSI_COLOR_RED), M_TestColorReset());
        p->test_mode.checks_failed++;
        p->test_mode.cases_failed++;
        return true;
    }

    char *const expr = String_Format("%.*s", (int)expr_len, expr_start);
    char *const script = String_Format(
        "if not ((function() return %s\n end)()) then error('expect failed') "
        "end",
        expr);
    LUA_RESULT eval_result = Lua_Eval(script);

    p->test_mode.case_checks++;
    if (eval_result.code == LUA_OK) {
        p->test_mode.checks_passed++;
    } else {
        p->test_mode.checks_failed++;
        p->test_mode.case_fails++;
        M_TestPrint(
            "%sFAIL%s | %s | expect \"%s\" | %s",
            M_TestColor(LOG_ANSI_COLOR_RED), M_TestColorReset(),
            p->test_mode.case_name, expr,
            eval_result.message != nullptr ? eval_result.message : "lua error");
    }

    Lua_FreeResult(&eval_result);
    Memory_Free(script);
    Memory_Free(expr);
    return true;
}

static bool M_ParseCommandEvent(const char *const event_str)
{
    const char *const prefix = "cmd ";
    if (strncmp(event_str, prefix, strlen(prefix)) != 0) {
        return false;
    }

    const char *payload_start = nullptr;
    size_t payload_len = 0;

    if (M_GetBracedPayload(event_str, prefix, &payload_start, &payload_len)) {
        char *const cmd_str =
            String_Format("%.*s", (int)payload_len, payload_start);
        M_TrimWhitespaceInPlace(cmd_str);
        Console_Eval(cmd_str);
        Memory_Free(cmd_str);
        return true;
    }

    if (M_ParseQuotedPayload(event_str, prefix, &payload_start, &payload_len)) {
        char *const cmd_str =
            String_Format("%.*s", (int)payload_len, payload_start);
        Console_Eval(cmd_str);
        Memory_Free(cmd_str);
        return true;
    }

    payload_start = M_SkipWhitespaceConst(event_str + strlen(prefix));
    payload_len = strlen(payload_start);
    if (payload_len == 0) {
        LOG_WARNING("Malformed cmd instruction: %s", event_str);
        return false;
    }

    char *const cmd_str =
        String_Format("%.*s", (int)payload_len, payload_start);
    M_TrimWhitespaceInPlace(cmd_str);
    if (cmd_str[0] == '\0') {
        LOG_WARNING("Malformed cmd instruction: %s", event_str);
        Memory_Free(cmd_str);
        return false;
    }
    Console_Eval(cmd_str);
    Memory_Free(cmd_str);
    return true;
}

static bool M_ParseLuaEvent(const char *const event_str)
{
    M_PRIV *const p = &m_Priv;
    if (strncmp(event_str, "lua ", 4) != 0) {
        return false;
    }

    const char *chunk_start = nullptr;
    size_t chunk_len = 0;
    LUA_RESULT eval_result = {};
    if (M_GetBracedPayload(event_str, "lua ", &chunk_start, &chunk_len)) {
        char *const chunk = String_Format("%.*s", (int)chunk_len, chunk_start);
        eval_result = Lua_Eval(chunk);
        Memory_Free(chunk);
    } else {
        eval_result = Lua_Eval(event_str + 4);
    }
    if (eval_result.code == LUA_ERRSYNTAX) {
        LOG_ERROR(
            "LUA syntax error on frame %d: %s", p->frame_idx,
            eval_result.message);
        Shell_Terminate(1);
    } else if (eval_result.code != LUA_OK) {
        LOG_ERROR(
            "LUA error on frame %d: %s", p->frame_idx, eval_result.message);
        Shell_Terminate(1);
    }
    Lua_FreeResult(&eval_result);
    return true;
}

static bool M_ParseEvent(const char *const event_str)
{
    for (int32_t i = 0; m_EventHandlers[i] != nullptr; i++) {
        if (m_EventHandlers[i](event_str)) {
            return true;
        }
    }
    return false;
}

static bool M_ParseSeedControl(const char *const line, M_PARSE_CTX *const ctx)
{
    int32_t val;
    if (sscanf(line, "seed_control %d", &val) == 1) {
        Random_SeedControl(val);
        return true;
    }
    return false;
}

static bool M_ParseSeedDraw(const char *const line, M_PARSE_CTX *const ctx)
{
    int32_t val;
    if (sscanf(line, "seed_draw %d", &val) == 1) {
        Random_SeedDraw(val);
        return true;
    }
    return false;
}

static bool M_ParseBindKeyboard(const char *const line, M_PARSE_CTX *const ctx)
{
    const char *prefix = "bind keyboard ";
    if (strncmp(line, prefix, strlen(prefix)) != 0) {
        return false;
    }
    const char *p = line + strlen(prefix);
    const char *q = strchr(p, ' ');
    if (q == nullptr) {
        return false;
    }
    const char *role_str = String_FormatStatic("%.*s", (int)(q - p), p);
    const INPUT_ROLE role = ENUM_MAP_GET(INPUT_ROLE, role_str, -1);
    if (role == (INPUT_ROLE)-1) {
        return false;
    }
    const char *const start = strchr(p, '"');
    const char *const end = start ? strrchr(start + 1, '"') : nullptr;
    if (start == nullptr || end == nullptr || end <= start + 1) {
        LOG_WARNING("Malformed bind keyboard instruction: %s", line);
        return false;
    }
    const size_t slen = end - (start + 1);
    const char *desc = String_FormatStatic("%.*s", slen, start + 1);
    SDL_Scancode sc;
    SDL_Keymod mod;
    if (!Input_ParseKeyDesc(desc, &sc, &mod)) {
        return false;
    }
    JSON_OBJECT *const bind = JSON_ObjectNew();
    JSON_ObjectAppendInt(bind, "scancode", sc);
    JSON_ObjectAppendInt(bind, "mod", mod);
    g_Input_Keyboard.assign_from_json_object(
        g_Config.input.keyboard_layout, role, bind);
    JSON_ObjectFree(bind);
    return true;
}

static bool M_ParseBindController(
    const char *const line, M_PARSE_CTX *const ctx)
{
    const char *prefix = "bind controller ";
    if (strncmp(line, prefix, strlen(prefix)) != 0) {
        return false;
    }
    const char *p = line + strlen(prefix);
    const char *q = strchr(p, ' ');
    if (q == nullptr) {
        return false;
    }
    const char *role_str = String_FormatStatic("%.*s", (int)(q - p), p);
    const INPUT_ROLE role =
        (INPUT_ROLE)ENUM_MAP_GET(INPUT_ROLE, role_str, (int32_t)(INPUT_ROLE)-1);
    if (role == (INPUT_ROLE)-1) {
        return false;
    }
    int32_t bt, b, ad;
    if (sscanf(q + 1, "%d %d %d", &bt, &b, &ad) == 3) {
        JSON_OBJECT *bind = JSON_ObjectNew();
        JSON_ObjectAppendInt(bind, "button_type", bt);
        JSON_ObjectAppendInt(bind, "bind", b);
        JSON_ObjectAppendInt(bind, "axis_dir", ad);
        g_Input_Controller.assign_from_json_object(
            g_Config.input.controller_layout, role, bind);
        JSON_ObjectFree(bind);
        return true;
    }
    return false;
}

static bool M_ParseArgs(const char *const line, M_PARSE_CTX *const ctx)
{
    if (strncmp(line, "args", 4) != 0) {
        return false;
    }

    if (ctx->args != nullptr) {
        Shell_FreeArgs(ctx->args);
        ctx->args = nullptr;
    }
    // Build an owned argv vector for Shell_ParseArgs adoption.
    VECTOR *raw_args = Vector_Create(sizeof(const char *));
    const char *p = line + 4;
    while (*p != '\0') {
        while (isspace((unsigned char)*p)) {
            p++;
        }
        if (*p != '"') {
            break;
        }

        p++;
        const char *start = p;
        while (*p != '\0' && *p != '"') {
            p++;
        }
        const ptrdiff_t len = p - start;
        char tmp[len + 1];
        memcpy(tmp, start, len);
        tmp[len] = '\0';
        char *arg = Memory_DupStr(tmp);
        Vector_Add(raw_args, &arg);
        if (*p == '"') {
            p++;
        }
    }
    ctx->args = Shell_ParseArgs(raw_args);
    return true;
}

static bool M_ParseConfig(const char *const line, M_PARSE_CTX *const ctx)
{
    char keybuf[64];
    char valbuf[128];
    if (sscanf(line, "config %63s %127s", keybuf, valbuf) == 2) {
        // Strip surrounding quotes from the value, if present
        size_t vlen = strlen(valbuf);
        if (vlen >= 2 && valbuf[0] == '"' && valbuf[vlen - 1] == '"') {
            valbuf[vlen - 1] = '\0';
            memmove(valbuf, valbuf + 1, vlen - 1);
        }
        const CONFIG_OPTION *opt = Config_GetOptionByPath(keybuf);
        if (opt) {
            Config_SetOptionValueFromString(opt, valbuf);
        } else {
            LOG_WARNING("Unknown option: %s", keybuf);
        }
        return true;
    }
    return false;
}

static void M_StripInlineComment(char *const line)
{
    bool in_quote = false;
    char *p;
    for (p = line; *p != '\0'; p++) {
        if (*p == '"') {
            in_quote = !in_quote;
        } else if (*p == '#' && !in_quote) {
            *p = '\0';
            break;
        }
    }
    // Trim trailing whitespace
    {
        char *end = line + strlen(line);
        while (end > line && (end[-1] == ' ' || end[-1] == '\t')) {
            end[-1] = '\0';
            end--;
        }
    }
}

static char *M_SkipWhitespace(char *const line)
{
    char *start = line;
    while (*start == ' ' || *start == '\t') {
        start++;
    }
    return start;
}

static bool M_IsFrameMarkerLine(const char *const line)
{
    int32_t delta = 0;
    return sscanf(line, "@+%d:", &delta) == 1;
}

SHELL_ARGS *TestReplay_Open(const char *path)
{
    M_PRIV *const p = &m_Priv;

    Memory_FreePointer(&p->test_mode.case_name);
    memset(p, 0, sizeof(m_Priv));
    p->replay_quiet = Log_GetMinLevel() >= LOG_LEVEL_WARNING;
    p->test_mode.log_level_before_quiet = Log_GetMinLevel();
    p->test_mode.use_ansi_colors = Log_ShouldUseAnsiColors();
    p->test_mode.exit_code_override = -1;

    char *data = nullptr;
    size_t size = 0;
    if (!File_Load(path, &data, &size)) {
        Shell_ExitSystemFmt("Cannot open replay file '%s'", path);
        return nullptr;
    }
    p->data = data;
    p->size = size;

    // Split file into lines by replacing '\n' with '\0'
    char *end = data + size;
    for (char *ch = data; ch < end; ch++) {
        if (*ch == '\n') {
            *ch = '\0';
        }
    }

    // Collect non-empty, comment-stripped lines
    VECTOR *lines = Vector_Create(sizeof(char *));
    for (char *line = data; line < end;) {
        char *const next_line = line + strlen(line) + 1;
        M_StripInlineComment(line);
        char *start = M_SkipWhitespace(line);
        if (*start != '\0') {
            Vector_Add(lines, &start);
        }
        line = next_line;
    }

    // Parse and execute headers
    p->headers = Vector_Create(sizeof(char *));
    int32_t idx = 0;
    while (idx < lines->count) {
        char *const ln = *(char **)Vector_Get(lines, idx);
        int32_t delta = 0;
        if (sscanf(ln, "@+%d:", &delta) == 1) {
            break;
        }
        Vector_Add(p->headers, &ln);
        idx++;
    }

    // Parse frames and their events
    p->frames = Vector_Create(sizeof(M_FRAME));
    p->next_frame_idx = 0;
    p->frame_idx = 0;
    int32_t last_frame = 0;
    while (idx < lines->count) {
        char *ln = *(char **)Vector_Get(lines, idx);
        int32_t delta = 0;
        if (sscanf(ln, "@+%d:", &delta) == 1) {
            M_FRAME frame = {
                .frame_idx = last_frame + delta,
                .events = Vector_Create(sizeof(char *)),
            };
            M_BLOCK_EVENT_CTX block_ctx = {};

            // Primary event on same line
            char *const colon = strchr(ln, ':');
            if (colon != nullptr) {
                char *const evt = M_SkipWhitespace(colon + 1);
                if (*evt != '\0') {
                    Vector_Add(frame.events, &evt);
                    M_TryStartBlockEvent(&block_ctx, evt);
                }
            }

            // Continued events
            idx++;
            while (idx < lines->count) {
                char *const cont = *(char **)Vector_Get(lines, idx);
                if (M_IsFrameMarkerLine(cont)) {
                    // Reached next frame - stop
                    break;
                }

                char *const evt = M_SkipWhitespace(cont);
                if (*evt == '\0') {
                    idx++;
                    continue;
                }

                if (block_ctx.collecting) {
                    if (evt > p->data) {
                        evt[-1] = '\n';
                    }
                    M_ScanBraceState(
                        evt, &block_ctx.brace_depth, &block_ctx.in_quote,
                        &block_ctx.escaped);
                    if (block_ctx.brace_depth == 0) {
                        block_ctx.collecting = false;
                    }
                    idx++;
                    continue;
                }

                Vector_Add(frame.events, &evt);
                M_TryStartBlockEvent(&block_ctx, evt);
                idx++;
            }
            Vector_Add(p->frames, &frame);
            last_frame = frame.frame_idx;
            continue;
        }
        idx++;
    }

    M_PARSE_CTX ctx = {};
    for (int32_t i = 0; i < p->headers->count; i++) {
        const char *const ln = *(const char **)Vector_Get(p->headers, i);
        M_ParseArgs(ln, &ctx);
    }
    Vector_Free(lines);
    LOG_INFO("Loaded %zu frames for playback", p->frames->count);
    if (ctx.args == nullptr) {
        ctx.args = Shell_ParseArgs(nullptr);
    }
    if (ctx.args != nullptr && ctx.args->quiet) {
        p->replay_quiet = true;
    }
    return ctx.args;
}

void TestReplay_Start(void)
{
    M_PARSE_CTX ctx = {};
    M_PRIV *const p = &m_Priv;
    for (int32_t i = 0; i < p->headers->count; i++) {
        const char *const ln = *(const char **)Vector_Get(p->headers, i);
        bool handled = false;
        for (int32_t j = 0; m_HeaderHandlers[j]; j++) {
            if (m_HeaderHandlers[j](ln, &ctx)) {
                handled = true;
                break;
            }
        }
        if (!handled) {
            LOG_WARNING("Unknown line: %s", ln);
        }
    }
    g_SavedConfig = g_Config;
    Shell_FreeArgs(ctx.args);
}

void TestReplay_Close(void)
{
    M_PRIV *const p = &m_Priv;
    M_TestReportSummary();

    if (p->test_mode.quiet_applied) {
        Log_SetMinLevel(p->test_mode.log_level_before_quiet);
        p->test_mode.quiet_applied = false;
    }

    Memory_FreePointer(&p->test_mode.case_name);
    if (p->headers) {
        Vector_Free(p->headers);
        p->headers = nullptr;
    }
    if (p->frames) {
        for (int32_t i = 0; i < p->frames->count; i++) {
            M_FRAME *const f = Vector_Get(p->frames, i);
            Vector_Free(f->events);
        }
        Vector_Free(p->frames);
        p->frames = nullptr;
    }
    if (p->data) {
        Memory_Free(p->data);
        p->data = nullptr;
    }
}

bool TestReplay_IsOpened(void)
{
    M_PRIV *const p = &m_Priv;
    return p->frames != nullptr;
}

void TestReplay_RunFrame(void)
{
    M_PRIV *const p = &m_Priv;
    if (!TestReplay_IsOpened()) {
        return;
    }
    while (p->next_frame_idx < p->frames->count) {
        M_FRAME *const f = Vector_Get(p->frames, p->next_frame_idx);
        if (f->frame_idx != p->frame_idx) {
            break;
        }
        for (int32_t j = 0; j < f->events->count; j++) {
            const char *const evt = *(char **)Vector_Get(f->events, j);
            if (!M_ParseEvent(evt)) {
                LOG_WARNING(
                    "Unknown replay event on frame %d: %s", p->frame_idx, evt);
            }
        }
        p->next_frame_idx++;
    }
    p->frame_idx++;

    if (p->test_mode.seen && p->next_frame_idx >= p->frames->count) {
        M_TestReportSummary();
        M_TerminateFromTestResult();
    }
}

int32_t TestReplay_GetExitCodeOverride(void)
{
    return m_Priv.test_mode.exit_code_override;
}
