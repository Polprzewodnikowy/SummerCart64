#include <trx/game/effects/draw.h>

#include <trx/game/effects/const.h>
#include <trx/game/effects/manager.h>
#include <trx/game/matrix.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/version.h>

void Effect_Draw(const int16_t effect_num)
{
    const EFFECT *const effect = Effect_Get(effect_num);
    const OBJECT *const obj = Object_Get(effect->object_id);
    if (!obj->loaded) {
        return;
    }

    // TR3 uses BUBBLES1 as a dummy effect carrier and renders the bubble via
    // an attached spark; keep TR1/TR2 legacy bubble rendering intact.
    if (g_TRVersion == 3 && effect->object_id == O_BUBBLE_1) {
        return;
    }

    if (effect->object_id == O_GLOW) {
        Output_DrawSprite(
            effect->interp.result.pos.x, effect->interp.result.pos.y,
            effect->interp.result.pos.z, Object_Get(O_GLOW)->mesh_idx,
            effect->shade, COLOR_RGB_F_WHITE, DRAW_BLEND);
        return;
    }

    if (obj->effect_draw_func != nullptr) {
        if (obj->effect_draw_func(effect)) {
            return;
        }
    }

    if (obj->mesh_count < 0) {
        const RGB_F tint =
            Object_IsType(effect->object_id, g_WaterSpriteObjects)
            ? COLOR_RGB_F_WHITE
            : Output_GetTint();
        int16_t shade = effect->shade;
        if (shade == -1) {
            Output_CalculateLight(effect->pos, effect->room_num);
            shade = Output_GetLightAdder();
        }
        Output_DrawSprite(
            effect->interp.result.pos.x, effect->interp.result.pos.y,
            effect->interp.result.pos.z, obj->mesh_idx - effect->frame_num,
            shade, tint, DRAW_BLEND);
    } else {
        Matrix_Push();
        Matrix_TranslateAbs32(effect->interp.result.pos);
        Matrix_Rot16(effect->interp.result.rot);
        if (obj->mesh_count != 0) {
            Output_CalculateStaticLight(effect->shade);
            Object_DrawMesh(obj->mesh_idx, -1, false);
        } else {
            Output_CalculateLight(effect->interp.result.pos, effect->room_num);
            Object_DrawMesh(effect->frame_num, -1, false);
        }
        Matrix_Pop();
    }
}
