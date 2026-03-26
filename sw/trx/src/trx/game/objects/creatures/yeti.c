#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/spawn.h>

// clang-format off
#define M_HITPOINTS        30
#define M_TOUCH_BITS_R     0b00000111'00000000 // = 0x0700
#define M_TOUCH_BITS_L     0b00111000'00000000 // = 0x3800
#define M_TOUCH_BITS_LR    (M_TOUCH_BITS_R | M_TOUCH_BITS_L) // = 0x3F00
#define M_RADIUS           (WALL_L / 8) // = 128
#define M_WALK_TURN        (DEG_1 * 4) // = 728
#define M_RUN_TURN         (DEG_1 * 6) // = 1092
#define M_VAULT_SHIFT      300
#define M_ATTACK_1_RANGE   SQUARE(WALL_L / 2) // = 262144
#define M_ATTACK_2_RANGE   SQUARE(WALL_L * 2 / 3) // = 465124
#define M_ATTACK_3_RANGE   SQUARE(WALL_L * 2) // = 4194304
#define M_RUN_RANGE        SQUARE(WALL_L * 2) // = 4194304
#define M_PUNCH_DAMAGE     100
#define M_THUMP_DAMAGE     150
#define M_CHARGE_DAMAGE    200
#define M_ATTACK_1_CHANCE  0x4000
#define M_WAIT_1_CHANCE    0x100
#define M_WAIT_2_CHANCE    (M_WAIT_1_CHANCE + 0x100) // = 512
#define M_WALK_CHANCE      (M_WAIT_2_CHANCE + 0x100) // = 768
#define M_STOP_ROAR_CHANCE 0x200
// clang-format on

typedef enum {
    M_STATE_EMPTY,
    M_STATE_RUN,
    M_STATE_STOP,
    M_STATE_WALK,
    M_STATE_ATTACK_1,
    M_STATE_ATTACK_2,
    M_STATE_ATTACK_3,
    M_STATE_WAIT_1,
    M_STATE_DEATH,
    M_STATE_WAIT_2,
    M_STATE_CLIMB_1,
    M_STATE_CLIMB_2,
    M_STATE_CLIMB_3,
    M_STATE_FALL,
    M_STATE_KILL,
} M_STATE;

typedef enum {
    // clang-format off
    M_ANIM_KILL    = 36,
    M_ANIM_FALL    = 35,
    M_ANIM_CLIMB_1 = 34,
    M_ANIM_CLIMB_2 = 33,
    M_ANIM_CLIMB_3 = 32,
    M_ANIM_DEATH   = 31,
    // clang-format on
} M_ANIM;

static const BITE m_YetiBiteL = {
    .pos = { .x = 12, .y = 101, .z = 19 },
    .mesh_num = 13,
};

static const BITE m_YetiBiteR = {
    .pos = { .x = 12, .y = 101, .z = 19 },
    .mesh_num = 10,
};

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    CREATURE *const creature = item->creature_data;

    int16_t head = 0;
    int16_t body = 0;
    int16_t angle = 0;

    const ITEM *const lara_item = Lara_GetItem();
    const bool lara_was_alive = lara_item->hit_points > 0;

    if (item->hit_points > 0) {
        AI_INFO info;
        Creature_AIInfo(item, &info);
        Creature_Mood(item, &info, true);

        if (info.ahead) {
            head = info.angle;
        }
        angle = Creature_Turn(item, creature->maximum_turn);

        switch (item->current_anim_state) {
        case M_STATE_STOP:
            creature->flags = 0;
            creature->maximum_turn = 0;
            if (creature->mood == MOOD_ESCAPE) {
                item->goal_anim_state = M_STATE_RUN;
            } else if (item->required_anim_state != M_STATE_EMPTY) {
                item->goal_anim_state = item->required_anim_state;
            } else if (creature->mood == MOOD_BORED) {
                const int32_t random = Random_GetControl();
                if (random < M_WAIT_1_CHANCE || !lara_was_alive) {
                    item->goal_anim_state = M_STATE_WAIT_1;
                } else if (random < M_WAIT_2_CHANCE) {
                    item->goal_anim_state = M_STATE_WAIT_2;
                } else if (random < M_WALK_CHANCE) {
                    item->goal_anim_state = M_STATE_WALK;
                }
            } else if (
                info.ahead && info.distance < M_ATTACK_1_RANGE
                && Random_GetControl() < M_ATTACK_1_CHANCE) {
                item->goal_anim_state = M_STATE_ATTACK_1;
                break;
            } else if (info.ahead && info.distance < M_ATTACK_2_RANGE) {
                item->goal_anim_state = M_STATE_ATTACK_2;
            } else if (creature->mood == MOOD_STALK) {
                item->goal_anim_state = M_STATE_WALK;
            } else {
                item->goal_anim_state = M_STATE_RUN;
            }
            break;

        case M_STATE_WAIT_1:
            if (creature->mood == MOOD_ESCAPE || item->hit_status) {
                item->goal_anim_state = M_STATE_STOP;
            } else if (creature->mood == MOOD_BORED) {
                if (lara_was_alive) {
                    const int32_t random = Random_GetControl();
                    if (random < M_WAIT_1_CHANCE) {
                        item->goal_anim_state = M_STATE_STOP;
                    } else if (random < M_WAIT_2_CHANCE) {
                        item->goal_anim_state = M_STATE_WAIT_2;
                    } else if (random < M_WALK_CHANCE) {
                        item->goal_anim_state = M_STATE_STOP;
                        item->required_anim_state = M_STATE_WALK;
                    }
                }
            } else if (Random_GetControl() < M_STOP_ROAR_CHANCE) {
                item->goal_anim_state = M_STATE_STOP;
            }
            break;

        case M_STATE_WAIT_2:
            if (creature->mood == MOOD_ESCAPE || item->hit_status) {
                item->goal_anim_state = M_STATE_STOP;
            } else if (creature->mood == MOOD_BORED) {
                const int32_t random = Random_GetControl();
                if (random < M_WAIT_1_CHANCE || !lara_was_alive) {
                    item->goal_anim_state = M_STATE_WAIT_1;
                } else if (random < M_WAIT_2_CHANCE) {
                    item->goal_anim_state = M_STATE_STOP;
                } else if (random < M_WALK_CHANCE) {
                    item->goal_anim_state = M_STATE_STOP;
                    item->required_anim_state = M_STATE_WALK;
                }
            } else if (Random_GetControl() < M_STOP_ROAR_CHANCE) {
                item->goal_anim_state = M_STATE_STOP;
            }
            break;

        case M_STATE_WALK:
            creature->maximum_turn = M_WALK_TURN;
            if (creature->mood == MOOD_ESCAPE) {
                item->goal_anim_state = M_STATE_RUN;
            } else if (creature->mood == MOOD_BORED) {
                const int32_t random = Random_GetControl();
                if (random < M_WAIT_1_CHANCE || !lara_was_alive) {
                    item->goal_anim_state = M_STATE_STOP;
                    item->required_anim_state = M_STATE_WAIT_1;
                } else if (random < M_WAIT_2_CHANCE) {
                    item->goal_anim_state = M_STATE_STOP;
                    item->required_anim_state = M_STATE_WAIT_2;
                } else if (random < M_WALK_CHANCE) {
                    item->goal_anim_state = M_STATE_STOP;
                }
            } else if (creature->mood == MOOD_ATTACK) {
                if (info.ahead && info.distance < M_ATTACK_2_RANGE) {
                    item->goal_anim_state = M_STATE_STOP;
                } else if (info.distance > M_RUN_RANGE) {
                    item->goal_anim_state = M_STATE_RUN;
                }
            }
            break;

        case M_STATE_RUN:
            creature->flags = 0;
            creature->maximum_turn = M_RUN_TURN;
            if (creature->mood == MOOD_ESCAPE) {
                break;
            } else if (creature->mood == MOOD_BORED) {
                item->goal_anim_state = M_STATE_WALK;
            } else if (info.ahead && info.distance < M_ATTACK_2_RANGE) {
                item->goal_anim_state = M_STATE_STOP;
            } else if (info.ahead && info.distance < M_ATTACK_3_RANGE) {
                item->goal_anim_state = M_STATE_ATTACK_3;
            } else if (creature->mood == MOOD_STALK) {
                item->goal_anim_state = M_STATE_WALK;
            }
            break;

        case M_STATE_ATTACK_1:
            body = head;
            head = 0;
            if (creature->flags == 0
                && (item->touch_bits & M_TOUCH_BITS_R) != 0) {
                Creature_Effect(item, &m_YetiBiteR, Spawn_Blood);
                Lara_TakeDamage(M_PUNCH_DAMAGE, true);
                creature->flags = 1;
                break;
            }
            break;

        case M_STATE_ATTACK_2:
            body = head;
            head = 0;

            creature->maximum_turn = M_WALK_TURN;
            if (creature->flags == 0
                && (item->touch_bits & M_TOUCH_BITS_LR) != 0) {
                if ((item->touch_bits & M_TOUCH_BITS_L) != 0) {
                    Creature_Effect(item, &m_YetiBiteL, Spawn_Blood);
                }
                if ((item->touch_bits & M_TOUCH_BITS_R) != 0) {
                    Creature_Effect(item, &m_YetiBiteR, Spawn_Blood);
                }
                Lara_TakeDamage(M_THUMP_DAMAGE, true);
                creature->flags = 1;
            }
            break;

        case M_STATE_ATTACK_3:
            body = head;
            head = 0;

            if (creature->flags == 0
                && (item->touch_bits & M_TOUCH_BITS_LR) != 0) {
                if ((item->touch_bits & M_TOUCH_BITS_L) != 0) {
                    Creature_Effect(item, &m_YetiBiteL, Spawn_Blood);
                }
                if ((item->touch_bits & M_TOUCH_BITS_R) != 0) {
                    Creature_Effect(item, &m_YetiBiteR, Spawn_Blood);
                }
                Lara_TakeDamage(M_CHARGE_DAMAGE, true);
                creature->flags = 1;
            }
            break;

        default:
            break;
        }
    } else if (item->current_anim_state != M_STATE_DEATH) {
        Item_SwitchToAnim(item, M_ANIM_DEATH, 0);
        item->current_anim_state = M_STATE_DEATH;
    }

    if (lara_was_alive && lara_item->hit_points <= 0) {
        Creature_SpecialKill(
            item, M_ANIM_KILL, M_STATE_KILL, LS_EXTRA_YETI_KILL);
        return;
    }

    Creature_Head(item, body);
    Creature_Neck(item, head);
    if (item->current_anim_state >= M_STATE_CLIMB_1) {
        Creature_Animate(item_num, angle, 0);
    } else {
        switch (Creature_Vault(item_num, angle, 2, M_VAULT_SHIFT)) {
        case -4:
            Item_SwitchToAnim(item, M_ANIM_FALL, 0);
            item->current_anim_state = M_STATE_FALL;
            break;

        case 2:
            Item_SwitchToAnim(item, M_ANIM_CLIMB_1, 0);
            item->current_anim_state = M_STATE_CLIMB_1;
            break;

        case 3:
            Item_SwitchToAnim(item, M_ANIM_CLIMB_2, 0);
            item->current_anim_state = M_STATE_CLIMB_2;
            break;

        case 4:
            Item_SwitchToAnim(item, M_ANIM_CLIMB_3, 0);
            item->current_anim_state = M_STATE_CLIMB_3;
            break;

        default:
            return;
        }
    }
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;

    obj->hit_points = M_HITPOINTS;
    obj->radius = M_RADIUS;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->pivot_length = 100;
    obj->lot_setup = LOT_Setup(LOT_SETUP_CLIMBER);

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    Object_GetBone(obj, 6)->rot.y = true;
    Object_GetBone(obj, 14)->rot.y = true;
}

REGISTER_OBJECT(O_YETI, M_Setup)
