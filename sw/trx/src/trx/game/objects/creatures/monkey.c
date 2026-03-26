#include <trx/config.h>
#include <trx/core/math/geom.h>
#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/game_buf.h>
#include <trx/game/items/anim.h>
#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/objects/draw.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/shell.h>
#include <trx/game/spawn.h>

// clang-format off
#define M_DAMAGE_NORMAL 40
#define M_DAMAGE_JUMP   50
#define M_WALK_TURN     (7 * DEG_1)
#define M_RUN_TURN      (11 * DEG_1)
#define M_JUMP_RANGE    SQUARE(WALL_L * 2/3) // = 465124
#define M_WALK_RANGE    SQUARE(WALL_L * 2/3) // = 465124
#define M_ATTACK_RANGE  SQUARE(WALL_L / 3) // = 116281
#define M_ROLL_RANGE    SQUARE(WALL_L) // = 1048576
#define M_WAIT_CHANCE   256
#define M_F_PICKUP      12
// clang-format on

static BITE m_MonkeyBite = {
    .pos = { 10, 10, 11 },
    .mesh_num = 13,
};

typedef enum {
    M_STATE_EMPTY,
    M_STATE_STOP,
    M_STATE_WALK,
    M_STATE_STAND,
    M_STATE_RUN,
    M_STATE_PICKUP,
    M_STATE_SIT,
    M_STATE_EAT,
    M_STATE_SCRATCH,
    M_STATE_ROLL,
    M_STATE_ANGRY,
    M_STATE_DEATH,
    M_STATE_ATTACK_LOW,
    M_STATE_ATTACK_HIGH,
    M_STATE_ATTACK_JUMP,
    M_STATE_CLIMB_4,
    M_STATE_CLIMB_3,
    M_STATE_CLIMB_2,
    M_STATE_DOWN_4,
    M_STATE_DOWN_3,
    M_STATE_DOWN_2
} M_STATE;

typedef enum {
    M_ANIM_SIT = 2,
    M_ANIM_DEATH = 14,
    M_ANIM_CLIMB_2 = 19,
    M_ANIM_CLIMB_3 = 18,
    M_ANIM_CLIMB_4 = 17,
    M_ANIM_DOWN_2 = 22,
    M_ANIM_DOWN_3 = 21,
    M_ANIM_DOWN_4 = 20,
} M_ANIM;

static void M_Bite(ITEM *const item, ITEM *const enemy, const int32_t dmg)
{
    CREATURE *const creature = item->creature_data;
    if (enemy == Lara_GetItem()) {
        if (creature->flags == 0 && item->touch_bits & 0x2400) {
            Lara_TakeDamage(dmg, true);
            creature->flags = 1;
            Creature_Effect(item, &m_MonkeyBite, Spawn_Blood);
        }
    } else if (creature->flags == 0 && enemy != nullptr) {
        if (ABS(enemy->pos.x - item->pos.x) < STEP_L
            && ABS(enemy->pos.y - item->pos.y) <= STEP_L
            && ABS(enemy->pos.z - item->pos.z) < STEP_L) {
            Item_TakeDamage(enemy, dmg / 2, true);
            creature->flags = 1;
            Creature_Effect(item, &m_MonkeyBite, Spawn_Blood);
        }
    }
}

static bool M_CarryPickup(
    ITEM *const item, CREATURE *const creature, const int16_t item_num)
{
    if (creature->enemy == nullptr) {
        return false;
    }

    if (creature->enemy->object_id != O_SMALL_MEDIPACK_ITEM
        && creature->enemy->object_id != O_KEY_ITEM_4) {
        return false;
    }

    if (!Item_TestFrameEqual(item, M_F_PICKUP)) {
        return false;
    }

    if (creature->enemy->room_num == NO_ROOM
        || creature->enemy->status == IS_INVISIBLE
        || creature->enemy->clear_body) {
        creature->enemy = nullptr;
        return true;
    }

    const int16_t pickup_num = Item_GetIndex(creature->enemy);
    if (item->carried_item == nullptr) {
        item->carried_item = GameBuf_Alloc(sizeof(CARRIED_ITEM), GBUF_ITEMS);
        item->carried_item->next_item = nullptr;
    }
    item->carried_item->object_id = creature->enemy->object_id;
    item->carried_item->spawn_num = pickup_num;
    item->carried_item->pos = creature->enemy->pos;
    item->carried_item->rot = creature->enemy->rot;
    item->carried_item->room_num = NO_ROOM;
    item->carried_item->fall_speed = 0;
    item->carried_item->status = DS_CARRIED;
    Item_UpdateRoom(pickup_num, NO_ROOM);
    creature->enemy->carried_item = nullptr;

    for (int32_t i = 0; i < LOT_SLOT_COUNT; i++) {
        CREATURE *const slot = LOT_GetBaddieSlot(i);
        if (slot->item_num != NO_ITEM && slot->item_num != item_num
            && slot->enemy == creature->enemy) {
            slot->enemy = nullptr;
        }
    }
    creature->enemy = nullptr;

    if (item->ai_bits != AI_MODIFY) {
        item->ai_bits |= AI_AMBUSH | AI_MODIFY;
    }

    return true;
}

static bool M_DropPickup(ITEM *const item, CREATURE *const creature)
{
    if (creature->enemy == nullptr) {
        return false;
    }

    if (creature->enemy->object_id != O_AI_AMBUSH) {
        return false;
    }

    if (!Item_TestFrameEqual(item, M_F_PICKUP)) {
        return false;
    }

    item->ai_bits = 0;
    ITEM *const pickup = Item_Get(item->carried_item->spawn_num);
    pickup->pos = item->pos;
    Item_UpdateRoom(item->carried_item->spawn_num, item->room_num);
    pickup->ai_bits = AI_GUARD;
    item->carried_item = nullptr;
    creature->enemy = nullptr;
    return true;
}

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    Creature_Initialise(item_num);
    Item_SwitchToAnim(item, M_ANIM_SIT, 0);
    item->current_anim_state = M_STATE_SIT;
    item->goal_anim_state = M_STATE_SIT;
}

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    CREATURE *const creature = item->creature_data;

    ITEM *const lara_item = Lara_GetItem();
    LARA_INFO *const lara = Lara_GetLaraInfo();

    int16_t angle = 0;
    int16_t tilt = 0;
    int16_t torso_y = 0;

    if (item->hit_points <= 0) {
        if (item->current_anim_state != M_STATE_DEATH) {
            Item_SwitchToAnim(item, M_ANIM_DEATH, 0);
            item->current_anim_state = M_STATE_DEATH;
            item->mesh_bits = -1;
        }
    } else {
        Creature_GetAITarget(creature);
        if (creature->hurt_by_lara
            && g_Config.gameplay.fix_monkey_pickup_priority) {
            creature->enemy = lara_item;
        }

        if (item->ai_bits == AI_MODIFY) {
            if (item->carried_item == nullptr) {
                item->mesh_bits = 0xFFFF6F6F;
            } else {
                item->mesh_bits = 0xFFFF6E6F;
            }
        } else if (item->carried_item == nullptr) {
            item->mesh_bits = -1;
        } else {
            item->mesh_bits = 0xFFFFFEFF;
        }

        AI_INFO info;
        Creature_AIInfo(item, &info);

        int32_t dist;
        if (creature->enemy == lara_item) {
            dist = info.distance;
        } else {
            const int32_t dx = lara_item->pos.x - item->pos.x;
            const int32_t dz = lara_item->pos.z - item->pos.z;
            Math_Atan(dz, dx);
            dist = XYZ_32_GetLength2((XYZ_32) { dx, 0, dz });
        }

        Creature_UpdateMood(item, &info, true);
        if (Lara_Vehicle_GetIndex() != NO_ITEM) {
            creature->mood = MOOD_ESCAPE;
        }

        Creature_ApplyMood(item, &info, true);
        angle = Creature_Turn(item, creature->maximum_turn);

        if (item->hit_status) {
            ITEM *const enemy = creature->enemy;
            creature->enemy = lara_item;
            Creature_AlertAllGuards(item_num);
            creature->enemy = enemy;
        }

        switch (item->current_anim_state) {
        case M_STATE_WALK:
            creature->maximum_turn = M_WALK_TURN;

            if (item->ai_bits & AI_PATROL_1) {
                item->goal_anim_state = M_STATE_WALK;
            } else if (creature->mood == MOOD_ESCAPE) {
                item->goal_anim_state = M_STATE_RUN;
            } else if (creature->mood == MOOD_BORED) {
                if (Random_GetControl() < M_WAIT_CHANCE) {
                    item->goal_anim_state = M_STATE_SIT;
                }
            } else if (info.bite && info.distance < M_JUMP_RANGE) {
                item->goal_anim_state = M_STATE_STAND;
            } else {
                item->goal_anim_state = M_STATE_STAND;
            }

            break;

        case M_STATE_STAND:
            creature->flags = 0;
            creature->maximum_turn = 0;

            if (item->ai_bits & AI_GUARD) {
                Creature_AIGuard(creature);
                if (!(Random_GetControl() & 0xF)) {
                    if (Random_GetControl() & 1) {
                        item->goal_anim_state = M_STATE_ANGRY;
                    } else {
                        item->goal_anim_state = M_STATE_SIT;
                    }
                }
            } else if (item->ai_bits & AI_PATROL_1) {
                item->goal_anim_state = M_STATE_WALK;
            } else if (creature->mood == MOOD_ESCAPE) {
                if (lara->target != item && info.ahead) {
                    item->goal_anim_state = M_STATE_STAND;
                } else {
                    item->goal_anim_state = M_STATE_RUN;
                }
            } else if (creature->mood == MOOD_BORED) {
                if (item->required_anim_state != M_STATE_EMPTY) {
                    item->goal_anim_state = item->required_anim_state;
                } else if (!(Random_GetControl() & 0xF)) {
                    item->goal_anim_state = M_STATE_WALK;
                } else if (!(Random_GetControl() & 0xF)) {
                    if (Random_GetControl() & 1) {
                        item->goal_anim_state = M_STATE_ANGRY;
                    } else {
                        item->goal_anim_state = M_STATE_SIT;
                    }
                }
            } else if (
                item->ai_bits & AI_FOLLOW
                && (creature->reached_goal || dist > SQUARE(WALL_L * 2))) {
                if (item->required_anim_state != M_STATE_EMPTY) {
                    item->goal_anim_state = item->required_anim_state;
                } else if (info.ahead) {
                    item->goal_anim_state = M_STATE_SIT;
                } else {
                    item->goal_anim_state = M_STATE_RUN;
                }
            } else if (info.bite && info.distance < M_ATTACK_RANGE) {
                if (lara_item->pos.y < item->pos.y) {
                    item->goal_anim_state = M_STATE_ATTACK_HIGH;
                } else {
                    item->goal_anim_state = M_STATE_ATTACK_LOW;
                }
            } else if (info.bite && info.distance < M_JUMP_RANGE) {
                item->goal_anim_state = M_STATE_ATTACK_JUMP;
            } else if (info.bite && info.distance < M_WALK_RANGE) {
                item->goal_anim_state = M_STATE_WALK;
            } else if (
                info.distance < M_WALK_RANGE && creature->enemy != lara_item
                && creature->enemy != nullptr
                && creature->enemy->object_id != O_AI_PATROL_1
                && creature->enemy->object_id != O_AI_PATROL_2
                && ABS(item->pos.y - creature->enemy->pos.y) < STEP_L) {
                item->goal_anim_state = M_STATE_PICKUP;
            } else if (info.bite && info.distance < M_ROLL_RANGE) {
                item->goal_anim_state = M_STATE_ROLL;
            } else {
                item->goal_anim_state = M_STATE_RUN;
            }
            break;

        case M_STATE_RUN:
            creature->maximum_turn = M_RUN_TURN;
            tilt = angle / 2;

            if (item->ai_bits & AI_GUARD) {
                item->goal_anim_state = M_STATE_STAND;
            } else if (creature->mood == MOOD_ESCAPE) {
                if (lara->target != item && info.ahead) {
                    item->goal_anim_state = M_STATE_STAND;
                }
            } else if (
                item->ai_bits & AI_FOLLOW
                && (creature->reached_goal || dist > SQUARE(WALL_L * 2))) {
                item->goal_anim_state = M_STATE_STAND;
            } else if (creature->mood == MOOD_BORED) {
                item->goal_anim_state = M_STATE_ROLL;
            } else if (info.distance < M_WALK_RANGE) {
                item->goal_anim_state = M_STATE_STAND;
            } else if (info.bite && info.distance < M_ROLL_RANGE) {
                item->goal_anim_state = M_STATE_ROLL;
            }

            break;

        case M_STATE_PICKUP:
            creature->reached_goal = true;
            if (creature->enemy == nullptr) {
                break;
            }

            if (M_CarryPickup(item, creature, item_num)) {
                break;
            } else if (M_DropPickup(item, creature)) {
                break;
            } else {
                creature->maximum_turn = 0;

                if (ABS(info.angle) < M_WALK_TURN) {
                    item->rot.y += info.angle;
                } else if (info.angle < 0) {
                    item->rot.y -= M_WALK_TURN;
                } else {
                    item->rot.y += M_WALK_TURN;
                }
            }
            break;

        case M_STATE_SIT:
            creature->flags = 0;
            creature->maximum_turn = 0;

            if (item->ai_bits & AI_GUARD) {
                Creature_AIGuard(creature);

                if (!(Random_GetControl() & 0xF)) {
                    if (Random_GetControl() & 1) {
                        item->goal_anim_state = M_STATE_SCRATCH;
                    } else {
                        item->goal_anim_state = M_STATE_EAT;
                    }
                }
            } else if (item->ai_bits & AI_PATROL_1) {
                item->goal_anim_state = M_STATE_WALK;
            } else if (creature->mood == MOOD_ESCAPE) {
                item->goal_anim_state = M_STATE_STAND;
            } else if (creature->mood == MOOD_BORED) {
                if (item->required_anim_state != M_STATE_EMPTY) {
                    item->goal_anim_state = item->required_anim_state;
                } else if (!(Random_GetControl() & 0xF)) {
                    item->goal_anim_state = M_STATE_WALK;
                } else if (!(Random_GetControl() & 0xF)) {
                    if (Random_GetControl() & 1) {
                        item->goal_anim_state = M_STATE_SCRATCH;
                    } else {
                        item->goal_anim_state = M_STATE_EAT;
                    }
                }
            } else if (
                item->ai_bits & AI_FOLLOW
                && (creature->reached_goal || dist > SQUARE(WALL_L * 2))) {
                if (item->required_anim_state != M_STATE_EMPTY) {
                    item->goal_anim_state = item->required_anim_state;
                } else if (info.ahead) {
                    item->goal_anim_state = M_STATE_SIT;
                } else {
                    item->goal_anim_state = M_STATE_STAND;
                }
            } else if (info.bite && info.distance < M_JUMP_RANGE) {
                item->goal_anim_state = M_STATE_STAND;
            } else if (info.bite && info.distance < M_WALK_RANGE) {
                item->goal_anim_state = M_STATE_WALK;
            } else {
                item->goal_anim_state = M_STATE_STAND;
            }
            break;

        case M_STATE_ATTACK_LOW:
            if (info.ahead) {
                torso_y = info.angle;
            }
            creature->maximum_turn = 0;
            if (ABS(info.angle) < M_WALK_TURN) {
                item->rot.y += info.angle;
            } else if (info.angle < 0) {
                item->rot.y -= M_WALK_TURN;
            } else {
                item->rot.y += M_WALK_TURN;
            }
            M_Bite(item, creature->enemy, M_DAMAGE_NORMAL);
            break;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
        case M_STATE_ATTACK_HIGH:
            if (info.ahead) {
                torso_y = info.angle;
            }
            creature->maximum_turn = 0;
            if (ABS(info.angle) < M_WALK_TURN) {
                item->rot.y += info.angle;
            } else if (info.angle < 0) {
                item->rot.y -= M_WALK_TURN;
            } else {
                item->rot.y += M_WALK_TURN;
            }
            M_Bite(item, creature->enemy, M_DAMAGE_NORMAL);

            // OG mistake
            // break;

        case M_STATE_ATTACK_JUMP:
            if (info.ahead) {
                torso_y = info.angle;
            }
            creature->maximum_turn = 0;
            if (ABS(info.angle) < M_WALK_TURN) {
                item->rot.y += info.angle;
            } else if (info.angle < 0) {
                item->rot.y -= M_WALK_TURN;
            } else {
                item->rot.y += M_WALK_TURN;
            }
            M_Bite(item, creature->enemy, M_DAMAGE_JUMP);
            break;
#pragma GCC diagnostic pop
        }
    }

    Creature_Tilt(item, tilt);
    Creature_Joint(item, 0, torso_y);

    if (item->current_anim_state >= M_STATE_CLIMB_4) {
        creature->maximum_turn = 0;
        Creature_Animate(item_num, angle, 0);
    } else {
        switch (Creature_Vault(item_num, angle, 2, 128)) {
        case -4:
            creature->maximum_turn = 0;
            Item_SwitchToObjAnim(item, M_ANIM_DOWN_4, 0, O_MONKEY);
            item->current_anim_state = M_STATE_DOWN_4;
            break;

        case -3:
            creature->maximum_turn = 0;
            Item_SwitchToObjAnim(item, M_ANIM_DOWN_3, 0, O_MONKEY);
            item->current_anim_state = M_STATE_DOWN_3;
            break;

        case -2:
            creature->maximum_turn = 0;
            Item_SwitchToObjAnim(item, M_ANIM_DOWN_2, 0, O_MONKEY);
            item->current_anim_state = M_STATE_DOWN_2;
            break;

        case 2:
            creature->maximum_turn = 0;
            Item_SwitchToObjAnim(item, M_ANIM_CLIMB_2, 0, O_MONKEY);
            item->current_anim_state = M_STATE_CLIMB_2;
            break;

        case 3:
            creature->maximum_turn = 0;
            Item_SwitchToObjAnim(item, M_ANIM_CLIMB_3, 0, O_MONKEY);
            item->current_anim_state = M_STATE_CLIMB_3;
            break;

        case 4:
            creature->maximum_turn = 0;
            Item_SwitchToObjAnim(item, M_ANIM_CLIMB_4, 0, O_MONKEY);
            item->current_anim_state = M_STATE_CLIMB_4;
            break;
        }
    }
}

static bool M_Draw(const ITEM *const item)
{
    const OBJECT *swap;
    if (item->ai_bits == AI_MODIFY) {
        swap = Object_Get(O_MESH_SWAP_3);
    } else {
        swap = Object_Get(O_MESH_SWAP_2);
    }
    return Object_DrawAnimatingItemWithSwap(item, swap);
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    if (!Object_Get(O_MESH_SWAP_2)->loaded) {
        Shell_ExitSystem("Monkey requires O_MESH_SWAP_2 (pickups)");
    }

    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;
    obj->draw_func = M_Draw;

    obj->shadow_size = UNIT_SHADOW / 2;
    obj->hit_points = 8;
    obj->radius = 102;
    obj->pivot_length = 0;
    obj->lot_setup = LOT_Setup(LOT_SETUP_CLIMBER);

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    Object_GetBone(obj, 0)->rot.z = true;
    Object_GetBone(obj, 7)->rot.x = true;
    Object_GetBone(obj, 7)->rot.y = true;
}

REGISTER_OBJECT(O_MONKEY, M_Setup)
