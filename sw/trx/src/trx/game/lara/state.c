#include <trx/game/lara/state.h>

#include <trx/debug.h>
#include <trx/game/lara.h>

static const LARA_TRX_ANIMATION m_TestResponsiveAnims[] = {
    // clang-format off
    LA_RUN,
    LA_UNDERWATER_SWIM_FORWARD,
    LA_SLIDE_FORWARD,
    LA_STAND_TO_JUMP,
    LA_REACH_TO_HANG,
    LA_TRX_INVALID,
    // clang-format on
};

static bool m_ResponsiveAnims[LA_NUMBER_OF] = {};
static void (*m_StateRoutines[LS_NUMBER_OF])(ITEM *item, COLL_INFO *coll) = {};
static void (*m_ExtraRoutines[LS_EXTRA_NUMBER_OF])(
    ITEM *item, COLL_INFO *coll) = {};

static bool M_HasResponsiveState(const LARA_TRX_ANIMATION anim_idx)
{
    const OBJECT *const obj = Object_Get(O_LARA);
    if (!obj->loaded) {
        return false;
    }

    const ANIM *const anim = Object_GetAnim(obj, LA(anim_idx));
    for (int32_t i = 0; i < anim->num_changes; i++) {
        const ANIM_CHANGE *const change = Anim_GetChange(anim->change_idx + i);
        if (change->goal_anim_state == LS(LS_RESPONSIVE)) {
            return true;
        }
    }

    return false;
}

void Lara_State_Register(
    const LARA_TRX_STATE state,
    void (*const handle_func)(ITEM *item, COLL_INFO *coll))
{
    m_StateRoutines[state] = handle_func;
}

void Lara_State_RegisterExtra(
    const LARA_EXTRA_STATE state,
    void (*const handle_func)(ITEM *item, COLL_INFO *coll))
{
    ASSERT(state >= 0 && state < LS_EXTRA_NUMBER_OF);
    m_ExtraRoutines[state] = handle_func;
}

void Lara_State_Initialise(void)
{
    for (int32_t i = 0; m_TestResponsiveAnims[i] != LA_TRX_INVALID; i++) {
        const LARA_TRX_ANIMATION anim = m_TestResponsiveAnims[i];
        m_ResponsiveAnims[anim] = M_HasResponsiveState(anim);
    }
}

bool Lara_State_IsResponsive(const LARA_TRX_ANIMATION anim_idx)
{
    return m_ResponsiveAnims[anim_idx];
}

void Lara_State_Update(ITEM *const item, COLL_INFO *const coll)
{
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    if (lara->water_status != LWS_SURFACE && lara->extra_anim) {
        if (m_ExtraRoutines[item->current_anim_state] != nullptr) {
            m_ExtraRoutines[item->current_anim_state](item, coll);
        }
        return;
    }

    const LARA_TRX_STATE state = LS_U(item->current_anim_state);
    if (state >= 0 && state < LS_NUMBER_OF) {
        if (m_StateRoutines[state] != nullptr) {
            m_StateRoutines[state](item, coll);
        }
        return;
    }
}
