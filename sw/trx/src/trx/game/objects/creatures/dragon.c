#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/core/math.h>
#include <trx/debug.h>
#include <trx/game/camera.h>
#include <trx/game/collision.h>
#include <trx/game/creature.h>
#include <trx/game/input.h>
#include <trx/game/items/carrier.h>
#include <trx/game/lara.h>
#include <trx/game/lara/common.h>
#include <trx/game/objects/common.h>
#include <trx/game/output.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/spawn.h>
#include <trx/game/stats.h>

// clang-format off
#define M_CLOSE_SHIFT    900
#define M_FAR_SHIFT      2300
#define M_MID_SHIFT      ((M_CLOSE_SHIFT + M_FAR_SHIFT) / 2) // = 1600
#define M_COL_L          (-WALL_L / 2) // = -512
#define M_COL_R          (+WALL_L / 2) // = +512
#define M_CLOSE_RANGE    SQUARE(WALL_L * 3) // = 9437184
#define M_STOP_RANGE     SQUARE(WALL_L * 6) // = 37748736
#define M_WALK_TURN      (DEG_1 * 2) // = 364
#define M_NEED_TURN      (DEG_1) // = 182
#define M_TOUCH_L        0x7F000000
#define M_TOUCH_R        0x000000FE
#define M_ALMOST_LIVE    100
#define M_LIVE_TIME      330
#define M_ONE_PHASE_TIME 178
#define M_LIGHT_TIME     (-20)
#define M_BONE_TIME      (-100)
#define M_DISSOLVE_TIME  (-240)
#define M_DISSOLVE_SHIFT 10
#define M_HITPOINTS      300
#define M_TOUCH_DAMAGE   10
#define M_SWIPE_DAMAGE   250
#define M_RADIUS         (WALL_L / 3) // = 341
// clang-format on

typedef enum {
    M_STATE_EMPTY,
    M_STATE_WALK,
    M_STATE_LEFT,
    M_STATE_RIGHT,
    M_STATE_AIM,
    M_STATE_FIRE,
    M_STATE_STOP,
    M_STATE_TURN_LEFT,
    M_STATE_TURN_RIGHT,
    M_STATE_SWIPE_LEFT,
    M_STATE_SWIPE_RIGHT,
    M_STATE_DEATH,
} M_STATE;

typedef enum {
    // clang-format off
    M_ANIM_DIE       = 21,
    M_ANIM_DEAD      = 22,
    M_ANIM_RESURRECT = 23,
    // clang-format on
} M_ANIM;

typedef enum {
    M_MODE_ONE_PHASE,
    M_MODE_TWO_PHASE,
} M_MODE;

typedef struct {
    int16_t dragon_front_item_num;
    M_MODE mode;
} M_PRIV;

static const BITE m_DragonMouth = {
    .pos = { .x = 35, .y = 171, .z = 1168 },
    .mesh_num = 12,
};

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_SHOULD(JSON_READ(io, "mode", &p->mode));
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "mode", p->mode);
}

static int16_t M_GetFrontItemNum(const ITEM *const dragon_back_item)
{
    const M_PRIV *const p = dragon_back_item->priv;
    if (p == nullptr) {
        return NO_ITEM;
    }
    return p->dragon_front_item_num;
}

static bool M_IsTwoPhaseMode(const ITEM *const item)
{
    const M_PRIV *const p = item->priv;
    return p != nullptr && p->mode == M_MODE_TWO_PHASE;
}

static bool M_CanDropItemsBack(const ITEM *const item)
{
    return item->hit_points <= 0 && item->status == IS_DEACTIVATED;
}

static void M_InitialiseFront(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    item->include_in_kill_stats = false;
}

static void M_InitialiseBack(const int16_t item_num)
{
    ITEM *const dragon_back_item = Item_Get(item_num);
    M_PRIV *const p = dragon_back_item->priv;
    p->mode = M_MODE_TWO_PHASE;

    dragon_back_item->status = IS_INVISIBLE;
    dragon_back_item->shade.value_1 = -1;
    dragon_back_item->mesh_bits = 0x1FFFFF;

    p->dragon_front_item_num = Item_CreateLevelItem();
    ASSERT(p->dragon_front_item_num != NO_ITEM);

    ITEM *const dragon_front_item = Item_Get(p->dragon_front_item_num);
    dragon_front_item->object_id = O_DRAGON_FRONT;
    dragon_front_item->pos = dragon_back_item->pos;
    dragon_front_item->rot.y = dragon_back_item->rot.y;
    dragon_front_item->room_num = dragon_back_item->room_num;
    dragon_front_item->flags = IF_INVISIBLE;
    dragon_front_item->shade.value_1 = -1;
    Item_Initialise(p->dragon_front_item_num);
}

static bool M_TriggerBack(ITEM *const item, const TRIGGER *const trigger)
{
    M_PRIV *const p = item->priv;
    p->mode = M_MODE_ONE_PHASE;
    return true;
}

static void M_ActivateBack(ITEM *const dragon_back_item)
{
    if (dragon_back_item->active
        || dragon_back_item->status == IS_DEACTIVATED) {
        return;
    }

    const int16_t dragon_front_item_num = M_GetFrontItemNum(dragon_back_item);
    if (dragon_front_item_num == NO_ITEM) {
        return;
    }

    ITEM *const dragon_front_item = Item_Get(dragon_front_item_num);
    dragon_back_item->touch_bits = 0;
    dragon_front_item->touch_bits = 0;

    LOT_EnableBaddieAI(dragon_front_item_num, true);
    Item_AddActive(dragon_front_item_num);
    Item_AddActive(Item_GetIndex(dragon_back_item));
    dragon_back_item->status = IS_ACTIVE;
}

static void M_MarkDragonDead(ITEM *const dragon_back_item)
{
    const int16_t dragon_front_item_num = M_GetFrontItemNum(dragon_back_item);
    if (dragon_front_item_num == NO_ITEM) {
        return;
    }
    const ITEM *const dragon_front_item = Item_Get(dragon_front_item_num);
    CREATURE *const creature = dragon_front_item->creature_data;
    creature->flags = -1;
    Stats_AddKill();

    // Allow drops to occur at the beginning of the cinematic camera for a
    // better window to avoid seeing the items spawn.
    const ITEM_STATUS current_status = dragon_back_item->status;
    dragon_back_item->status = IS_DEACTIVATED;
    Carrier_TestItemDrops(Item_GetIndex(dragon_back_item));
    dragon_back_item->status = current_status;
}

static void M_PushLaraAway(
    ITEM *const lara_item, ITEM *const dragon_item, const int32_t shift)
{
    const int32_t cy = Math_Cos(dragon_item->rot.y);
    const int32_t sy = Math_Sin(dragon_item->rot.y);
    const int32_t base = shift < M_MID_SHIFT ? M_CLOSE_SHIFT : M_FAR_SHIFT;
    lara_item->pos.x += (cy * (base - shift)) >> W2V_SHIFT;
    lara_item->pos.z -= (sy * (base - shift)) >> W2V_SHIFT;
}

static void M_PullDagger(ITEM *const lara_item, ITEM *const dragon_back_item)
{
    lara_item->pos = dragon_back_item->pos;
    lara_item->rot = dragon_back_item->rot;
    lara_item->fall_speed = 0;
    lara_item->gravity = false;
    lara_item->speed = 0;

    const LARA_INFO *const lara = Lara_GetLaraInfo();
    Item_UpdateRoom(lara->item_num, dragon_back_item->room_num);

    Lara_SwitchToExtraState(LS_EXTRA_PULL_DAGGER);

    Camera_InvokeCinematic(lara_item, 0, 0);

    M_MarkDragonDead(dragon_back_item);
}

static void M_Bones(const int16_t item_num)
{
    const int16_t bone_front_item_num = Item_Create();
    const int16_t bone_back_item_num = Item_Create();

    if (bone_back_item_num == NO_ITEM || bone_front_item_num == NO_ITEM) {
        return;
    }

    const ITEM *const dragon_item = Item_Get(item_num);

    ITEM *const bone_back = Item_Get(bone_back_item_num);
    bone_back->object_id = O_DRAGON_BONES_3;
    bone_back->pos = dragon_item->pos;
    bone_back->rot.x = 0;
    bone_back->rot.y = dragon_item->rot.y;
    bone_back->rot.z = 0;
    bone_back->room_num = dragon_item->room_num;
    bone_back->shade.value_1 = -1;
    Item_Initialise(bone_back_item_num);

    ITEM *const bone_front = Item_Get(bone_front_item_num);
    bone_front->object_id = O_DRAGON_BONES_2;
    bone_front->pos = dragon_item->pos;
    bone_front->rot.x = 0;
    bone_front->rot.y = dragon_item->rot.y;
    bone_front->rot.z = 0;
    bone_front->room_num = dragon_item->room_num;
    bone_front->shade.value_1 = -1;
    Item_Initialise(bone_front_item_num);
    bone_front->mesh_bits = ~0xC00000u;
}

static void M_HandleSaveBack(ITEM *const item, const SAVEGAME_STAGE stage)
{
    if (stage == SAVEGAME_STAGE_AFTER_LOAD) {
        if (item->status == IS_DEACTIVATED && M_IsTwoPhaseMode(item)) {
            const int32_t y_pos = item->pos.y;
            int16_t room_num = item->room_num;
            const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
            item->pos.y = Room_GetHeight(sector, item->pos);
            const int16_t item_num = Item_GetIndex(item);
            M_Bones(item_num);
            item->pos.y = y_pos;
        }
    }
}

static void M_Collision(
    const int16_t item_num, ITEM *const lara_item, COLL_INFO *const coll)
{
    ITEM *const item = Item_Get(item_num);
    if (!Item_TestBoundsCollide(item, lara_item, coll->radius)) {
        return;
    }

    if (!Collide_TestCollision(item, lara_item)) {
        return;
    }

    if (item->current_anim_state != M_STATE_DEATH) {
        Lara_Col_ItemPush(item, coll, true, false);
        return;
    }

    const int32_t dx = lara_item->pos.x - item->pos.x;
    const int32_t dz = lara_item->pos.z - item->pos.z;
    const int32_t cy = Math_Cos(item->rot.y);
    const int32_t sy = Math_Sin(item->rot.y);
    const int32_t side_shift = (cy * dz + sy * dx) >> W2V_SHIFT;

    if (side_shift <= M_COL_L || side_shift >= M_COL_R) {
        Lara_Col_ItemPush(item, coll, true, false);
        return;
    }

    const int32_t shift = (cy * dx - sy * dz) >> W2V_SHIFT;
    const int32_t angle = lara_item->rot.y - item->rot.y;
    if (g_Input.action && item->object_id == O_DRAGON_BACK
        && M_IsTwoPhaseMode(item)
        && (Item_TestAnimEqual(item, M_ANIM_DEAD)
            || (Item_TestAnimEqual(item, M_ANIM_RESURRECT)
                && Item_TestFrameRange(item, 0, M_ALMOST_LIVE)))
        && !lara_item->gravity && shift <= M_MID_SHIFT
        && shift > M_CLOSE_SHIFT - 350 && side_shift > -350 && side_shift < 350
        && angle > DEG_90 - 30 * DEG_1 && angle < DEG_90 + 30 * DEG_1) {
        M_PullDagger(lara_item, item);
    } else {
        M_PushLaraAway(lara_item, item, shift);
    }
}

static void M_ControlFront(const int16_t item_num)
{
}

static void M_ControlBack(const int16_t item_num)
{
    const int16_t dragon_back_item_num = item_num;
    ITEM *const dragon_back_item = Item_Get(item_num);

    const int16_t dragon_front_item_num = M_GetFrontItemNum(dragon_back_item);
    if (dragon_front_item_num == NO_ITEM) {
        return;
    }
    ITEM *const dragon_front_item = Item_Get(dragon_front_item_num);
    if (!Creature_Activate(dragon_front_item_num)) {
        return;
    }

    int16_t angle = 0;
    int16_t head = 0;
    CREATURE *const creature = dragon_front_item->creature_data;
    const OBJECT *const front_obj = Object_Get(O_DRAGON_FRONT);
    const bool is_two_phase = M_IsTwoPhaseMode(dragon_back_item);

    if (dragon_front_item->hit_points <= 0) {
        if (dragon_front_item->current_anim_state != M_STATE_DEATH) {
            Item_SwitchToAnim(dragon_front_item, M_ANIM_DIE, 0);
            dragon_front_item->goal_anim_state = M_STATE_DEATH;
            dragon_front_item->current_anim_state = M_STATE_DEATH;
            creature->flags = 0;
        } else if (creature->flags >= 0) {
            creature->flags++;
            if (is_two_phase) {
                Spawn_MysticLight(dragon_front_item_num);
                if (creature->flags == M_LIVE_TIME) {
                    dragon_front_item->goal_anim_state = M_STATE_STOP;
                }
                if (creature->flags > M_LIVE_TIME + M_ALMOST_LIVE) {
                    dragon_front_item->hit_points = front_obj->hit_points / 2;
                }
            } else if (creature->flags == M_ONE_PHASE_TIME) {
                M_MarkDragonDead(dragon_back_item);
                creature->flags = M_DISSOLVE_TIME;
            }
        } else {
            if (creature->flags > M_LIGHT_TIME) {
                Output_AddDynamicLight(
                    dragon_front_item->pos,
                    ((4 * Random_GetDraw()) >> 15) + 12 + creature->flags / 2,
                    ((4 * Random_GetDraw()) >> 15) + 10 + creature->flags / 2);
            }

            if (creature->flags == M_BONE_TIME) {
                M_Bones(dragon_back_item_num);
            } else if (creature->flags == M_DISSOLVE_TIME) {
                Room_TestTriggers(dragon_back_item);
                LOT_DisableBaddieAI(dragon_front_item_num);
                dragon_front_item->status = IS_DEACTIVATED;
                dragon_back_item->status = IS_DEACTIVATED;
                if (is_two_phase) {
                    Item_Kill(dragon_front_item_num);
                    Item_Kill(dragon_back_item_num);
                } else {
                    Item_RemoveActive(dragon_front_item_num);
                    Item_RemoveActive(dragon_back_item_num);
                    dragon_front_item->collidable = false;
                    dragon_back_item->collidable = false;
                    dragon_front_item->flags |= IF_ONE_SHOT;
                    dragon_back_item->flags |= IF_ONE_SHOT;
                }
            } else if (creature->flags < M_BONE_TIME) {
                dragon_front_item->pos.y += M_DISSOLVE_SHIFT;
                dragon_back_item->pos.y += M_DISSOLVE_SHIFT;
            }

            creature->flags--;
            return;
        }
    } else {
        AI_INFO info;
        Creature_AIInfo(dragon_front_item, &info);
        Creature_Mood(dragon_front_item, &info, true);

        angle = Creature_Turn(dragon_front_item, M_WALK_TURN);
        const bool is_ahead = info.ahead && info.distance > M_CLOSE_RANGE
            && info.distance < M_STOP_RANGE;
        if (dragon_front_item->touch_bits) {
            Lara_TakeDamage(M_TOUCH_DAMAGE, true);
        }

        switch (dragon_front_item->current_anim_state) {
        case M_STATE_WALK:
            creature->flags = 0;
            if (is_ahead) {
                dragon_front_item->goal_anim_state = M_STATE_STOP;
            } else if (angle < -M_NEED_TURN) {
                if (info.distance < M_STOP_RANGE && info.ahead) {
                    dragon_front_item->goal_anim_state = M_STATE_STOP;
                } else {
                    dragon_front_item->goal_anim_state = M_STATE_LEFT;
                }
            } else if (angle > M_NEED_TURN) {
                if (info.distance < M_STOP_RANGE && info.ahead) {
                    dragon_front_item->goal_anim_state = M_STATE_STOP;
                } else {
                    dragon_front_item->goal_anim_state = M_STATE_RIGHT;
                }
            }
            break;

        case M_STATE_LEFT:
            if (angle > -M_NEED_TURN || is_ahead) {
                dragon_front_item->goal_anim_state = M_STATE_WALK;
            }
            break;

        case M_STATE_RIGHT:
            if (angle < M_NEED_TURN || is_ahead) {
                dragon_front_item->goal_anim_state = M_STATE_WALK;
            }
            break;

        case M_STATE_AIM:
            dragon_front_item->rot.y -= angle;
            if (info.ahead) {
                head = -info.angle;
            }

            if (is_ahead) {
                creature->flags = 30;
                dragon_front_item->goal_anim_state = M_STATE_FIRE;
            } else {
                creature->flags = 0;
                dragon_front_item->goal_anim_state = M_STATE_AIM;
            }
            break;

        case M_STATE_FIRE:
            dragon_front_item->rot.y -= angle;
            if (info.ahead) {
                head = -info.angle;
            }

            Sound_Effect(SFX_DRAGON_FIRE, &dragon_front_item->pos, SPM_NORMAL);

            if (creature->flags != 0) {
                if (info.ahead) {
                    Creature_Effect(
                        dragon_front_item, &m_DragonMouth, Spawn_FireStream);
                }
                creature->flags--;
            } else {
                dragon_front_item->goal_anim_state = M_STATE_STOP;
            }
            break;

        case M_STATE_STOP:
            dragon_front_item->rot.y -= angle;
            if (is_ahead) {
                dragon_front_item->goal_anim_state = M_STATE_AIM;
            } else if (info.distance > M_STOP_RANGE || !info.ahead) {
                dragon_front_item->goal_anim_state = M_STATE_WALK;
            } else if (info.distance >= M_CLOSE_RANGE || creature->flags != 0) {
                if (info.angle < 0) {
                    dragon_front_item->goal_anim_state = M_STATE_TURN_LEFT;
                } else {
                    dragon_front_item->goal_anim_state = M_STATE_TURN_RIGHT;
                }
            } else {
                creature->flags = 1;
                if (info.angle < 0) {
                    dragon_front_item->goal_anim_state = M_STATE_SWIPE_LEFT;
                } else {
                    dragon_front_item->goal_anim_state = M_STATE_SWIPE_RIGHT;
                }
            }
            break;

        case M_STATE_TURN_LEFT:
            creature->flags = 0;
            dragon_front_item->rot.y += -DEG_1 - angle;
            break;

        case M_STATE_TURN_RIGHT:
            creature->flags = 0;
            dragon_front_item->rot.y += DEG_1 - angle;
            break;

        case M_STATE_SWIPE_LEFT:
            if ((dragon_front_item->touch_bits & M_TOUCH_L) != 0) {
                Lara_TakeDamage(M_SWIPE_DAMAGE, true);
                creature->flags = 0;
            }
            break;

        case M_STATE_SWIPE_RIGHT:
            if ((dragon_front_item->touch_bits & M_TOUCH_R) != 0) {
                Lara_TakeDamage(M_SWIPE_DAMAGE, true);
                creature->flags = 0;
            }
            break;

        default:
            break;
        }
    }

    Creature_Head(dragon_front_item, head);
    Creature_Animate(dragon_front_item_num, angle, 0);
    dragon_back_item->current_anim_state =
        dragon_front_item->current_anim_state;
    const int16_t anim_num = Item_GetRelativeAnim(dragon_front_item);
    const int16_t frame_num = Item_GetRelativeFrame(dragon_front_item);
    Item_SwitchToAnim(dragon_back_item, anim_num, frame_num);
    dragon_back_item->pos = dragon_front_item->pos;
    dragon_back_item->rot = dragon_front_item->rot;
    Item_UpdateRoom(dragon_back_item_num, dragon_front_item->room_num);
}

static void M_SetupFront(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    SOFT_ASSERT(
        Object_Get(O_DRAGON_BACK)->loaded, "Dragon back object missing");
    obj->initialise_func = M_InitialiseFront;
    obj->control_func = M_ControlFront;
    obj->collision_func = M_Collision;

    obj->hit_points = M_HITPOINTS;
    obj->radius = M_RADIUS;
    obj->pivot_length = 300;

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    Object_GetBone(obj, 10)->rot.z = true;
}

static void M_SetupBack(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->initialise_func = M_InitialiseBack;
    obj->handle_save_func = M_HandleSaveBack;
    obj->trigger_func = M_TriggerBack;
    obj->activate_func = M_ActivateBack;
    obj->control_func = M_ControlBack;
    obj->collision_func = M_Collision;
    obj->can_drop_items_func = M_CanDropItemsBack;
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;

    obj->radius = M_RADIUS;

    obj->save_position = true;
    obj->save_flags = true;
    obj->save_anim = true;
    obj->priv_size = sizeof(M_PRIV);
}

REGISTER_OBJECT(O_DRAGON_FRONT, M_SetupFront)
REGISTER_OBJECT(O_DRAGON_BACK, M_SetupBack)
