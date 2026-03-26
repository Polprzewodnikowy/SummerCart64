#include <trx/av/audio.h>
#include <trx/config.h>
#include <trx/debug.h>
#include <trx/game/console/common.h>
#include <trx/game/fmv.h>
#include <trx/game/input/common.h>
#include <trx/game/replay/test_recorder.h>
#include <trx/game/replay/test_replay.h>
#include <trx/game/screenshot.h>
#include <trx/game/shell.h>
#include <trx/game/ui.h>

// If true, next SDL_TEXT* event should be zeroed out.
static bool m_ConsoleJustOpened = false;

static void M_HandleQuit(void)
{
    Shell_ScheduleExit();
}

static void M_HandleKeyDown(const SDL_Event *const event)
{
    // NOTE: Opening the console normally would get handled by Input_Update,
    // but by the time Input_Update gets ran, we may already have lost some
    // keypresses if the player types really fast, so we need to react sooner.
    if (g_Config.gameplay.enable_console && !Console_IsOpened()
        && !Input_IsInListenMode()
        && Input_IsPressedEx(
            INPUT_BACKEND_KEYBOARD, g_Config.input.keyboard_layout,
            INPUT_ROLE_ENTER_CONSOLE)) {
        Console_Open();
        // Zero out the next text event so the console-open glyph never
        // shows up.
        m_ConsoleJustOpened = true;
    } else {
        UI_HandleKeyDown(event->key.keysym.sym);
    }
}

static void M_HandleKeyUp(const SDL_Event *const event)
{
    // NOTE: needs special handling on Windows -
    // SDL_SCANCODE_PRINTSCREEN is not sufficient to react to this.
    if (event->key.keysym.sym == SDLK_PRINTSCREEN) {
        Screenshot_Make(g_Config.rendering.screenshot_format);
    }
}

static void M_HandleFocusGained(void)
{
    if (g_Config.audio.mute_out_of_focus) {
        Audio_Unmute();
    }
}

static void M_HandleFocusLost(void)
{
    if (g_Config.audio.mute_out_of_focus) {
        Audio_Mute();
    }
}

static void M_HandleWindowShown(void)
{
    LOG_DEBUG("");
}

static void M_HandleWindowRestored(void)
{
    Shell_SyncFromWindow(true);
}

static void M_HandleWindowMinimized(void)
{
    LOG_DEBUG("");
}

static void M_HandleWindowMaximized(void)
{
    Shell_SyncFromWindow(true);
}

static void M_HandleWindowMoved(const int32_t x, const int32_t y)
{
    Shell_SyncFromWindow(false);
}

static void M_HandleWindowResized(int32_t width, int32_t height)
{
    Shell_SyncFromWindow(true);
}

static bool M_ProcessReplayEvent(const SDL_Event *const event)
{
    switch (event->type) {
    case SDL_QUIT:
        M_HandleQuit();
        return true;
    }
    return false;
}

bool Shell_ProcessEvent(const SDL_Event *const event)
{
    Input_ProcessEvent(event);

    switch (event->type) {
    case SDL_QUIT:
        M_HandleQuit();
        return true;

    case SDL_KEYDOWN:
        M_HandleKeyDown(event);
        return true;

    case SDL_KEYUP:
        M_HandleKeyUp(event);
        return true;

    case SDL_TEXTINPUT:
        if (m_ConsoleJustOpened) {
            m_ConsoleJustOpened = false;
        } else {
            UI_HandleTextEdit(event->text.text);
        }
        return true;

    case SDL_CONTROLLERDEVICEADDED:
    case SDL_JOYDEVICEADDED:
    case SDL_CONTROLLERDEVICEREMOVED:
    case SDL_JOYDEVICEREMOVED:
        Input_Discover();
        return true;

    case SDL_WINDOWEVENT:
        switch (event->window.event) {
        case SDL_WINDOWEVENT_SHOWN:
            M_HandleWindowShown();
            break;

        case SDL_WINDOWEVENT_FOCUS_GAINED:
            M_HandleFocusGained();
            break;

        case SDL_WINDOWEVENT_FOCUS_LOST:
            M_HandleFocusLost();
            break;

        case SDL_WINDOWEVENT_RESTORED:
            M_HandleWindowRestored();
            break;

        case SDL_WINDOWEVENT_MINIMIZED:
            M_HandleWindowMinimized();
            break;

        case SDL_WINDOWEVENT_MAXIMIZED:
            M_HandleWindowMaximized();
            break;

        case SDL_WINDOWEVENT_MOVED:
            M_HandleWindowMoved(event->window.data1, event->window.data2);
            break;

        case SDL_WINDOWEVENT_RESIZED:
            M_HandleWindowResized(event->window.data1, event->window.data2);
            break;
        }
        break;
    }

    return false;
}

void Shell_ProcessEvents(void)
{
    SDL_Event event;
    if (TestReplay_IsOpened()) {
        TestReplay_RunFrame();
        while (SDL_PollEvent(&event) != 0) {
            M_ProcessReplayEvent(&event);
        }
        return;
    }

    if (TestRecorder_IsOpened()) {
        TestRecorder_BeginFrame();
    }
    while (SDL_PollEvent(&event) != 0) {
        TestRecorder_RecordEvent(&event);
        Shell_ProcessEvent(&event);
    }
    if (TestRecorder_IsOpened()) {
        TestRecorder_EndFrame();
    }
}
