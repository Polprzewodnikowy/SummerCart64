#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/game/creature.h>
#include <trx/game/items.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/spawn.h>

// clang-format off
#define M_WALK_TURN    (9 * DEG_1)
#define M_RUN_TURN     (6 * DEG_1)
#define M_OTHER_TURN   (4 * DEG_1)
#define M_CLOSE_RANGE  SQUARE(WALL_L * 2 / 3)
#define M_LONG_RANGE   SQUARE(WALL_L)
#define M_WALK_RANGE   SQUARE(WALL_L * 2)
#define M_ESCAPE_RANGE SQUARE(WALL_L * 3)
#define M_HIT_RANGE    (STEP_L * 2)
#define M_TOUCH_BITS   (1 << 13) // = 0x2000
// clang-format on

typedef struct {
    bool wants_wait_2;
} M_PRIV;

typedef enum {
    M_STATE_NULL,
    M_STATE_WAIT_1,
    M_STATE_WALK,
    M_STATE_RUN,
    M_STATE_ATTACK_1,
    M_STATE_ATTACK_2,
    M_STATE_ATTACK_3,
    M_STATE_ATTACK_4,
    M_STATE_AIM_3,
    M_STATE_DEATH,
    M_STATE_ATTACK_5,
    M_STATE_WAIT_2,
    M_STATE_ATTACK_6
} M_STATE;

typedef enum {
    M_ANIM_DEATH_STAND = 20,
    M_ANIM_DEATH_DOWN = 21,
} M_ANIM;

typedef struct {
    uint8_t start_frame;
    uint8_t end_frame;
    uint8_t damage;
} M_HIT_FRAME;

static BITE m_AxeHit = {
    .pos = { .x = 0, .y = 16, .z = 265 },
    .mesh_num = 13,
};

static M_HIT_FRAME m_HitFrames[13] = {
    {},
    {},
    {},
    {},
    {},
    { .start_frame = 2, .end_frame = 12, .damage = 8 },
    { .start_frame = 8, .end_frame = 9, .damage = 32 },
    { .start_frame = 19, .end_frame = 28, .damage = 8 },
    {},
    {},
    { .start_frame = 7, .end_frame = 14, .damage = 8 },
    {},
    { .start_frame = 15, .end_frame = 19, .damage = 32 }
};

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_SHOULD(JSON_READ(io, "wants_wait_2", &p->wants_wait_2));
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "wants_wait_2", p->wants_wait_2);
}

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    const ITEM *const lara_item = Lara_GetItem();
    const LARA_INFO *const lara = Lara_GetLaraInfo();

    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    CREATURE *const creature = item->creature_data;
    int16_t angle = 0;
    int16_t head = 0;
    int16_t tilt = 0;

    if (item->hit_points <= 0) {
        if (item->current_anim_state != M_STATE_DEATH) {
            if (item->current_anim_state == M_STATE_WAIT_1
                || item->current_anim_state == M_STATE_ATTACK_4) {
                Item_SwitchToAnim(item, M_ANIM_DEATH_DOWN, 0);
            } else {
                Item_SwitchToAnim(item, M_ANIM_DEATH_STAND, 0);
            }
            item->current_anim_state = M_STATE_DEATH;
        }
    } else {
        AI_INFO info;
        Creature_AIInfo(item, &info);
        Creature_UpdateMood(item, &info, true);

        if (creature->enemy == lara_item && creature->hurt_by_lara
            && info.distance > M_ESCAPE_RANGE && info.enemy_facing < 0x3000
            && info.enemy_facing > -0x3000) {
            creature->mood = MOOD_ESCAPE;
        }

        Creature_ApplyMood(item, &info, true);
        angle = Creature_Turn(item, creature->maximum_turn);

        if (info.ahead) {
            head = info.angle;
        }

        switch (item->current_anim_state) {
        case M_STATE_WAIT_1:
            creature->maximum_turn = M_OTHER_TURN;
            creature->flags = 0;

            if (creature->mood == MOOD_BORED) {
                creature->maximum_turn = 0;
                if (Random_GetControl() < 0x100) {
                    item->goal_anim_state = M_STATE_WALK;
                }
            } else if (creature->mood == MOOD_ESCAPE) {
                if (lara->target != item && info.ahead && !item->hit_status) {
                    item->goal_anim_state = M_STATE_WAIT_1;
                } else {
                    item->goal_anim_state = M_STATE_RUN;
                }
            } else if (p->wants_wait_2) {
                p->wants_wait_2 = false;
                item->goal_anim_state = M_STATE_WAIT_2;
            } else if (info.ahead && info.distance < M_CLOSE_RANGE) {
                item->goal_anim_state = M_STATE_ATTACK_4;
            } else if (info.ahead && info.distance < M_LONG_RANGE) {
                if (Random_GetControl() < 0x4000) {
                    item->goal_anim_state = M_STATE_WALK;
                } else {
                    item->goal_anim_state = M_STATE_ATTACK_4;
                }
            } else if (info.ahead && info.distance < M_WALK_RANGE) {
                item->goal_anim_state = M_STATE_WALK;
            } else {
                item->goal_anim_state = M_STATE_RUN;
            }
            break;

        case M_STATE_WALK:
            creature->maximum_turn = M_WALK_TURN;
            creature->flags = 0;
            tilt = angle >> 3;

            if (creature->mood == MOOD_BORED) {
                creature->maximum_turn = 409;
                if (Random_GetControl() < 0x100) {
                    if (Random_GetControl() < 0x2000) {
                        item->goal_anim_state = M_STATE_WAIT_1;
                    } else {
                        item->goal_anim_state = M_STATE_WAIT_2;
                    }
                }
            } else if (creature->mood == MOOD_ESCAPE) {
                item->goal_anim_state = M_STATE_RUN;
            } else if (info.ahead && info.distance < M_CLOSE_RANGE) {
                if (Random_GetControl() < 0x2000) {
                    item->goal_anim_state = M_STATE_WAIT_1;
                } else {
                    item->goal_anim_state = M_STATE_WAIT_2;
                }
            } else if (info.distance > M_WALK_RANGE) {
                item->goal_anim_state = M_STATE_RUN;
            }
            break;

        case M_STATE_RUN:
            creature->maximum_turn = M_RUN_TURN;
            creature->flags = 0;
            tilt = angle >> 2;

            if (creature->mood == MOOD_BORED) {
                creature->maximum_turn = 1.5f * DEG_1;

                if (Random_GetControl() < 0x100) {
                    if (Random_GetControl() < 0x4000) {
                        item->goal_anim_state = M_STATE_WAIT_1;
                    } else {
                        item->goal_anim_state = M_STATE_WAIT_2;
                    }
                }
            } else if (
                creature->mood == MOOD_ESCAPE && lara->target != item
                && info.ahead) {
                item->goal_anim_state = M_STATE_WAIT_2;
            } else if (info.bite || info.distance < M_WALK_RANGE) {
                if (Random_GetControl() < 0x4000) {
                    item->goal_anim_state = M_STATE_ATTACK_6;
                } else if (Random_GetControl() < 0x2000) {
                    item->goal_anim_state = M_STATE_ATTACK_5;
                } else {
                    item->goal_anim_state = M_STATE_WALK;
                }
            }
            break;

        case M_STATE_ATTACK_2:
        case M_STATE_ATTACK_3:
        case M_STATE_ATTACK_4:
        case M_STATE_ATTACK_5:
        case M_STATE_ATTACK_6:
            p->wants_wait_2 = true;
            creature->maximum_turn = M_OTHER_TURN;
            creature->flags = Item_GetRelativeFrame(item);
            const M_HIT_FRAME *const hit_frame =
                &m_HitFrames[item->current_anim_state];
            ITEM *const enemy = creature->enemy;

            if (enemy == lara_item) {
                if (item->touch_bits & M_TOUCH_BITS
                    && creature->flags >= hit_frame->start_frame
                    && creature->flags <= hit_frame->end_frame) {
                    Lara_TakeDamage(hit_frame->damage, true);

                    for (int32_t i = 0; i < hit_frame->damage; i += 8) {
                        Creature_Effect(item, &m_AxeHit, Spawn_Blood);
                    }

                    Sound_Effect(SFX_LARA_THUD, &item->pos, SPM_NORMAL);
                }
            } else if (enemy != nullptr) {
                if (Item_IsNearby(enemy, item, M_HIT_RANGE)) {
                    if (creature->flags >= hit_frame->start_frame
                        && creature->flags <= hit_frame->end_frame) {
                        enemy->hit_points -= 2;
                        enemy->hit_status = 1;
                        Creature_Effect(item, &m_AxeHit, Spawn_Blood);
                        Sound_Effect(SFX_LARA_THUD, &item->pos, SPM_NORMAL);
                    }
                }
            }
            break;

        case M_STATE_AIM_3:
            creature->maximum_turn = M_OTHER_TURN;
            if (info.bite || info.distance < M_CLOSE_RANGE) {
                item->goal_anim_state = M_STATE_ATTACK_3;
            } else {
                item->goal_anim_state = M_STATE_WAIT_2;
            }
            break;

        case M_STATE_WAIT_2:
            creature->maximum_turn = M_OTHER_TURN;
            creature->flags = 0;

            if (creature->mood == MOOD_BORED) {
                creature->maximum_turn = 0;
                if (Random_GetControl() < 0x100) {
                    item->goal_anim_state = M_STATE_WALK;
                }
            } else if (creature->mood == MOOD_ESCAPE) {
                if (lara->target != item && info.ahead && !item->hit_status) {
                    item->goal_anim_state = M_STATE_WAIT_1;
                } else {
                    item->goal_anim_state = M_STATE_RUN;
                }
            } else if (info.ahead && info.distance < M_CLOSE_RANGE) {
                if (Random_GetControl() < 0x800) {
                    item->goal_anim_state = M_STATE_ATTACK_2;
                } else {
                    item->goal_anim_state = M_STATE_AIM_3;
                }
            } else if (info.distance < M_WALK_RANGE) {
                item->goal_anim_state = M_STATE_WALK;
            } else {
                item->goal_anim_state = M_STATE_RUN;
            }
            break;
        }
    }

    Creature_Tilt(item, tilt);
    Creature_Joint(item, 0, head >> 1);
    Creature_Joint(item, 1, head >> 1);
    Creature_Animate(item_num, angle, 0);
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;
    obj->priv_size = sizeof(M_PRIV);
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;

    obj->shadow_size = UNIT_SHADOW / 2;
    obj->hit_points = 28;
    obj->radius = 102;
    obj->pivot_length = 0;

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    Object_GetBone(obj, 13)->rot.y = true;
    Object_GetBone(obj, 6)->rot.y = true;
}

REGISTER_OBJECT(O_TRIBE_AXEMAN, M_Setup)
