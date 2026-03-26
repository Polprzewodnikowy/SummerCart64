#include <trx/config.h>
#include <trx/core/log.h>
#include <trx/game/game_strings/manager.h>
#include <trx/game/lara.h>
#include <trx/game/music.h>
#include <trx/game/output.h>
#include <trx/game/replay/test_replay.h>
#include <trx/game/savegame.h>
#include <trx/game/shell.h>
#include <trx/game/sound.h>
#include <trx/game/viewport.h>

#include <SDL2/SDL_timer.h>

static Uint64 m_UpdateDebounce = 0;
static bool m_IgnoreConfigChanges = false;
static SHELL_SIZE m_ViewportSize = { .w = -1, .h = -1 };

static bool M_MustUpdateRendererViewport(void)
{
    const SHELL_SIZE size = Shell_GetCurrentSize();
    return m_ViewportSize.w != size.w || m_ViewportSize.h != size.h;
}

void Shell_RefreshRendererViewport(void)
{
    Viewport_Reset();
    m_ViewportSize = Shell_GetCurrentSize();
}

void Shell_SyncToWindow(void)
{
    m_UpdateDebounce = SDL_GetTicks();

    LOG_DEBUG(
        "is_fullscreen=%d is_maximized=%d x=%d y=%d width=%d height=%d",
        g_Config.window.is_fullscreen, g_Config.window.is_maximized,
        g_Config.window.x, g_Config.window.y, g_Config.window.width,
        g_Config.window.height);

    SDL_Window *const window = Shell_GetWindow();
    if (g_Config.window.is_fullscreen) {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        SDL_ShowCursor(SDL_DISABLE);
    } else if (g_Config.window.is_maximized) {
        SDL_SetWindowFullscreen(window, 0);
        SDL_MaximizeWindow(window);
        SDL_ShowCursor(SDL_ENABLE);
    } else {
        int32_t x = g_Config.window.x;
        int32_t y = g_Config.window.y;
        int32_t width = g_Config.window.width;
        int32_t height = g_Config.window.height;
        if (width <= 0 || height <= 0) {
            width = 1280;
            height = 720;
        }

        // Handle default position
        if (x == -1 && y == -1) {
            SDL_DisplayMode display_mode;
            SDL_GetCurrentDisplayMode(0, &display_mode);
            x = (display_mode.w - width) / 2;
            y = (display_mode.h - height) / 2;
        } else {
            // Adjust window position if completely offscreen
            bool on_screen = false;
            const int32_t num_displays = SDL_GetNumVideoDisplays();
            for (int32_t i = 0; i < num_displays; i++) {
                SDL_Rect bounds;
                SDL_GetDisplayBounds(i, &bounds);
                if (x + width > bounds.x && x < bounds.x + bounds.w
                    && y + height > bounds.y && y < bounds.y + bounds.h) {
                    on_screen = true;
                    break;
                }
            }
            if (!on_screen) {
                x = 0;
                y = 0;
                // Find the first display to reposition the window
                SDL_Rect bounds;
                SDL_GetDisplayBounds(0, &bounds);
                x = bounds.x + (bounds.w - width) / 2;
                y = bounds.y + (bounds.h - height) / 2;
            }
        }

        SDL_SetWindowFullscreen(window, 0);
        SDL_SetWindowPosition(window, x, y);
        SDL_SetWindowSize(window, width, height);
        SDL_ShowCursor(SDL_ENABLE);
    }
}

void Shell_SyncFromWindow(const bool update_viewport)
{
    // Determine if this call should sync config, i.e., skip immediate
    // programmatic events
    const Uint32 now = SDL_GetTicks();
    const bool skip_config = (now - m_UpdateDebounce) < 500;

    // Always pull current window state for logging and viewport reset
    SDL_Window *const window = Shell_GetWindow();
    const Uint32 window_flags = SDL_GetWindowFlags(window);
    const bool is_maximized = window_flags & SDL_WINDOW_MAXIMIZED;
    int32_t x, y;
    int32_t width, height;
    SDL_GetWindowSize(window, &width, &height);
    SDL_GetWindowPosition(window, &x, &y);
    LOG_TRACE("%dx%d+%d,%d (maximized: %d)", width, height, x, y, is_maximized);

    // Update config only when not in debounce window
    if (!skip_config) {
        g_Config.window.is_maximized = is_maximized;
        if (!is_maximized && !g_Config.window.is_fullscreen) {
            g_Config.window.x = x;
            g_Config.window.y = y;
            g_Config.window.width = width;
            g_Config.window.height = height;
        } else {
            g_Config.window.fs_width = width;
            g_Config.window.fs_height = height;
        }
        if (g_Config.loaded) {
            m_IgnoreConfigChanges = true;
            Config_Update();
            m_IgnoreConfigChanges = false;
        }
    }

    if (update_viewport || M_MustUpdateRendererViewport()) {
        // Refresh viewport to reflect the actual window size
        Shell_RefreshRendererViewport();
    }
}

void Shell_HandleConfigChange(const CONFIG *const old, const CONFIG *const new)
{
    if (!TestReplay_IsOpened()) {
        Config_Write();
    }

#define L_CHANGED(subject) (old->subject != new->subject)

    if (L_CHANGED(audio.sound_volume)) {
        Sound_SetMasterVolume(g_Config.audio.sound_volume);
    }
    if (L_CHANGED(audio.master_volume) || L_CHANGED(audio.music_volume)
        || L_CHANGED(audio.ambient_volume)) {
        Music_SetVolume(g_Config.audio.music_volume);
    }

    if (L_CHANGED(language)) {
        GameStringManager_ReloadLanguage(g_Config.language);
    }

    if (L_CHANGED(window.is_fullscreen) || L_CHANGED(window.is_maximized)
        || L_CHANGED(window.width) || L_CHANGED(window.height)
        || L_CHANGED(window.fs_width) || L_CHANGED(window.fs_height)
        || L_CHANGED(rendering.upscaling_factor) || L_CHANGED(rendering.borders)
        || L_CHANGED(rendering.aspect_mode)) {
        if (!m_IgnoreConfigChanges) {
            Shell_SyncToWindow();
        }
        Shell_RefreshRendererViewport();
    }

    if (L_CHANGED(visuals.fog_start) || L_CHANGED(visuals.fog_end)
        || L_CHANGED(visuals.fog_color.g) || L_CHANGED(visuals.fog_color.b)
        || L_CHANGED(visuals.fog_color.r) || L_CHANGED(visuals.fog_transparency)
        || L_CHANGED(visuals.water_color.g) || L_CHANGED(visuals.water_color.b)
        || L_CHANGED(visuals.water_color.r)) {
        Output_ApplyLevelSettings();
    }

    if (L_CHANGED(visuals.enable_braid) || L_CHANGED(visuals.sunglasses_mode)) {
        Lara_Skin_ApplyOutfit();
    }
    if (L_CHANGED(visuals.lara_outfit)) {
        Lara_Skin_ApplyOutfitFromConfig();
    }

    if (L_CHANGED(rendering.upscaling_filter)
        || L_CHANGED(rendering.enable_wireframe)
        || L_CHANGED(rendering.wireframe_width)
        || L_CHANGED(rendering.enable_vsync)
        || L_CHANGED(rendering.anisotropy_filter)) {
        Output_ApplyRenderSettings();
    }

    if (L_CHANGED(visuals.fov)) {
        if (Viewport_GetSystemFOV() == -1) {
            Viewport_AlterFOV(-1, FOV_MODE_GAME);
        }
    }

    if ((L_CHANGED(gameplay.maximum_save_slots)
         || L_CHANGED(gameplay.maximum_quick_save_slots))
        && Savegame_IsInitialised()) {
        Savegame_Shutdown();
        Savegame_Init();
        Savegame_ScanSavedGames();
    }
#undef L_CHANGED
}
