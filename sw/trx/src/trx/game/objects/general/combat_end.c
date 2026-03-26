#include <trx/core/utils.h>
#include <trx/game/camera.h>
#include <trx/game/collision/los.h>
#include <trx/game/creature.h>
#include <trx/game/game.h>
#include <trx/game/gun.h>
#include <trx/game/items.h>
#include <trx/game/lara.h>
#include <trx/game/lara/vehicle.h>
#include <trx/game/objects/common.h>
#include <trx/game/objects/vars.h>
#include <trx/game/pathing.h>

#define M_CUTSCENE_DELAY (5 * LOGIC_FPS) // = 150
#define M_BOSS_TYPE O_CULT_3

static int16_t m_BossTimer = 0;
static uint16_t m_BossCount = 0;

static int32_t M_CountAliveEnemies(void)
{
    int32_t count = 0;
    for (int32_t i = 0; i < Item_GetLevelCount(); i++) {
        const ITEM *const item = Item_Get(i);
        if (item->object_id != M_BOSS_TYPE && Item_IsAlive(item)
            && Creature_IsHostile(item)) {
            count++;
        }
    }
    return count;
}

static bool M_IsBossDead(void)
{
    for (int32_t i = 0; i < Item_GetLevelCount(); i++) {
        const ITEM *const item = Item_Get(i);
        if (item->object_id == M_BOSS_TYPE && !Item_IsAlive(item)) {
            return true;
        }
    }
    return false;
}

static int16_t M_FindNearestBoss(void)
{
    // Note that in the original, the first boss item was always selected here.
    // For speedruns, the change here means that is no longer guaranteed, but
    // positional manipulation can be used for the best outcome.
    int32_t best_dist = INT32_MAX;
    int16_t best_item = NO_ITEM;
    for (int32_t i = 0; i < Item_GetLevelCount(); i++) {
        const ITEM *const item = Item_Get(i);
        if (item->object_id != M_BOSS_TYPE) {
            continue;
        }

        if (item->status == IS_ACTIVE || item->status == IS_DEACTIVATED) {
            best_item = i;
            break;
        }

        const ITEM *const lara_item = Lara_GetItem();
        const GAME_VECTOR start = {
            .x = lara_item->pos.x,
            .y = lara_item->pos.y - STEP_L * 2,
            .z = lara_item->pos.z,
            .room_num = lara_item->room_num,
        };

        GAME_VECTOR target = {
            .x = item->pos.x,
            .y = item->pos.y - STEP_L * 2,
            .z = item->pos.z,
            .room_num = item->room_num,
        };

        if (!LOS_Check(&start, &target, true)) {
            const int32_t dx = (lara_item->pos.x - item->pos.x) >> 6;
            const int32_t dy = (lara_item->pos.y - item->pos.y) >> 6;
            const int32_t dz = (lara_item->pos.z - item->pos.z) >> 6;
            const int32_t dist = SQUARE(dx) + SQUARE(dy) + SQUARE(dz);
            if (dist < best_dist) {
                best_dist = dist;
                best_item = i;
            }
        }
    }
    return best_item;
}

static void M_ActivateNearestBoss(void)
{
    const int16_t item_num = M_FindNearestBoss();
    if (item_num == NO_ITEM) {
        return;
    }
    ITEM *const item = Item_Get(item_num);
    if (item->status != IS_ACTIVE && item->status != IS_DEACTIVATED) {
        item->touch_bits = 0;
        item->status = IS_ACTIVE;
        item->mesh_bits = 0xFFFF1FFF;
        Item_AddActive(item_num);
        LOT_EnableBaddieAI(item_num, true);
    }
}

static void M_PrepareCutscene(const int16_t item_num)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (lara->gun_type == LGT_FLARE) {
        Lara_Flare_Undraw();
        lara->flare.control = false;
        lara->left_arm.lock = false;
    }

    Lara_Vehicle_Dismount();
    Gun_SetLaraHandLMesh(LGT_UNARMED);
    Gun_SetLaraHandRMesh(LGT_UNARMED);
    lara->water_status = LWS_ABOVE_WATER;
    lara->target = nullptr;

    ITEM *const item = Item_Get(item_num);
    Creature_SpecialKill(item, 0, 0, LS_EXTRA_END_HOUSE);

    Camera_InvokeCinematic(item, 428, 0);
}

static void M_Control(const int16_t item_num)
{
    const int32_t alive_enemies = M_CountAliveEnemies();
    const int32_t is_boss_dead = M_IsBossDead();
    if (alive_enemies == 0 && m_BossTimer == 0) {
        m_BossTimer = 1;
        M_ActivateNearestBoss();
    } else if (alive_enemies == 0 && is_boss_dead) {
        m_BossTimer++;
        if (m_BossTimer == M_CUTSCENE_DELAY) {
            M_PrepareCutscene(item_num);
        }
    }
}

OBJECT_ID CombatEnd_GetBossType(void)
{
    return M_BOSS_TYPE;
}

bool CombatEnd_IsWaitingForBoss(void)
{
    for (int32_t i = 0; i < Item_GetTotalCount(); i++) {
        if (Item_Get(i)->object_id == O_COMBAT_END) {
            return m_BossTimer == 0;
        }
    }
    return false;
}

bool CombatEnd_IsComplete(void)
{
    return m_BossTimer >= M_CUTSCENE_DELAY;
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->draw_func = nullptr;
    obj->save_flags = true;

    m_BossTimer = 0;
}

REGISTER_OBJECT(O_COMBAT_END, M_Setup)
