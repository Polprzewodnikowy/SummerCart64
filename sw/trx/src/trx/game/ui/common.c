#include <trx/game/ui/common.h>

#include <trx/config.h>
#include <trx/core/memory.h>
#include <trx/debug.h>
#include <trx/game/console/common.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/ui/draw.h>
#include <trx/game/ui/elements/anchor.h>
#include <trx/game/ui/events.h>
#include <trx/game/ui/scaler.h>
#include <trx/game/ui/text.h>
#include <trx/game/viewport.h>

#include <SDL2/SDL.h>
#include <string.h>

static struct {
    MEMORY_ARENA_ALLOCATOR alloc;
    UI_NODE *root; // The top-level container
    UI_NODE *current; // The current container into which we attach nodes
} m_Priv = {
    .alloc = {
        .default_chunk_size = 1024 * 4,
    },
};

extern void UI_ClearDraw(void);

static UI_INPUT M_TranslateInput(const uint32_t system_keycode)
{
    // clang-format off
    switch (system_keycode) {
    case SDLK_UP:        return UI_KEY_UP;
    case SDLK_DOWN:      return UI_KEY_DOWN;
    case SDLK_LEFT:      return UI_KEY_LEFT;
    case SDLK_RIGHT:     return UI_KEY_RIGHT;
    case SDLK_HOME:      return UI_KEY_HOME;
    case SDLK_END:       return UI_KEY_END;
    case SDLK_BACKSPACE: return UI_KEY_BACK;
    case SDLK_RETURN:    return UI_KEY_RETURN;
    case SDLK_ESCAPE:    return UI_KEY_ESCAPE;
    }
    // clang-format on
    return -1;
}

// Depth-first measure pass
static void M_MeasureNode(UI_NODE *const node)
{
    if (node == nullptr || node->ops.measure == nullptr) {
        return;
    }

    // Recurse to children
    UI_NODE *child = node->first_child;
    while (child != nullptr) {
        M_MeasureNode(child);
        child = child->next_sibling;
    }

    node->ops.measure(node);
}

// Depth-first layout pass
static void M_LayoutNode(
    UI_NODE *const node, const float x, const float y, const float w,
    const float h)
{
    if (node == nullptr || node->ops.layout == nullptr) {
        return;
    }

    node->ops.layout(node, x, y, w, h);
    // Recursing to children is a responsibility of the layout function.
}

// Depth-first draw pass
static void M_DrawNode(const UI_NODE *const node)
{
    if (node == nullptr || node->ops.draw == nullptr) {
        return;
    }

    node->ops.draw(node);
    // Recursing to children is a responsibility of the draw function.
}

// Allocate a new node
UI_NODE *UI_AllocNode(
    const UI_WIDGET_OPS *const ops, const size_t additional_size)
{
    const size_t size =
        Memory_Align(sizeof(UI_NODE)) + Memory_Align(additional_size);
    UI_NODE *const node = Memory_ArenaAlloc(&m_Priv.alloc, size);
    memset(node, 0, size);
    node->ops = *ops;
    node->data = (char *)node + Memory_Align(sizeof(UI_NODE));
    return node;
}

// Attach child to parent's child list
void UI_AddChild(UI_NODE *const child)
{
    // Special case - the root widget
    if (m_Priv.root == nullptr) {
        m_Priv.root = child;
        return;
    }

    UI_NODE *const parent = m_Priv.current;
    if (parent == nullptr || child == nullptr) {
        return;
    }
    child->parent = parent;
    if (parent->first_child == nullptr) {
        parent->first_child = child;
    } else {
        parent->last_child->next_sibling = child;
    }
    parent->last_child = child;
}

void UI_PushCurrent(UI_NODE *const child)
{
    m_Priv.current = child;
}

void UI_PopCurrent(void)
{
    ASSERT(m_Priv.current != nullptr);
    m_Priv.current = m_Priv.current->parent;
    if (m_Priv.current == nullptr) {
        m_Priv.root = nullptr;
    }
}

const UI_NODE *UI_GetCurrent(void)
{
    return m_Priv.current;
}

// Scene management
void UI_BeginScene(void)
{
    UI_ClearDraw();
    Memory_ArenaReset(&m_Priv.alloc);
    UI_BeginAnchor(0.5f, 0.5f); // Make a root node.
}

void UI_EndScene(void)
{
    M_MeasureNode(m_Priv.root);
    M_LayoutNode(m_Priv.root, 0, 0, UI_GetCanvasWidth(), UI_GetCanvasHeight());
    M_DrawNode(m_Priv.root);
    UI_EndAnchor();
    ASSERT(m_Priv.root == nullptr);
}

void UI_Init(void)
{
    UI_InitEvents();
    UI_InitText();
    UI_InitDraw();
}

void UI_Shutdown(void)
{
    UI_ShutdownDraw();
    UI_ShutdownText();
    Memory_ArenaFree(&m_Priv.alloc);
    UI_ShutdownEvents();
}

void UI_ToggleState(bool *const config_setting)
{
    *config_setting ^= true;
    Config_Update();
    Console_Log(
        *config_setting ? GS("general/osd/ui_on") : GS("general/osd/ui_off"));
}

void UI_HandleKeyDown(const uint32_t key)
{
    UI_FireEvent((EVENT) {
        .name = "key_down",
        .sender = nullptr,
        .data = (void *)M_TranslateInput(key),
    });
}

void UI_HandleKeyUp(const uint32_t key)
{
    UI_FireEvent((EVENT) {
        .name = "key_up",
        .sender = nullptr,
        .data = (void *)M_TranslateInput(key),
    });
}

void UI_HandleTextEdit(const char *const text)
{
    UI_FireEvent((EVENT) {
        .name = "text_edit", .sender = nullptr, .data = (void *)text });
}

int32_t UI_GetCanvasWidth(void)
{
    return UI_Scaler_CalcInverse(
        Viewport_GetWidth(VIEWPORT_UI), UI_SCALER_TARGET_GENERIC);
}

int32_t UI_GetCanvasHeight(void)
{
    return UI_Scaler_CalcInverse(
        Viewport_GetHeight(VIEWPORT_UI), UI_SCALER_TARGET_GENERIC);
}

float UI_ScaleX(const float x)
{
    return UI_Scaler_Calc(x * 0x10000, UI_SCALER_TARGET_GENERIC) / 0x10000.p0;
}

float UI_ScaleY(const float y)
{
    return UI_Scaler_Calc(y * 0x10000, UI_SCALER_TARGET_GENERIC) / 0x10000.p0;
}
