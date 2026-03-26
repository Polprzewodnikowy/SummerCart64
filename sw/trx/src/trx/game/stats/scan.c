#include <trx/core/benchmark.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/debug.h>
#include <trx/game/creature.h>
#include <trx/game/game_buf.h>
#include <trx/game/inject.h>
#include <trx/game/items.h>
#include <trx/game/items/carrier.h>
#include <trx/game/level.h>
#include <trx/game/objects.h>
#include <trx/game/objects/creatures/pod.h>
#include <trx/game/rooms.h>
#include <trx/game/savegame.h>
#include <trx/game/shell.h>
#include <trx/game/sound.h>
#include <trx/game/stats.h>
#include <trx/version.h>

#include <string.h>

static bool m_KillableItems[MAX_ITEMS] = {};

static void M_IncludeKillableItem(
    LEVEL_MAX_STATS *const stats, int16_t item_num)
{
    m_KillableItems[item_num] = true;
    const ITEM *const item = Item_Get(item_num);
    const bool is_ally = Creature_IsAlly(item);
    if (is_ally) {
        stats->max_kill_ally_count++;
    } else {
        stats->max_kill_non_ally_count++;
    }
    LOG_TRACE(
        "Killable item %d: object = %s", item_num,
        Object_GetName(item->object_id));
    if (Carrier_GetItemCount(item_num) > 0) {
        LOG_TRACE(
            "+%d pickups from carrier %d", Carrier_GetItemCount(item_num),
            item_num);
        stats->max_pickup_count += Carrier_GetItemCount(item_num);
    }
}

static uint32_t M_ReserveSecretConcreteBit(
    LEVEL_MAX_STATS *const stats, const OBJECT_ID object_id,
    const int32_t position)
{
    LOG_TRACE("Reserving bit %d for secret %d", position, object_id);
    if (position < 0 || position >= STATS_MAX_SECRETS) {
        LOG_ERROR(
            "Invalid secret bit %d (max: %d)", position, STATS_MAX_SECRETS);
        return 0;
    }
    const uint32_t secret_bit = 1 << position;
    if (!(stats->all_secrets_mask & secret_bit)) {
        stats->all_secrets_mask |= secret_bit;
        stats->max_secret_count++;
        if (object_id != NO_OBJECT) {
            stats->max_pickup_secret_count++;
        }
    }
    stats->secret_objects[position].assigned_object_id = object_id;
    stats->secret_objects[position].item_num = NO_ITEM;
    stats->secret_objects[position].taken = true;
    return secret_bit;
}

static uint32_t M_ReserveSecretUnusedBit(
    LEVEL_MAX_STATS *const stats, const OBJECT_ID object_id)
{
    // Find unused bit
    int32_t position = 0;
    uint32_t n = stats->all_secrets_mask;
    while ((n & 1) == 1) {
        n >>= 1;
        position++;
    }
    return M_ReserveSecretConcreteBit(stats, object_id, position);
}

static void M_CheckTriggers(
    LEVEL_MAX_STATS *const stats, const ROOM *const room,
    const int32_t room_num, const int32_t z_sector, const int32_t x_sector)
{
    if (z_sector == 0 || z_sector == room->size.z - 1) {
        if (x_sector == 0 || x_sector == room->size.x - 1) {
            return;
        }
    }
    const SECTOR *const sector = Room_GetUnitSector(room, x_sector, z_sector);

    if (sector->trigger == nullptr) {
        return;
    }

    const TRIGGER_CMD *cmd = sector->trigger->command;
    for (; cmd != nullptr; cmd = cmd->next_cmd) {
        if (cmd->type == TO_SECRET) {
            const uint16_t secret_num = (uint16_t)(intptr_t)cmd->parameter;
            M_ReserveSecretConcreteBit(stats, NO_OBJECT, secret_num);
        } else if (cmd->type == TO_OBJECT) {
            const int16_t item_num = (int16_t)(intptr_t)cmd->parameter;
            if (m_KillableItems[item_num]) {
                continue;
            }

            const ITEM *const item = Item_Get(item_num);
            switch (item->object_id) {
            case O_RAPTOR_EMITTER:
                for (int32_t i = 0; i < sector->trigger->timer; i++) {
                    M_IncludeKillableItem(stats, item_num);
                }
                break;

            case O_PIERRE:
                // Add Pierre pickup and kills if oneshot
                if (sector->trigger->one_shot) {
                    M_IncludeKillableItem(stats, item_num);
                }
                break;

            case O_PODS:
            case O_BIG_POD:
                // Check for only valid pods
                const OBJECT_ID object_id = Pod_GetBugObjectID(item);
                if (Object_Get(object_id)->loaded) {
                    M_IncludeKillableItem(stats, item_num);
                }
                break;

            case O_BARTOLI:
            case O_DRAGON_BACK:
            case O_DRAGON_FRONT:
                if (Object_Get(O_DRAGON_BACK)->loaded
                    && Object_Get(O_DRAGON_FRONT)->loaded) {
                    M_IncludeKillableItem(stats, item_num);
                    if (Object_Get(O_PUZZLE_OPTION_2)->loaded
                        || Object_Get(O_PUZZLE_ITEM_2)->loaded) {
                        LOG_TRACE("+1 pickup from dragon");
                        stats->max_pickup_count++;
                    }
                }
                break;

            case O_EEL:
            case O_BIG_EEL:
                break;

            case O_SCION_ITEM_3:
                M_IncludeKillableItem(stats, item_num);
                break;

            default:
                // Add killable if object triggered
                if (Creature_IsHostile(item) || Creature_IsAlly(item)
                    || Creature_IsAllyTargetingEnemy(item)) {
                    M_IncludeKillableItem(stats, item_num);
                }
                break;
            }
        }
    }
}

static void M_TraverseFloor(LEVEL_MAX_STATS *const stats)
{
    for (int32_t i = 0; i < Room_GetCount(); i++) {
        const ROOM *const room = Room_Get(i);
        for (int32_t z_sector = 0; z_sector < room->size.z; z_sector++) {
            for (int32_t x_sector = 0; x_sector < room->size.x; x_sector++) {
                M_CheckTriggers(stats, room, i, z_sector, x_sector);
            }
        }
    }
}

static void M_CalculateStats(LEVEL_MAX_STATS *const stats)
{
    memset(stats, 0, sizeof(*stats));
    for (int32_t i = 0; i < STATS_MAX_SECRETS; i++) {
        stats->secret_item_masks[i].item_num = NO_ITEM;
        stats->secret_item_masks[i].secret_mask = 0;

        stats->secret_objects[i].assigned_object_id = NO_OBJECT;
        stats->secret_objects[i].item_num = NO_ITEM;
        stats->secret_objects[i].taken = false;
    }

    memset(&m_KillableItems, 0, sizeof(m_KillableItems));

    for (int32_t i = 0; i < Item_GetTotalCount(); i++) {
        const ITEM *const item = Item_Get(i);
        if (Object_IsType(item->object_id, g_PickupObjects)
            && !Carrier_IsItemCarried(i)) {
            LOG_TRACE(
                "+1 pickup from pickup item %d in room %d", i, item->room_num);
            stats->max_pickup_count++;
        }
    }

    // Check triggers for special pickups / killables
    M_TraverseFloor(stats);

    for (int32_t i = 0; i < Item_GetTotalCount(); i++) {
        ITEM *const item = Item_Get(i);
        if (item->object_id < O_FIRST || item->object_id >= O_NUMBER_OF) {
            LOG_ERROR("Bad Object number (%d) on Item %d", item->object_id, i);
            continue;
        }

        if (item->object_id == O_COMBAT_END) {
            M_IncludeKillableItem(stats, i);
        }

        if (Object_IsType(item->object_id, g_SecretObjects)) {
            int32_t position = -1;
            for (int32_t j = 0; j < STATS_MAX_SECRETS; j++) {
                if (!stats->secret_objects[j].taken) {
                    position = j;
                    break;
                }
            }
            if (position == -1) {
                LOG_ERROR("Too many secrets, max %d", STATS_MAX_SECRETS);
                break;
            }
            stats->secret_objects[position].assigned_object_id =
                item->object_id;
            stats->secret_objects[position].item_num = i;
            stats->secret_objects[position].taken = true;
            position++;
        }
    }

    // Sorts secret objects by their type so that the dragons line up nicely
    for (int32_t i = 0; i < STATS_MAX_SECRETS; i++) {
        if (stats->secret_objects[i].assigned_object_id == NO_OBJECT) {
            continue;
        }
        for (int32_t j = i + 1; j < STATS_MAX_SECRETS; j++) {
            if (stats->secret_objects[j].assigned_object_id == NO_OBJECT) {
                continue;
            }
            if (stats->secret_objects[i].assigned_object_id
                > stats->secret_objects[j].assigned_object_id) {
                SWAP(stats->secret_objects[i], stats->secret_objects[j]);
            }
        }
    }

    // Assign secret items their bits so they know which secret to set on
    // pickup. NOTE: Do not persist runtime pickup state here. This scan runs
    // at game launch to compute max stats; gameplay level loads restore secret
    // masks using cached info in LEVEL_MAX_STATS.
    for (int32_t i = 0; i < STATS_MAX_SECRETS; i++) {
        const int32_t item_num = stats->secret_objects[i].item_num;
        if (item_num == NO_ITEM) {
            continue;
        }

        const uint32_t secret_mask = M_ReserveSecretUnusedBit(
            stats, stats->secret_objects[i].assigned_object_id);

        for (int32_t j = 0; j < STATS_MAX_SECRETS; j++) {
            if (stats->secret_item_masks[j].item_num == NO_ITEM) {
                stats->secret_item_masks[j].item_num = item_num;
                stats->secret_item_masks[j].secret_mask = secret_mask;
                break;
            }
        }
    }
}

void Stats_ScanLevel(const GF_LEVEL *const level)
{
    ASSERT(level != nullptr);
    BENCHMARK benchmark = Benchmark_Start();
    LEVEL_MAX_STATS *const max_stats = Stats_GetLevelMaxStats(level);
    M_CalculateStats(max_stats);
    max_stats->max_pickup_count += GF_GetSecretRewardCount(level);
    max_stats->max_pickup_count -= level->unobtainable.pickups;
    max_stats->max_secret_count -= level->unobtainable.secrets;
    max_stats->max_kill_ally_count -= level->unobtainable.ally_kills;
    max_stats->max_kill_non_ally_count -= level->unobtainable.kills;
    max_stats->max_kill_count =
        max_stats->max_kill_non_ally_count + max_stats->max_kill_ally_count;
    Benchmark_End(&benchmark, nullptr);
}
