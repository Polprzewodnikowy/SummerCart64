/**
 * SC64 Asset Streaming - Debug Diagnostics Implementation
 *
 * Only compiled when ASSET_DEBUG is defined.
 */

#ifdef ASSET_DEBUG

#include "asset_debug.h"
#include "asset_manager.h"
#include <string.h>

/* Global debug state */
asset_debug_state_t asset_debug = {0};

void asset_debug_set_log(asset_debug_log_fn fn) {
    asset_debug.log_fn = fn;
}

void asset_debug_on_evict(uint8_t *data, uint32_t size, uint32_t asset_id) {
    /* Poison the evicted slot with a recognizable pattern */
    uint32_t *words = (uint32_t *)data;
    uint32_t word_count = size / sizeof(uint32_t);
    for (uint32_t i = 0; i < word_count; i++) {
        words[i] = ASSET_DEBUG_POISON;
    }

    asset_debug.eviction_count++;

    ASSET_DBG_LOG("[ASSET_DBG] EVICT: asset %u from slot at %p (%u bytes poisoned)\n",
                  asset_id, (void *)data, size);
}

void asset_debug_on_pin(uint32_t asset_id, uint32_t new_pin_count) {
    /* Record pin event */
    if (asset_debug.pin_event_count < ASSET_DEBUG_MAX_EVENTS) {
        asset_debug_pin_event_t *ev = &asset_debug.pin_events[asset_debug.pin_event_count++];
        ev->asset_id = asset_id;
        ev->delta = +1;
        ev->pin_count = new_pin_count;
    }

    ASSET_DBG_LOG("[ASSET_DBG] PIN:   asset %u (pin_count now %u)\n",
                  asset_id, new_pin_count);
}

void asset_debug_on_unpin(uint32_t asset_id, uint32_t old_pin_count) {
    /* Warn on unbalanced unpin */
    if (old_pin_count == 0) {
        ASSET_DBG_LOG("[ASSET_DBG] WARNING: UNPIN of asset %u with pin_count=0! "
                      "This is an unbalanced unpin.\n", asset_id);
    }

    /* Record unpin event */
    if (asset_debug.pin_event_count < ASSET_DEBUG_MAX_EVENTS) {
        asset_debug_pin_event_t *ev = &asset_debug.pin_events[asset_debug.pin_event_count++];
        ev->asset_id = asset_id;
        ev->delta = -1;
        ev->pin_count = (old_pin_count > 0) ? old_pin_count - 1 : 0;
    }

    ASSET_DBG_LOG("[ASSET_DBG] UNPIN: asset %u (pin_count was %u, now %u)\n",
                  asset_id, old_pin_count,
                  (old_pin_count > 0) ? old_pin_count - 1 : 0);
}

void asset_debug_on_load(uint32_t asset_id, int cache_hit, int slot_idx) {
    if (cache_hit) {
        ASSET_DBG_LOG("[ASSET_DBG] LOAD:  asset %u -> HIT (slot %d)\n",
                      asset_id, slot_idx);
    } else {
        ASSET_DBG_LOG("[ASSET_DBG] LOAD:  asset %u -> MISS (loading into slot %d)\n",
                      asset_id, slot_idx);
    }
}

int asset_debug_is_poisoned(const void *data, uint32_t check_bytes) {
    const uint32_t *words = (const uint32_t *)data;
    uint32_t word_count = check_bytes / sizeof(uint32_t);
    if (word_count == 0) word_count = 1;

    int poisoned = 1;
    for (uint32_t i = 0; i < word_count; i++) {
        if (words[i] != ASSET_DEBUG_POISON) {
            poisoned = 0;
            break;
        }
    }

    if (poisoned) {
        asset_debug.poison_checks_failed++;
        ASSET_DBG_LOG("[ASSET_DBG] POISON CHECK FAILED: data at %p is poisoned "
                      "(use-after-evict detected!)\n", data);
    } else {
        asset_debug.poison_checks_passed++;
    }

    return poisoned;
}

int asset_debug_check_leaks(void *mgr_ptr) {
    asset_manager_t *mgr = (asset_manager_t *)mgr_ptr;
    int leaked = 0;

    ASSET_DBG_LOG("[ASSET_DBG] --- Pin Leak Check ---\n");

    for (uint32_t i = 0; i < mgr->cache.config.slot_count; i++) {
        cache_slot_t *slot = &mgr->cache.slots[i];
        if (slot->pin_count > 0 && slot->state == SLOT_READY) {
            ASSET_DBG_LOG("[ASSET_DBG] LEAK:  asset %u in slot %u still pinned "
                          "(pin_count=%u)\n",
                          slot->asset_id, i, slot->pin_count);
            leaked++;
        }
    }

    if (leaked == 0) {
        ASSET_DBG_LOG("[ASSET_DBG] No pin leaks detected.\n");
    } else {
        ASSET_DBG_LOG("[ASSET_DBG] WARNING: %d leaked pin(s) detected!\n", leaked);
    }

    return leaked;
}

void asset_debug_print_stats(void) {
    ASSET_DBG_LOG("[ASSET_DBG] --- Statistics ---\n");
    ASSET_DBG_LOG("[ASSET_DBG] Evictions:       %u\n", asset_debug.eviction_count);
    ASSET_DBG_LOG("[ASSET_DBG] Pin events:      %u\n", asset_debug.pin_event_count);
    ASSET_DBG_LOG("[ASSET_DBG] Poison passed:   %u\n", asset_debug.poison_checks_passed);
    ASSET_DBG_LOG("[ASSET_DBG] Poison FAILED:   %u\n", asset_debug.poison_checks_failed);
}

#endif /* ASSET_DEBUG */
