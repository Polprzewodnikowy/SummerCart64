/**
 * LRU Asset Cache Manager for SC64
 */

#include "asset_cache.h"
#include <string.h>

void cache_init(asset_cache_t *cache, cache_slot_t *slots,
                uint8_t *base_address, uint32_t total_size, uint32_t slot_size) {
    cache->config.base_address = base_address;
    cache->config.total_size = total_size;
    cache->config.slot_size = slot_size;
    cache->config.slot_count = total_size / slot_size;
    cache->slots = slots;
    cache->lru_clock = 0;
    cache->hit_count = 0;
    cache->miss_count = 0;

    for (uint32_t i = 0; i < cache->config.slot_count; i++) {
        slots[i].asset_id = 0;
        slots[i].state = SLOT_EMPTY;
        slots[i].lru_counter = 0;
        slots[i].data = base_address + (i * slot_size);
        slots[i].data_size = 0;
    }
}

void *cache_lookup(asset_cache_t *cache, uint32_t asset_id) {
    for (uint32_t i = 0; i < cache->config.slot_count; i++) {
        if (cache->slots[i].asset_id == asset_id &&
            cache->slots[i].state == SLOT_READY) {
            /* Cache hit - update LRU */
            cache->slots[i].lru_counter = ++cache->lru_clock;
            cache->hit_count++;
            return cache->slots[i].data;
        }
    }
    cache->miss_count++;
    return NULL;
}

int cache_allocate(asset_cache_t *cache, uint32_t asset_id, bool *evicted) {
    *evicted = false;

    /* First: check if already cached */
    for (uint32_t i = 0; i < cache->config.slot_count; i++) {
        if (cache->slots[i].asset_id == asset_id) {
            cache->slots[i].lru_counter = ++cache->lru_clock;
            return (int)i;
        }
    }

    /* Second: find an empty slot */
    for (uint32_t i = 0; i < cache->config.slot_count; i++) {
        if (cache->slots[i].state == SLOT_EMPTY) {
            cache->slots[i].asset_id = asset_id;
            cache->slots[i].state = SLOT_LOADING;
            cache->slots[i].lru_counter = ++cache->lru_clock;
            *evicted = true;  /* Caller needs to load data */
            return (int)i;
        }
    }

    /* Third: evict LRU slot (skip LOADING slots) */
    uint32_t min_lru = 0xFFFFFFFF;
    int victim = -1;

    for (uint32_t i = 0; i < cache->config.slot_count; i++) {
        if (cache->slots[i].state == SLOT_LOADING) continue;
        if (cache->slots[i].lru_counter < min_lru) {
            min_lru = cache->slots[i].lru_counter;
            victim = (int)i;
        }
    }

    if (victim < 0) return -1;  /* All slots busy loading */

    cache->slots[victim].asset_id = asset_id;
    cache->slots[victim].state = SLOT_LOADING;
    cache->slots[victim].lru_counter = ++cache->lru_clock;
    cache->slots[victim].data_size = 0;
    *evicted = true;

    return victim;
}

void cache_slot_ready(asset_cache_t *cache, int slot_idx, uint32_t data_size) {
    if (slot_idx < 0 || (uint32_t)slot_idx >= cache->config.slot_count) return;
    cache->slots[slot_idx].state = SLOT_READY;
    cache->slots[slot_idx].data_size = data_size;
}

void *cache_slot_data(asset_cache_t *cache, int slot_idx) {
    if (slot_idx < 0 || (uint32_t)slot_idx >= cache->config.slot_count) return NULL;
    return cache->slots[slot_idx].data;
}

void cache_flush(asset_cache_t *cache) {
    for (uint32_t i = 0; i < cache->config.slot_count; i++) {
        cache->slots[i].asset_id = 0;
        cache->slots[i].state = SLOT_EMPTY;
        cache->slots[i].lru_counter = 0;
        cache->slots[i].data_size = 0;
    }
    cache->lru_clock = 0;
}

int cache_hit_rate(asset_cache_t *cache) {
    uint32_t total = cache->hit_count + cache->miss_count;
    if (total == 0) return 0;
    return (int)((cache->hit_count * 100) / total);
}
