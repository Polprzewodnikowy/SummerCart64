#include <trx/config.h>
#include <trx/core/utils.h>
#include <trx/game/camera.h>
#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/sparks.h>
#include <trx/game/spawn.h>

// clang-format off
#define M_BIFF_DAMAGE       100
#define M_BIFF_ENEMY_DAMAGE 5
#define M_WALK_TURN         (9 * DEG_1) // = 1638
#define M_RUN_TURN          (6 * DEG_1) // = 1092
#define M_WAIT_TURN         (2 * DEG_1) // = 364
#define M_PIPE_RANGE        SQUARE(WALL_L * 8) // = 0x4000000
#define M_CLOSE_RANGE       SQUARE(WALL_L / 2) // = 0x40000
#define M_WALK_RANGE        SQUARE(WALL_L * 2) // = 0x400000
#define M_AWARE_DISTANCE    (WALL_L)
#define M_HIT_RANGE         (STEP_L * 2)
#define M_TOUCH_BITS        0x2400
// clang-format on

typedef enum {
    M_ANIM_DEATH_STANDING = 20,
    M_ANIM_DEATH_KNEELING = 21,
} M_ANIM;

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
    M_STATE_WAIT_2
} M_STATE;

static BITE m_BiffHit = {
    .pos = { .x = 0, .y = 0, .z = -200 },
    .mesh_num = 13,
};
static BITE m_ShootHit = {
    .pos = { .x = 8, .y = 40, .z = -248 },
    .mesh_num = 13,
};

static void M_SpawnDart(ITEM *const item)
{
    const int16_t dart_item_num = Item_Create();
    if (dart_item_num == NO_ITEM) {
        return;
    }

    ITEM *const dart_item = Item_Get(dart_item_num);
    dart_item->object_id = O_POISON_DART;
    dart_item->room_num = item->room_num;

    XYZ_32 pos1 = m_ShootHit.pos;
    Collide_GetJointAbsPosition(item, &pos1, m_ShootHit.mesh_num);

    XYZ_32 pos2 = {};
    const CREATURE *const creature = item->creature_data;
    if (g_Config.gameplay.fix_pipeman_aim && creature->enemy != nullptr) {
        if (creature->enemy == Lara_GetItem()) {
            Lara_GetMeshPos(LM_TORSO, &pos2);
        } else {
            pos2 = creature->enemy->pos;
        }
    } else {
        pos2 = m_ShootHit.pos;
        pos2.z <<= 1;
        Collide_GetJointAbsPosition(item, &pos2, m_ShootHit.mesh_num);
    }

    int16_t angles[2];
    Math_GetVectorAngles(
        pos2.x - pos1.x, pos2.y - pos1.y, pos2.z - pos1.z, angles);
    dart_item->pos = pos1;

    Item_Initialise(dart_item_num);
    dart_item->rot.x = angles[1];
    dart_item->rot.y = angles[0];
    dart_item->speed = 256;
    Item_AddActive(dart_item_num);
    dart_item->status = IS_ACTIVE;

    XYZ_32 smoke_pos = {
        .x = m_ShootHit.pos.x,
        .y = m_ShootHit.pos.y,
        .z = m_ShootHit.pos.z + 96,
    };
    Collide_GetJointAbsPosition(item, &smoke_pos, m_ShootHit.mesh_num);
    for (int32_t i = 0; i < 2; i++) {
        Sparks_TriggerDartSmoke(smoke_pos, (XZ_32) {}, true);
    }
}

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    const ITEM *const lara_item = Lara_GetItem();
    const LARA_INFO *const lara = Lara_GetLaraInfo();

    ITEM *const item = Item_Get(item_num);
    CREATURE *const creature = item->creature_data;
    int16_t angle = 0;
    int16_t tilt = 0;
    int16_t torso_x = 0;
    int16_t torso_y = 0;
    int16_t head = 0;

    if (item->hit_points <= 0) {
        if (item->current_anim_state != M_STATE_DEATH) {
            if (item->current_anim_state == M_STATE_WAIT_1
                || item->current_anim_state == M_STATE_ATTACK_1) {
                Item_SwitchToAnim(item, M_ANIM_DEATH_KNEELING, 0);
            } else {
                Item_SwitchToAnim(item, M_ANIM_DEATH_STANDING, 0);
            }
            item->current_anim_state = M_STATE_DEATH;
        }
    } else {
        if (item->ai_bits != 0) {
            Creature_GetAITarget(creature);
        }

        AI_INFO info;
        Creature_AIInfo(item, &info);
        Creature_UpdateMood(item, &info, info.zone_num == info.enemy_zone_num);

        if (item->hit_status && lara->poison_timer >= 256
            && creature->mood == MOOD_BORED) {
            creature->mood = MOOD_ESCAPE;
        }

        Creature_ApplyMood(item, &info, false);
        angle = Creature_Turn(
            item,
            creature->mood == MOOD_BORED ? M_WAIT_TURN
                                         : creature->maximum_turn);

        if (info.ahead) {
            head = info.angle >> 1;
            torso_y = info.angle >> 1;
        }

        if (item->hit_status
            || (creature->enemy == lara_item
                && (info.distance < M_AWARE_DISTANCE
                    || Creature_CanSeeEnemy(item, &info))
                && (ABS(lara_item->pos.y - item->pos.y) < WALL_L * 2))) {
            Creature_AlertAllGuards(item_num);
        }

        switch (item->current_anim_state) {
        case M_STATE_WAIT_1:
            if (info.ahead) {
                torso_x = info.x_angle >> 1;
                torso_y = info.angle;
            }

            creature->flags &= 0xFFF;
            creature->maximum_turn = M_WAIT_TURN;

            if (item->ai_bits & AI_GUARD) {
                head = Creature_AIGuard(creature);
                torso_x = 0;
                torso_y = 0;
                creature->maximum_turn = 0;

                if (!(Random_GetControl() & 0xFF)) {
                    item->goal_anim_state = M_STATE_WAIT_2;
                }
            } else if (creature->mood == MOOD_ESCAPE) {
                if (lara->target == item || !info.ahead || item->hit_status) {
                    item->goal_anim_state = M_STATE_RUN;
                } else {
                    item->goal_anim_state = M_STATE_WAIT_1;
                }
            } else if (info.bite && info.distance < M_CLOSE_RANGE) {
                item->goal_anim_state = M_STATE_WAIT_2;
            } else if (info.bite && info.distance < M_WALK_RANGE) {
                item->goal_anim_state = M_STATE_WALK;
            } else if (
                Creature_CanTargetEnemy(item, &info)
                && info.distance < M_PIPE_RANGE) {
                item->goal_anim_state = M_STATE_ATTACK_1;
            } else if (creature->mood == MOOD_BORED) {
                if (Random_GetControl() < 512) {
                    item->goal_anim_state = M_STATE_WALK;
                }
            } else {
                item->goal_anim_state = M_STATE_RUN;
            }
            break;

        case M_STATE_WALK:
            creature->maximum_turn = M_WALK_TURN;

            if (info.bite && info.distance < M_CLOSE_RANGE) {
                item->goal_anim_state = M_STATE_WAIT_2;
            } else if (info.bite && info.distance < M_WALK_RANGE) {
                item->goal_anim_state = M_STATE_WALK;
            } else if (
                Creature_CanTargetEnemy(item, &info)
                && info.distance < M_PIPE_RANGE) {
                item->goal_anim_state = M_STATE_WAIT_1;
            } else if (creature->mood == MOOD_ESCAPE) {
                item->goal_anim_state = M_STATE_RUN;
            } else if (creature->mood == MOOD_BORED) {
                if (Random_GetControl() > 512) {
                    item->goal_anim_state = M_STATE_WALK;
                } else if (Random_GetControl() > 512) {
                    item->goal_anim_state = M_STATE_WAIT_2;
                } else {
                    item->goal_anim_state = M_STATE_WAIT_1;
                }
            } else if (info.distance > M_WALK_RANGE) {
                item->goal_anim_state = M_STATE_RUN;
            }
            break;

        case M_STATE_RUN:
            creature->flags &= 0xFFF;
            creature->maximum_turn = M_RUN_TURN;
            tilt = angle >> 2;

            if (info.bite && info.distance < M_CLOSE_RANGE) {
                item->goal_anim_state = M_STATE_WAIT_2;
            } else if (
                Creature_CanTargetEnemy(item, &info)
                && info.distance < M_PIPE_RANGE) {
                item->goal_anim_state = M_STATE_WAIT_1;
            }

            if (item->ai_bits & AI_GUARD) {
                item->goal_anim_state = M_STATE_WAIT_2;
            } else if (
                creature->mood == MOOD_ESCAPE && lara->target != item
                && info.ahead) {
                item->goal_anim_state = M_STATE_WAIT_2;
            } else if (creature->mood == MOOD_BORED) {
                item->goal_anim_state = M_STATE_WAIT_1;
            }
            break;

        case M_STATE_ATTACK_1:
            if (info.ahead) {
                torso_x = info.x_angle;
                torso_y = info.angle;
            }

            creature->maximum_turn = 0;

            if (ABS(info.angle) < M_WAIT_TURN) {
                item->rot.y += info.angle;
            } else if (info.angle < 0) {
                item->rot.y -= M_WAIT_TURN;
            } else {
                item->rot.y += M_WAIT_TURN;
            }

            if (Item_GetRelativeFrame(item) == 15) {
                M_SpawnDart(item);
                item->goal_anim_state = M_STATE_WAIT_1;
            }
            break;

        case M_STATE_ATTACK_3:
            ITEM *const enemy = creature->enemy;
            if (enemy == lara_item) {
                if (!(creature->flags & 0xF000)
                    && item->touch_bits & M_TOUCH_BITS) {
                    Lara_TakeDamage(M_BIFF_DAMAGE, true);
                    creature->flags |= 0x1000;
                    Sound_Effect(SFX_LARA_THUD, &item->pos, SPM_NORMAL);
                    Creature_Effect(item, &m_BiffHit, Spawn_Blood);
                }
            } else if (!(creature->flags & 0xF000) && enemy != nullptr) {
                if (Item_IsNearby(enemy, item, M_HIT_RANGE)) {
                    Item_TakeDamage(enemy, M_BIFF_ENEMY_DAMAGE, true);
                    creature->flags |= 0x1000;
                    Sound_Effect(SFX_LARA_THUD, &item->pos, SPM_NORMAL);
                }
            }
            break;

        case M_STATE_AIM_3:
            if (!info.bite || info.distance > M_CLOSE_RANGE) {
                item->goal_anim_state = M_STATE_WAIT_2;
            } else {
                item->goal_anim_state = M_STATE_ATTACK_3;
            }
            break;

        case M_STATE_WAIT_2:
            creature->flags &= 0xFFF;
            creature->maximum_turn = M_WAIT_TURN;

            if (item->ai_bits & AI_GUARD) {
                head = Creature_AIGuard(creature);
                torso_x = 0;
                torso_y = 0;
                creature->maximum_turn = 0;

                if (!(Random_GetControl() & 0xFF)) {
                    item->goal_anim_state = M_STATE_WAIT_1;
                }
            } else if (creature->mood == MOOD_ESCAPE) {
                if (lara->target != item && info.ahead && !item->hit_status) {
                    item->goal_anim_state = M_STATE_WAIT_1;
                } else {
                    item->goal_anim_state = M_STATE_RUN;
                }
            } else if (info.bite && info.distance < M_CLOSE_RANGE) {
                item->goal_anim_state = M_STATE_ATTACK_3;
            } else if (info.bite && info.distance < M_WALK_RANGE) {
                item->goal_anim_state = M_STATE_WALK;
            } else if (
                Creature_CanTargetEnemy(item, &info)
                && info.distance < M_PIPE_RANGE) {
                item->goal_anim_state = M_STATE_WAIT_1;
            } else if (
                creature->mood == MOOD_BORED && Random_GetControl() < 512) {
                item->goal_anim_state = M_STATE_WALK;
            } else {
                item->goal_anim_state = M_STATE_RUN;
            }
            break;
        }
    }

    Creature_Tilt(item, tilt);
    Creature_Joint(item, 0, torso_y);
    Creature_Joint(item, 1, torso_x);
    Creature_Joint(item, 2, head - torso_y);
    Creature_Joint(item, 3, 0);
    Creature_Animate(item_num, angle, 0);
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;

    obj->shadow_size = UNIT_SHADOW / 2;
    obj->hit_points = 28;
    obj->radius = 102;

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    Object_GetBone(obj, 6)->rot.y = true;
    Object_GetBone(obj, 6)->rot.x = true;
    Object_GetBone(obj, 13)->rot.y = true;
    Object_GetBone(obj, 13)->rot.x = true;
}

REGISTER_OBJECT(O_TRIBE_PIPEMAN, M_Setup)
