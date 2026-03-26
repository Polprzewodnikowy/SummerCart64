#include <trx/core/log.h>
#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/game.h>
#include <trx/game/game_flow.h>
#include <trx/game/objects/general/pickup.h>
#include <trx/game/savegame.h>
#include <trx/game/stats.h>

bool Stats_IsSecretValid(const int16_t secret_idx)
{
    const GF_LEVEL *const level = Game_GetCurrentLevel();
    const LEVEL_MAX_STATS *const max_stats = Stats_GetLevelMaxStats(level);
    if (secret_idx < 0 || secret_idx >= STATS_MAX_SECRETS) {
        return false;
    }
    const uint32_t secret_mask = 1 << secret_idx;
    return (secret_mask & max_stats->all_secrets_mask) != 0;
}

bool Stats_HasSecret(const int16_t secret_idx)
{
    const GF_LEVEL *const level = Game_GetCurrentLevel();
    const LEVEL_MAX_STATS *const max_stats = Stats_GetLevelMaxStats(level);
    RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);
    if (secret_idx < 0 || secret_idx >= STATS_MAX_SECRETS) {
        return false;
    }
    const uint32_t secret_mask = 1 << secret_idx;
    if ((secret_mask & max_stats->all_secrets_mask) == 0) {
        return false;
    }
    return (resume->stats.secret_flags & secret_mask) != 0;
}

bool Stats_RemoveSecret(const int16_t secret_idx)
{
    const GF_LEVEL *const level = Game_GetCurrentLevel();
    const LEVEL_MAX_STATS *const max_stats = Stats_GetLevelMaxStats(level);
    RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);
    if (secret_idx < 0 || secret_idx >= STATS_MAX_SECRETS) {
        return false;
    }
    const uint32_t secret_mask = 1 << secret_idx;
    if ((secret_mask & max_stats->all_secrets_mask) == 0) {
        return false;
    }
    if (!(resume->stats.secret_flags & secret_mask)) {
        return false;
    }
    LOG_INFO("Removing secret %d", secret_idx);
    resume->stats.secret_flags &= ~secret_mask;
    resume->stats.secret_count--;
    return true;
}

bool Stats_AddSecret(const int16_t secret_idx)
{
    const GF_LEVEL *const level = Game_GetCurrentLevel();
    const LEVEL_MAX_STATS *const max_stats = Stats_GetLevelMaxStats(level);
    RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);
    if (secret_idx < 0 || secret_idx >= STATS_MAX_SECRETS) {
        return false;
    }
    const uint32_t secret_mask = 1 << secret_idx;
    if ((secret_mask & max_stats->all_secrets_mask) == 0) {
        return false;
    }
    if (resume->stats.secret_flags & secret_mask) {
        return false;
    }
    LOG_INFO("Adding secret %d", secret_idx);
    resume->stats.secret_flags |= secret_mask;
    resume->stats.secret_count++;
    return true;
}

void Stats_UpdateSecrets(LEVEL_STATS *const stats)
{
    stats->secret_count = 0;
    for (int32_t i = 0; i < STATS_MAX_SECRETS; i++) {
        stats->secret_count += (stats->secret_flags & (1 << i)) ? 1 : 0;
    }
}

void Stats_MarkSecretCollected(const ITEM *const item)
{
    RESUME_INFO *const resume = Savegame_GetCurrentInfo(Game_GetCurrentLevel());
    resume->stats.secret_flags |= Pickup_GetSecretMask(item);
    Stats_UpdateSecrets(&resume->stats);
}

bool Stats_CheckAllLevelSecretsPickedUp(void)
{
    const GF_LEVEL *const level = Game_GetCurrentLevel();
    const LEVEL_MAX_STATS *const max_stats = Stats_GetLevelMaxStats(level);
    const RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);
    int32_t flags = resume->stats.secret_flags;
    size_t count = 0;
    while (flags != 0) {
        count += flags & 1;
        flags >>= 1;
    }
    return count >= max_stats->max_pickup_secret_count;
}

bool Stats_CheckAllSecretsCollected(void)
{
    const FINAL_STATS final_stats = Stats_ComputeFinalStats(false);
    return final_stats.stats.secret_count
        >= final_stats.max_stats.max_secret_count;
}

void Stats_AddMedipacksUsed(const double medipack_value)
{
    RESUME_INFO *const resume = Savegame_GetCurrentInfo(Game_GetCurrentLevel());
    resume->stats.medipacks_used += medipack_value;
}

void Stats_AddDeath(void)
{
    const GF_LEVEL *const current_level = Game_GetCurrentLevel();
    RESUME_INFO *const resume = Savegame_GetCurrentInfo(current_level);
    resume->stats.death_count++;
    const SAVEGAME_SLOT_REF save_slot = Savegame_GetBoundSlot();
    if (Savegame_IsValidSlotRef(save_slot)) {
        Savegame_UpdateDeathCounters(save_slot, resume->stats.death_count);
    }
}

void Stats_UpdateTimer(void)
{
    const GF_LEVEL *const level = Game_GetCurrentLevel();
    if (level != nullptr) {
        RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);
        resume->stats.timer++;
    }
}

void Stats_AddKill(void)
{
    const GF_LEVEL *const level = Game_GetCurrentLevel();
    if (level != nullptr) {
        RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);
        resume->stats.kill_count++;
    }
}

void Stats_AddPickup(void)
{
    const GF_LEVEL *const level = Game_GetCurrentLevel();
    if (level != nullptr) {
        RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);
        resume->stats.pickup_count++;
    }
}

void Stats_AddAmmoHits(void)
{
    const GF_LEVEL *const level = Game_GetCurrentLevel();
    if (level != nullptr) {
        RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);
        resume->stats.ammo_hits++;
    }
}

void Stats_AddAmmoUsed(void)
{
    const GF_LEVEL *const level = Game_GetCurrentLevel();
    if (level != nullptr) {
        RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);
        resume->stats.ammo_used++;
    }
}

void Stats_AddDistanceTravelled(const XYZ_32 pos, const XYZ_32 last_pos)
{
    const GF_LEVEL *const level = Game_GetCurrentLevel();
    if (level != nullptr) {
        RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);
        resume->stats.distance_travelled += XYZ_32_GetDistance(pos, last_pos);
    }
}

FINAL_STATS Stats_ComputeFinalStats(const bool include_bonus_levels)
{
    FINAL_STATS result = {};
    const GF_LEVEL_TABLE *const level_table = GF_GetLevelTable(GFLT_MAIN);
    for (int32_t i = 0; i < level_table->count; i++) {
        const GF_LEVEL *const level = &level_table->levels[i];
        if (!(level->type == GFL_NORMAL
              || (level->type == GFL_BONUS && include_bonus_levels))) {
            continue;
        }

        const RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);
        if (resume != nullptr) {
#define L_ADD(prop) result.stats.prop += resume->stats.prop;
            L_ADD(kill_count);
            L_ADD(pickup_count);
            L_ADD(secret_count);
            L_ADD(timer);
            L_ADD(ammo_hits);
            L_ADD(ammo_used);
            L_ADD(medipacks_used);
            L_ADD(distance_travelled);
            L_ADD(death_count);
#undef L_ADD
        }

        const LEVEL_MAX_STATS *const max_stats = Stats_GetLevelMaxStats(level);
        if (max_stats != nullptr) {
#define L_ADD(prop) result.max_stats.prop += max_stats->prop;
            L_ADD(max_kill_count);
            L_ADD(max_kill_ally_count);
            L_ADD(max_kill_non_ally_count);
            L_ADD(max_pickup_count);
            L_ADD(max_secret_count);
            L_ADD(max_pickup_secret_count);
#undef L_ADD
        }
    }

    return result;
}

OBJECT_ID Stats_GetSecretObject(const int32_t secret_idx)
{
    const GF_LEVEL *const level = Game_GetCurrentLevel();
    if (level == nullptr) {
        return NO_OBJECT;
    }
    const LEVEL_MAX_STATS *const stats = Stats_GetLevelMaxStats(level);
    ASSERT(stats != nullptr);
    ASSERT(secret_idx >= 0 && secret_idx < STATS_MAX_SECRETS);
    return stats->secret_objects[secret_idx].assigned_object_id;
}

uint32_t Stats_GetSecretMaskForItem(
    const GF_LEVEL *const level, const int16_t item_num)
{
    if (level == nullptr) {
        return 0;
    }

    const LEVEL_MAX_STATS *const max_stats = Stats_GetLevelMaxStats(level);
    if (max_stats == nullptr) {
        return 0;
    }

    for (int32_t i = 0; i < STATS_MAX_SECRETS; i++) {
        if (max_stats->secret_item_masks[i].item_num == item_num) {
            return max_stats->secret_item_masks[i].secret_mask;
        }
    }

    return 0;
}

void Stats_MarkAlliesHostile(void)
{
    const GF_LEVEL *const level = Game_GetCurrentLevel();
    RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);
    resume->hurt_allies = true;
}
