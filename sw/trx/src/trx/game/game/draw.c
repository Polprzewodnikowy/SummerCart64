#include <trx/config.h>
#include <trx/game/camera.h>
#include <trx/game/interpolation.h>
#include <trx/game/output.h>
#include <trx/game/overlay.h>

void Game_Draw(const bool draw_overlay)
{
    Interpolation_Interpolate();
    Camera_Apply();
    Room_DrawAllRooms(g_Camera.interp.room_num, g_Camera.target.room_num);
    if (draw_overlay) {
        Overlay_DrawGameInfo();
    }
    SceneCompositor_Flush();
    if (g_Config.visuals.enable_reflections) {
        Output_Textures_UpdateEnvironmentMap();
    }
}
