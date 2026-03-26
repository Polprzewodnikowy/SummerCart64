#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/spawn.h>

#define APE_ATTACK_DAMAGE 200
#define APE_TOUCH 0xFF00
#define APE_DIE_ANIM 7
#define APE_RUN_TURN (DEG_1 * 5) // = 910
#define APE_DISPLAY_ANGLE (DEG_1 * 45) // = 8190
#define APE_ATTACK_RANGE SQUARE(430) // = 184900
#define APE_PANIC_RANGE SQUARE(WALL_L * 2) // = 4194304
#define APE_JUMP_CHANCE 160
#define APE_WARN1_CHANCE (APE_JUMP_CHANCE + 160) // = 320
#define APE_WARN2_CHANCE (APE_WARN1_CHANCE + 160) // = 480
#define APE_RUN_LEFT_CHANCE (APE_WARN2_CHANCE + 272) // = 752
#define APE_ATTACK_FLAG 1
#define APE_VAULT_ANIM 19
#define APE_TURN_L_FLAG 2
#define APE_TURN_R_FLAG 4
#define APE_SHIFT 75
#define APE_HITPOINTS 22
#define APE_RADIUS (WALL_L / 3) // = 341
#define APE_SMARTNESS 0x7FFF

typedef enum {
    APE_STATE_EMPTY = 0,
    APE_STATE_STOP = 1,
    APE_STATE_WALK = 2,
    APE_STATE_RUN = 3,
    APE_STATE_ATTACK = 4,
    APE_STATE_DEATH = 5,
    APE_STATE_WARNING_1 = 6,
    APE_STATE_WARNING_2 = 7,
    APE_STATE_RUN_LEFT = 8,
    APE_STATE_RUN_RIGHT = 9,
    APE_STATE_JUMP = 10,
    APE_STATE_VAULT = 11,
} APE_STATE;

static BITE m_ApeBite = { .pos = { 0, -19, 75 }, .mesh_num = 15 };

static bool M_Vault(int16_t item_num, int16_t angle)
{
    ITEM *const item = Item_Get(item_num);
    CREATURE *const ape = item->creature_data;
    int32_t x = item->pos.x >> WALL_SHIFT;
    int32_t y = item->pos.y;
    int32_t z = item->pos.z >> WALL_SHIFT;
    int16_t room_num = item->room_num;

    if (ape->flags & APE_TURN_L_FLAG) {
        item->rot.y -= DEG_90;
        ape->flags &= ~APE_TURN_L_FLAG;
    } else if (ape->flags & APE_TURN_R_FLAG) {
        item->rot.y += DEG_90;
        ape->flags &= ~APE_TURN_R_FLAG;
    }

    Creature_Animate(item_num, angle, 0);

    if (item->pos.y > y - STEP_L * 3 / 2) {
        return false;
    }

    int32_t x_floor = item->pos.x >> WALL_SHIFT;
    int32_t z_floor = item->pos.z >> WALL_SHIFT;

    if (z == z_floor) {
        if (x == x_floor) {
            return false;
        }

        if (x >= x_floor) {
            item->rot.y = -DEG_90;
            item->pos.x = (x << WALL_SHIFT) + APE_SHIFT;
        } else {
            item->rot.y = DEG_90;
            item->pos.x = (x_floor << WALL_SHIFT) - APE_SHIFT;
        }
    } else if (x == x_floor) {
        if (z < z_floor) {
            item->rot.y = 0;
            item->pos.z = (z_floor << WALL_SHIFT) - APE_SHIFT;
        } else {
            item->rot.y = -DEG_180;
            item->pos.z = (z << WALL_SHIFT) + APE_SHIFT;
        }
    }

    item->floor = y;
    item->pos.y = y;

    Item_UpdateRoom(item_num, room_num);

    return true;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    if (item->status == IS_INVISIBLE) {
        if (!LOT_EnableBaddieAI(item_num, 0)) {
            return;
        }
        item->status = IS_ACTIVE;
    }

    CREATURE *const ape = item->creature_data;
    int16_t head = 0;
    int16_t angle = 0;

    if (item->hit_points <= 0) {
        if (item->current_anim_state != APE_STATE_DEATH) {
            item->current_anim_state = APE_STATE_DEATH;
            Item_SwitchToAnim(
                item, APE_DIE_ANIM + (int16_t)(Random_GetControl() / 0x4000),
                0);
        }
    } else {
        AI_INFO info;
        Creature_AIInfo(item, &info);

        if (info.ahead) {
            head = info.angle;
        }

        Creature_Mood(item, &info, false);

        angle = Creature_Turn(item, ape->maximum_turn);

        if (item->hit_status || info.distance < APE_PANIC_RANGE) {
            ape->flags |= APE_ATTACK_FLAG;
        }

        switch (item->current_anim_state) {
        case APE_STATE_STOP:
            if (ape->flags & APE_TURN_L_FLAG) {
                item->rot.y -= DEG_90;
                ape->flags &= ~APE_TURN_L_FLAG;
            } else if (ape->flags & APE_TURN_R_FLAG) {
                item->rot.y += DEG_90;
                ape->flags &= ~APE_TURN_R_FLAG;
            }

            if (item->required_anim_state) {
                item->goal_anim_state = item->required_anim_state;
            } else if (info.bite && info.distance < APE_ATTACK_RANGE) {
                item->goal_anim_state = APE_STATE_ATTACK;
            } else if (
                !(ape->flags & APE_ATTACK_FLAG)
                && info.zone_num == info.enemy_zone_num && info.ahead) {
                int16_t random = Random_GetControl() >> 5;
                if (random < APE_JUMP_CHANCE) {
                    item->goal_anim_state = APE_STATE_JUMP;
                } else if (random < APE_WARN1_CHANCE) {
                    item->goal_anim_state = APE_STATE_WARNING_1;
                } else if (random < APE_WARN2_CHANCE) {
                    item->goal_anim_state = APE_STATE_WARNING_2;
                } else if (random < APE_RUN_LEFT_CHANCE) {
                    item->goal_anim_state = APE_STATE_RUN_LEFT;
                    ape->maximum_turn = 0;
                } else {
                    item->goal_anim_state = APE_STATE_RUN_RIGHT;
                    ape->maximum_turn = 0;
                }
            } else {
                item->goal_anim_state = APE_STATE_RUN;
            }
            break;

        case APE_STATE_RUN:
            ape->maximum_turn = APE_RUN_TURN;
            if (!ape->flags && info.angle > -APE_DISPLAY_ANGLE
                && info.angle < APE_DISPLAY_ANGLE) {
                item->goal_anim_state = APE_STATE_STOP;
            } else if (info.ahead && (item->touch_bits & APE_TOUCH)) {
                item->required_anim_state = APE_STATE_ATTACK;
                item->goal_anim_state = APE_STATE_STOP;
            } else if (ape->mood != MOOD_ESCAPE) {
                int16_t random = Random_GetControl();
                if (random < APE_JUMP_CHANCE) {
                    item->required_anim_state = APE_STATE_JUMP;
                    item->goal_anim_state = APE_STATE_STOP;
                } else if (random < APE_WARN1_CHANCE) {
                    item->required_anim_state = APE_STATE_WARNING_1;
                    item->goal_anim_state = APE_STATE_STOP;
                } else if (random < APE_WARN2_CHANCE) {
                    item->required_anim_state = APE_STATE_WARNING_2;
                    item->goal_anim_state = APE_STATE_STOP;
                }
            }
            break;

        case APE_STATE_RUN_LEFT:
            if (!(ape->flags & APE_TURN_R_FLAG)) {
                item->rot.y -= DEG_90;
                ape->flags |= APE_TURN_R_FLAG;
            }
            item->goal_anim_state = APE_STATE_STOP;
            break;

        case APE_STATE_RUN_RIGHT:
            if (!(ape->flags & APE_TURN_L_FLAG)) {
                item->rot.y += DEG_90;
                ape->flags |= APE_TURN_L_FLAG;
            }
            item->goal_anim_state = APE_STATE_STOP;
            break;

        case APE_STATE_ATTACK:
            if (!item->required_anim_state && (item->touch_bits & APE_TOUCH)) {
                Creature_Effect(item, &m_ApeBite, Spawn_Blood);
                Lara_TakeDamage(APE_ATTACK_DAMAGE, true);
                item->required_anim_state = APE_STATE_STOP;
            }
            break;
        }
    }

    Creature_Head(item, head);

    if (item->current_anim_state == APE_STATE_VAULT) {
        Creature_Animate(item_num, angle, 0);
    } else if (M_Vault(item_num, angle)) {
        ape->maximum_turn = 0;
        item->current_anim_state = APE_STATE_VAULT;
        Item_SwitchToAnim(item, APE_VAULT_ANIM, 0);
    }
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }
    obj->initialise_func = Creature_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->hit_points = APE_HITPOINTS;
    obj->pivot_length = 250;
    obj->radius = APE_RADIUS;
    obj->smartness = APE_SMARTNESS;
    obj->lot_setup = LOT_Setup(LOT_SETUP_JUMPER);
    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_anim = true;
    obj->save_flags = true;

    Object_GetBone(obj, 13)->rot.y = true;
}

REGISTER_OBJECT(O_APE, M_Setup)
