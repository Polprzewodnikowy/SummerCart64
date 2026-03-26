#pragma once

#include <trx/game/anims/types.h>
#include <trx/game/items/types.h>

ANIM *Item_GetAnim(const ITEM *item);
bool Item_TestAnimEqual(const ITEM *item, int16_t anim_idx);
int16_t Item_GetRelativeAnim(const ITEM *item);
int16_t Item_GetRelativeObjAnim(const ITEM *item, OBJECT_ID obj_id);
int16_t Item_GetRelativeFrame(const ITEM *item);
void Item_SwitchToAnim(ITEM *item, int16_t anim_idx, int16_t frame);
void Item_SwitchToObjAnim(
    ITEM *item, int16_t anim_idx, int16_t frame, OBJECT_ID obj_id);

// Tests if the given item's current relative animation frame matches the
// provided value. If a negative value is passed, the test is performed from the
// end of the animation's frame set.
bool Item_TestFrameEqual(const ITEM *item, int16_t frame);
bool Item_TestFrameRange(const ITEM *item, int16_t start, int16_t end);

ANIM_FRAME *Item_GetBestFrame(const ITEM *item);
int32_t Item_GetFrames(const ITEM *item, ANIM_FRAME *frmptr[], int32_t *rate);

void Item_Animate(ITEM *item);
bool Item_GetAnimChange(ITEM *item, const ANIM *anim);
void Item_PlayAnimSFX(const ITEM *item, const ANIM_COMMAND_EFFECT_DATA *data);
