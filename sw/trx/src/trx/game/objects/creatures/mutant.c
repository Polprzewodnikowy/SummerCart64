#include <trx/game/objects/creatures/mutant.h>

#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/items/carrier.h>
#include <trx/game/lara.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/spawn.h>

#define FLYER_CHARGE_DAMAGE 100
#define FLYER_LUNGE_DAMAGE 150
#define FLYER_PUNCH_DAMAGE 200
#define FLYER_PART_DAMAGE 100
#define FLYER_WALK_TURN (DEG_1 * 2) // = 364
#define FLYER_RUN_TURN (DEG_1 * 6) // = 1092
#define FLYER_POSE_CHANCE 80
#define FLYER_UNPOSE_CHANCE 256
#define FLYER_WALK_RANGE SQUARE(WALL_L * 9 / 2) // = 21233664
#define FLYER_ATTACK_1_RANGE SQUARE(600) // = 360000
#define FLYER_ATTACK_2_RANGE SQUARE(WALL_L * 5 / 2) // = 6553600
#define FLYER_ATTACK_3_RANGE SQUARE(300) // = 90000
#define FLYER_ATTACK_RANGE SQUARE(WALL_L * 15 / 4) // = 14745600
#define FLYER_TOUCH 0x678
#define FLYER_BULLET1 1
#define FLYER_BULLET2 2
#define FLYER_FLYMODE 4
#define FLYER_TWIST 8
#define FLYER_HITPOINTS 50
#define FLYER_RADIUS (WALL_L / 3) // = 341
#define FLYER_SMARTNESS 0x7FFF

#define WARRIOR2_SMARTNESS 0x2000

typedef enum {
    MUTANT_STATE_EMPTY = 0,
    MUTANT_STATE_STOP = 1,
    MUTANT_STATE_WALK = 2,
    MUTANT_STATE_RUN = 3,
    MUTANT_STATE_ATTACK_1 = 4,
    MUTANT_STATE_DEATH = 5,
    MUTANT_STATE_POSE = 6,
    MUTANT_STATE_ATTACK_2 = 7,
    MUTANT_STATE_ATTACK_3 = 8,
    MUTANT_STATE_AIM_1 = 9,
    MUTANT_STATE_AIM_2 = 10,
    MUTANT_STATE_SHOOT = 11,
    MUTANT_STATE_MUMMY = 12,
    MUTANT_STATE_FLY = 13,
} MUTANT_STATE;

static bool m_EnableExplosions = true;
static BITE m_WarriorBite = { .pos = { -27, 98, 0 }, .mesh_num = 10 };
static BITE m_WarriorRocket = { .pos = { 51, 213, 0 }, .mesh_num = 14 };
static BITE m_WarriorShard = { .pos = { -35, 269, 0 }, .mesh_num = 9 };

static void M_Initialise2(const int16_t item_num)
{
    Creature_Initialise(item_num);
    Item_Get(item_num)->mesh_bits = 0xFFE07FFF;
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

    CREATURE *const flyer = item->creature_data;
    int16_t head = 0;
    int16_t angle = 0;

    if (item->hit_points <= 0) {
        Item_Explode(
            item_num, -1,
            m_EnableExplosions ? FLYER_PART_DAMAGE : -FLYER_PART_DAMAGE);
        Sound_Effect(SFX_ATLANTEAN_DEATH, &item->pos, SPM_NORMAL);
        LOT_DisableBaddieAI(item_num);
        Item_Kill(item_num);
        item->status = IS_DEACTIVATED;
        Carrier_TestItemDrops(item_num);
        return;
    } else {
        flyer->lot.setup.step = STEP_L;
        flyer->lot.setup.drop = -STEP_L;
        flyer->lot.setup.fly = 0;

        AI_INFO info;
        Creature_AIInfo(item, &info);

        int32_t shoot1 = 0;
        int32_t shoot2 = 0;
        if (item->object_id != O_WARRIOR_3
            && Creature_CanTargetEnemy(item, &info)
            && (info.zone_num != info.enemy_zone_num
                || info.distance > FLYER_ATTACK_RANGE)) {
            if (info.angle > 0 && info.angle < DEG_45) {
                shoot1 = 1;
            } else if (info.angle < 0 && info.angle > -DEG_45) {
                shoot2 = 1;
            }
        }

        if (item->object_id == O_WARRIOR_1) {
            if (item->current_anim_state == MUTANT_STATE_FLY) {
                if ((flyer->flags & FLYER_FLYMODE) && flyer->mood != MOOD_ESCAPE
                    && info.zone_num == info.enemy_zone_num) {
                    flyer->flags &= ~FLYER_FLYMODE;
                }

                if (!(flyer->flags & FLYER_FLYMODE)) {
                    Creature_Mood(item, &info, true);
                }

                flyer->lot.setup.step = WALL_L * 30;
                flyer->lot.setup.drop = -WALL_L * 30;
                flyer->lot.setup.fly = STEP_L / 8;
                Creature_AIInfo(item, &info);
            } else if (
                (info.zone_num != info.enemy_zone_num && !shoot1 && !shoot2
                 && (!info.ahead || flyer->mood == MOOD_BORED))
                || flyer->mood == MOOD_ESCAPE) {
                flyer->flags |= FLYER_FLYMODE;
            }
        }

        if (info.ahead) {
            head = info.angle;
        }

        if (item->current_anim_state != MUTANT_STATE_FLY) {
            Creature_Mood(item, &info, false);
        } else if (flyer->flags & FLYER_FLYMODE) {
            Creature_Mood(item, &info, true);
        }

        angle = Creature_Turn(item, flyer->maximum_turn);

        switch (item->current_anim_state) {
        case MUTANT_STATE_MUMMY:
            item->goal_anim_state = MUTANT_STATE_STOP;
            break;

        case MUTANT_STATE_STOP:
            flyer->flags &= ~(FLYER_BULLET1 | FLYER_BULLET2 | FLYER_TWIST);
            if (flyer->flags & FLYER_FLYMODE) {
                item->goal_anim_state = MUTANT_STATE_FLY;
            } else if (item->touch_bits & FLYER_TOUCH) {
                item->goal_anim_state = MUTANT_STATE_ATTACK_3;
            } else if (info.bite && info.distance < FLYER_ATTACK_3_RANGE) {
                item->goal_anim_state = MUTANT_STATE_ATTACK_3;
            } else if (info.bite && info.distance < FLYER_ATTACK_1_RANGE) {
                item->goal_anim_state = MUTANT_STATE_ATTACK_1;
            } else if (shoot1) {
                item->goal_anim_state = MUTANT_STATE_AIM_1;
            } else if (shoot2) {
                item->goal_anim_state = MUTANT_STATE_AIM_2;
            } else if (
                flyer->mood == MOOD_BORED
                || (flyer->mood == MOOD_STALK
                    && info.distance < FLYER_WALK_RANGE)) {
                item->goal_anim_state = MUTANT_STATE_POSE;
            } else {
                item->goal_anim_state = MUTANT_STATE_RUN;
            }
            break;

        case MUTANT_STATE_POSE:
            head = 0;
            if (shoot1 || shoot2 || (flyer->flags & FLYER_FLYMODE)) {
                item->goal_anim_state = MUTANT_STATE_STOP;
            } else if (flyer->mood == MOOD_STALK) {
                if (info.distance < FLYER_WALK_RANGE) {
                    if (info.zone_num == info.enemy_zone_num
                        || Random_GetControl() < FLYER_UNPOSE_CHANCE) {
                        item->goal_anim_state = MUTANT_STATE_WALK;
                    }
                } else {
                    item->goal_anim_state = MUTANT_STATE_STOP;
                }
            } else if (
                flyer->mood == MOOD_BORED
                && Random_GetControl() < FLYER_UNPOSE_CHANCE) {
                item->goal_anim_state = MUTANT_STATE_WALK;
            } else if (
                flyer->mood == MOOD_ATTACK || flyer->mood == MOOD_ESCAPE) {
                item->goal_anim_state = MUTANT_STATE_STOP;
            }
            break;

        case MUTANT_STATE_WALK:
            flyer->maximum_turn = FLYER_WALK_TURN;
            if (shoot1 || shoot2 || (flyer->flags & FLYER_FLYMODE)) {
                item->goal_anim_state = MUTANT_STATE_STOP;
            } else if (
                flyer->mood == MOOD_ATTACK || flyer->mood == MOOD_ESCAPE) {
                item->goal_anim_state = MUTANT_STATE_STOP;
            } else if (
                flyer->mood == MOOD_BORED
                || (flyer->mood == MOOD_STALK
                    && info.zone_num != info.enemy_zone_num)) {
                if (Random_GetControl() < FLYER_POSE_CHANCE) {
                    item->goal_anim_state = MUTANT_STATE_POSE;
                }
            } else if (
                flyer->mood == MOOD_STALK && info.distance > FLYER_WALK_RANGE) {
                item->goal_anim_state = MUTANT_STATE_STOP;
            }
            break;

        case MUTANT_STATE_RUN:
            flyer->maximum_turn = FLYER_RUN_TURN;
            if (flyer->flags & FLYER_FLYMODE) {
                item->goal_anim_state = MUTANT_STATE_STOP;
            } else if (item->touch_bits & FLYER_TOUCH) {
                item->goal_anim_state = MUTANT_STATE_STOP;
            } else if (info.bite && info.distance < FLYER_ATTACK_1_RANGE) {
                item->goal_anim_state = MUTANT_STATE_STOP;
            } else if (info.ahead && info.distance < FLYER_ATTACK_2_RANGE) {
                item->goal_anim_state = MUTANT_STATE_ATTACK_2;
            } else if (shoot1 || shoot2) {
                item->goal_anim_state = MUTANT_STATE_STOP;
            } else if (
                flyer->mood == MOOD_BORED
                || (flyer->mood == MOOD_STALK
                    && info.distance < FLYER_WALK_RANGE)) {
                item->goal_anim_state = MUTANT_STATE_STOP;
            }
            break;

        case MUTANT_STATE_ATTACK_1:
            if (item->required_anim_state == MUTANT_STATE_EMPTY
                && (item->touch_bits & FLYER_TOUCH)) {
                Creature_Effect(item, &m_WarriorBite, Spawn_Blood);
                Lara_TakeDamage(FLYER_LUNGE_DAMAGE, true);
                item->required_anim_state = MUTANT_STATE_STOP;
            }
            break;

        case MUTANT_STATE_ATTACK_2:
            if (item->required_anim_state == MUTANT_STATE_EMPTY
                && (item->touch_bits & FLYER_TOUCH)) {
                Creature_Effect(item, &m_WarriorBite, Spawn_Blood);
                Lara_TakeDamage(FLYER_CHARGE_DAMAGE, true);
                item->required_anim_state = MUTANT_STATE_RUN;
            }
            break;

        case MUTANT_STATE_ATTACK_3:
            if (item->required_anim_state == MUTANT_STATE_EMPTY
                && (item->touch_bits & FLYER_TOUCH)) {
                Creature_Effect(item, &m_WarriorBite, Spawn_Blood);
                Lara_TakeDamage(FLYER_PUNCH_DAMAGE, true);
                item->required_anim_state = MUTANT_STATE_STOP;
            }
            break;

        case MUTANT_STATE_AIM_1:
            flyer->flags |= FLYER_TWIST;
            flyer->flags |= FLYER_BULLET1;
            if (shoot1) {
                item->goal_anim_state = MUTANT_STATE_SHOOT;
            } else {
                item->goal_anim_state = MUTANT_STATE_STOP;
            }
            break;

        case MUTANT_STATE_AIM_2:
            flyer->flags |= FLYER_BULLET2;
            if (shoot2) {
                item->goal_anim_state = MUTANT_STATE_SHOOT;
            } else {
                item->goal_anim_state = MUTANT_STATE_STOP;
            }
            break;

        case MUTANT_STATE_SHOOT:
            if (flyer->flags & FLYER_BULLET1) {
                flyer->flags &= ~FLYER_BULLET1;
                Creature_Effect(item, &m_WarriorShard, Spawn_AtlanteanShard);
            } else if (flyer->flags & FLYER_BULLET2) {
                flyer->flags &= ~FLYER_BULLET2;
                Creature_Effect(item, &m_WarriorRocket, Spawn_AtlanteanBomb);
            }
            break;

        case MUTANT_STATE_FLY:
            if (!(flyer->flags & FLYER_FLYMODE) && item->pos.y == item->floor) {
                item->goal_anim_state = MUTANT_STATE_STOP;
            }
            break;
        }
    }

    if (!(flyer->flags & FLYER_TWIST)) {
        flyer->head_rotation = flyer->neck_rotation;
    }

    Creature_Head(item, head);

    if (!(flyer->flags & FLYER_TWIST)) {
        flyer->neck_rotation = flyer->head_rotation;
        flyer->head_rotation = 0;
    } else {
        flyer->neck_rotation = 0;
    }

    Creature_Animate(item_num, angle, 0);
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }
    obj->initialise_func = Creature_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;
    obj->shadow_size = UNIT_SHADOW / 3;
    obj->hit_points = FLYER_HITPOINTS;
    obj->pivot_length = 150;
    obj->radius = FLYER_RADIUS;
    obj->smartness = FLYER_SMARTNESS;
    obj->lot_setup = LOT_Setup(LOT_SETUP_BEAST);
    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_anim = true;
    obj->save_flags = true;
    Object_GetBone(obj, 0)->rot.y = true;
    Object_GetBone(obj, 2)->rot.y = true;
}

static void M_Setup2(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }
    *obj = *Object_Get(O_WARRIOR_1);
    obj->setup_func = M_Setup2;
    obj->initialise_func = M_Initialise2;
    obj->smartness = WARRIOR2_SMARTNESS;
    obj->lot_setup = LOT_Setup(LOT_SETUP_DEFAULT);
}

static void M_Setup3(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }
    *obj = *Object_Get(O_WARRIOR_1);
    obj->setup_func = M_Setup3;
    obj->initialise_func = M_Initialise2;
    obj->lot_setup = LOT_Setup(LOT_SETUP_DEFAULT);
}

void Mutant_ToggleExplosions(bool enable)
{
    m_EnableExplosions = enable;
}

REGISTER_OBJECT(O_WARRIOR_1, M_Setup)
REGISTER_OBJECT(O_WARRIOR_2, M_Setup2)
REGISTER_OBJECT(O_WARRIOR_3, M_Setup3)
