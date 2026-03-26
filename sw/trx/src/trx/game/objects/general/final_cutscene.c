#include <trx/game/const.h>
#include <trx/game/game.h>
#include <trx/game/objects.h>
#include <trx/game/objects/general/combat_end.h>
#include <trx/game/output.h>

#define M_CUTSCENE_DURATION (15 * LOGIC_FPS)
#define M_FADE_DURATION (3 * LOGIC_FPS)

static int32_t m_FadeTimer = -1;

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (CombatEnd_IsComplete()) {
        item->status = IS_ACTIVE;
        Item_Animate(item);

        if (m_FadeTimer == -1) {
            m_FadeTimer = M_CUTSCENE_DURATION;
        } else if (m_FadeTimer > 0) {
            m_FadeTimer--;
        }
    } else {
        item->status = IS_INVISIBLE;
        m_FadeTimer = -1;
    }
}

static bool M_Draw(const ITEM *const item)
{
    Object_DrawAnimatingItem(item);

    if (m_FadeTimer < 0 || m_FadeTimer > M_FADE_DURATION) {
        return true;
    }
    const float opacity = 1.0f - (m_FadeTimer / (float)M_FADE_DURATION);
    Output_Overlay_DrawBlackRectangle(opacity, false);
    return true;
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->draw_func = M_Draw;
    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_CUT_SHOTGUN, M_Setup)
