#pragma once

#include <trx/game/collision.h>
#include <trx/game/creature/types.h>

void Creature_Initialise(int16_t item_num);
bool Creature_Activate(int16_t item_num);
void Creature_AIInfo(ITEM *item, AI_INFO *info);
bool Creature_EnsureHabitat(
    int16_t item_num, int32_t *wh, const HYBRID_INFO *info);
void Creature_Mood(const ITEM *item, const AI_INFO *info, bool violent);
void Creature_UpdateMood(const ITEM *item, const AI_INFO *info, bool violent);
void Creature_ApplyMood(const ITEM *item, const AI_INFO *info, bool violent);

int16_t Creature_Turn(ITEM *item, int16_t max_turn);
void Creature_Tilt(ITEM *item, int16_t angle);
void Creature_Head(ITEM *item, int16_t required);
void Creature_Neck(ITEM *item, int16_t required);
void Creature_Joint(ITEM *item, int16_t joint, int16_t required);

void Creature_Float(int16_t item_num);
void Creature_Underwater(ITEM *item, int32_t depth);

bool Creature_CanSeeEnemy(const ITEM *item, const AI_INFO *info);
bool Creature_CanTargetEnemy(const ITEM *item, const AI_INFO *info);
void Creature_Collision(int16_t item_num, ITEM *lara_item, COLL_INFO *coll);
bool Creature_Animate(int16_t item_num, int16_t angle, int16_t tilt);

void Creature_SpecialKill(
    ITEM *item, int32_t kill_anim, int32_t kill_state, int32_t lara_kill_state);
void Creature_Die(int16_t item_num, bool explode);
int32_t Creature_Vault(
    int16_t item_num, int16_t angle, int32_t vault, int32_t shift);

void Creature_Reset(void);
bool Creature_AreAlliesHostile(void);
void Creature_SetAlliesHostile(bool enable);
void Creature_Hurt(ITEM *item, int32_t damage);
bool Creature_IsHostile(const ITEM *item);
bool Creature_IsAlly(const ITEM *item);
bool Creature_IsAllyTargetingEnemy(const ITEM *item);
void Creature_AddAlly(OBJECT_ID obj_id);
void Creature_AddAllyTargetingEnemy(OBJECT_ID obj_id);

int16_t Creature_Effect(
    const ITEM *item, const BITE *bite,
    int16_t (*spawn)(
        int32_t x, int32_t y, int32_t z, int16_t speed, int16_t y_rot,
        int16_t room_num));

bool Creature_Shoot(
    ITEM *item, const AI_INFO *info, const CREATURE_GUN *gun,
    int16_t extra_rotation, int32_t damage);

int16_t Creature_AIGuard(CREATURE *creature);
void Creature_GetAITarget(CREATURE *creature);
