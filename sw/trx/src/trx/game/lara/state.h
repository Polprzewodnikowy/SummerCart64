#include <trx/game/collision.h>
#include <trx/game/lara/enum.h>

void Lara_State_Register(
    LARA_TRX_STATE state, void (*handle_func)(ITEM *item, COLL_INFO *coll));
void Lara_State_RegisterExtra(
    LARA_EXTRA_STATE state, void (*handle_func)(ITEM *item, COLL_INFO *coll));
void Lara_State_Initialise(void);
bool Lara_State_IsResponsive(LARA_TRX_ANIMATION anim_idx);
void Lara_State_Update(ITEM *item, COLL_INFO *coll);
