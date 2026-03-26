#include <trx/game/shell/platform.h>

#ifdef _WIN32
    #include <objbase.h>
    #include <windows.h>
    #include <SDL2/SDL_syswm.h>
    #include <winreg.h>

#endif

#include <SDL2/SDL.h>
#include <libavcodec/version.h>
#include <libavutil/log.h>

void Shell_SetupHiDPI(void)
{
#ifdef _WIN32
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "0");
#endif
}

void Shell_SetupLibAV(void)
{
#ifdef _WIN32
    // necessary for SDL_OpenAudioDevice to work with WASAPI
    // https://www.mail-archive.com/ffmpeg-trac@avcodec.org/msg43300.html
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
#endif

#if LIBAVCODEC_VERSION_MAJOR <= 57
    av_register_all();
#endif

    av_log_set_level(AV_LOG_ERROR);
}

#ifdef _WIN32
// NOTE – taken from SDL3:
// From 8994878767cfb9403f525d12c0770c1e149a4d08 Mon Sep 17 00:00:00 2001
// From: Sam Lantinga <slouken@libsdl.org>
// Date: Tue, 7 Mar 2023 00:01:34 -0800
// Subject: [PATCH] Added SDL_GetSystemTheme() to return whether the system is
//  using a dark or light color theme, and SDL_EVENT_SYSTEM_THEME_CHANGED is
//  sent when this changes

    #ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
        #define DWMWA_USE_IMMERSIVE_DARK_MODE 20
    #endif

// Previous window procedure pointer.
LRESULT(CALLBACK *m_OldWndProc)(HWND, UINT, WPARAM, LPARAM) = nullptr;

static bool M_GetWindowsDarkMode(void)
{
    DWORD type = 0;
    DWORD value = 1;
    DWORD size = sizeof(value);
    const char *const key =
        "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    const LSTATUS status = RegGetValue(
        HKEY_CURRENT_USER, TEXT(key), TEXT("AppsUseLightTheme"),
        RRF_RT_REG_DWORD, &type, &value, &size);
    return (status == ERROR_SUCCESS && value == 0);
}

static void M_ApplyDarkMode(HWND hwnd)
{
    void *dwm = SDL_LoadObject("dwmapi.dll");
    if (dwm == nullptr) {
        return;
    }
    typedef HRESULT(WINAPI * DwmSetWindowAttribute_t)(
        HWND, DWORD, LPCVOID, DWORD);
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wpedantic"
    DwmSetWindowAttribute_t fn =
        (DwmSetWindowAttribute_t)SDL_LoadFunction(dwm, "DwmSetWindowAttribute");
    #pragma GCC diagnostic pop
    if (fn != nullptr) {
        BOOL dark = M_GetWindowsDarkMode() ? TRUE : FALSE;
        fn(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    }
    SDL_UnloadObject(dwm);
}

// Custom window procedure to listen for theme changes.
static LRESULT CALLBACK
M_DarkModeWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_SETTINGCHANGE && wParam == 0 && lParam != 0
        && lstrcmpi((LPCTSTR)lParam, TEXT("ImmersiveColorSet")) == 0) {
        M_ApplyDarkMode(hwnd);
    }
    return CallWindowProc(m_OldWndProc, hwnd, msg, wParam, lParam);
}

void Shell_EnableThemeSupport(SDL_Window *const window)
{
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) {
        return;
    }
    HWND hwnd = info.info.win.window;
    m_OldWndProc = (WNDPROC)SetWindowLongPtr(
        hwnd, GWLP_WNDPROC, (LONG_PTR)M_DarkModeWndProc);
    M_ApplyDarkMode(hwnd);
}

#else
void Shell_EnableThemeSupport(SDL_Window *const window)
{
}
#endif
