/**
 * Host-side validation tests for asset_cache and lz4_decode.
 *
 * Tests the cache logic, LRU eviction, pinning, and LZ4 decompression
 * in isolation, without any SC64 hardware dependency.
 *
 * Build and run:
 *   cc -o test_cache test_cache.c ../src/asset_cache.c ../src/lz4_decode.c \
 *      -I../src -Wall -Wextra && ./test_cache
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "asset_cache.h"
#include "lz4_decode.h"

/* ---------- Test infrastructure ---------- */

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { printf("  %-50s ", #name); } while(0)

#define PASS() \
    do { printf("[PASS]\n"); tests_passed++; } while(0)

#define FAIL(msg) \
    do { printf("[FAIL] %s\n", msg); tests_failed++; } while(0)

#define ASSERT(cond, msg) \
    do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* ---------- Simulated SDRAM ---------- */

#define FAKE_SDRAM_SIZE     (1024 * 1024)  /* 1 MB */
#define SLOT_SIZE           (4096)          /* 4 KB per slot */
#define NUM_SLOTS           (4)            /* Only 4 slots to force eviction */

static uint8_t fake_sdram[FAKE_SDRAM_SIZE];
static cache_slot_t slot_storage[NUM_SLOTS];
static asset_cache_t cache;

static void setup_cache(void) {
    memset(fake_sdram, 0, sizeof(fake_sdram));
    cache_init(&cache, slot_storage, fake_sdram,
               NUM_SLOTS * SLOT_SIZE, SLOT_SIZE);
}

/* ---------- Test: Basic init ---------- */

static void test_cache_init(void) {
    TEST(cache_init_sets_up_slots);
    setup_cache();
    ASSERT(cache.config.slot_count == NUM_SLOTS, "wrong slot count");
    ASSERT(cache.config.slot_size == SLOT_SIZE, "wrong slot size");
    ASSERT(cache.lru_clock == 0, "clock not zero");
    for (uint32_t i = 0; i < NUM_SLOTS; i++) {
        ASSERT(slot_storage[i].state == SLOT_EMPTY, "slot not empty");
        ASSERT(slot_storage[i].data == fake_sdram + (i * SLOT_SIZE), "wrong data ptr");
        ASSERT(slot_storage[i].pin_count == 0, "pin not zero");
    }
    PASS();
}

/* ---------- Test: Cold load (cache miss -> allocate) ---------- */

static void test_cold_load(void) {
    TEST(cold_load_allocates_empty_slot);
    setup_cache();

    bool evicted;
    int slot = cache_allocate(&cache, 42, &evicted);
    ASSERT(slot >= 0, "allocation failed");
    ASSERT(evicted == true, "should signal need-to-load");
    ASSERT(cache.slots[slot].asset_id == 42, "wrong asset id");
    ASSERT(cache.slots[slot].state == SLOT_LOADING, "not in loading state");

    /* Simulate loading data */
    memset(cache.slots[slot].data, 0xAA, 100);
    cache_slot_ready(&cache, slot, 100);
    ASSERT(cache.slots[slot].state == SLOT_READY, "not ready after mark");
    ASSERT(cache.slots[slot].data_size == 100, "wrong data size");
    PASS();
}

/* ---------- Test: Hot load (cache hit) ---------- */

static void test_hot_load(void) {
    TEST(hot_load_returns_cached_data);
    setup_cache();

    /* Load asset 42 */
    bool evicted;
    int slot = cache_allocate(&cache, 42, &evicted);
    memset(cache.slots[slot].data, 0xBB, 200);
    cache_slot_ready(&cache, slot, 200);

    /* Lookup should hit */
    void *data = cache_lookup(&cache, 42);
    ASSERT(data != NULL, "lookup returned NULL");
    ASSERT(data == cache.slots[slot].data, "wrong pointer");
    ASSERT(((uint8_t *)data)[0] == 0xBB, "data corrupted");
    ASSERT(cache.hit_count == 1, "hit not counted");
    PASS();
}

/* ---------- Test: Cache miss counted ---------- */

static void test_cache_miss(void) {
    TEST(cache_miss_returns_null_and_counts);
    setup_cache();

    void *data = cache_lookup(&cache, 999);
    ASSERT(data == NULL, "should return NULL for missing asset");
    ASSERT(cache.miss_count == 1, "miss not counted");
    PASS();
}

/* ---------- Test: LRU eviction ---------- */

static void test_lru_eviction(void) {
    TEST(lru_evicts_oldest_when_full);
    setup_cache();

    /* Fill all 4 slots with assets 1,2,3,4 */
    bool evicted;
    for (uint32_t id = 1; id <= NUM_SLOTS; id++) {
        int s = cache_allocate(&cache, id, &evicted);
        ASSERT(s >= 0, "alloc failed");
        cache_slot_ready(&cache, s, 100);
    }

    /* Touch asset 1 to make it recently used */
    cache_lookup(&cache, 1);

    /* Allocate asset 5 - should evict asset 2 (oldest untouched) */
    int s5 = cache_allocate(&cache, 5, &evicted);
    ASSERT(s5 >= 0, "alloc failed for 5");
    ASSERT(evicted == true, "should have evicted");

    /* Asset 2 should be gone */
    /* Asset 1 should still be findable (it was touched) */
    cache_slot_ready(&cache, s5, 100);

    /* Verify asset 1 survives */
    void *d1 = cache_lookup(&cache, 1);
    ASSERT(d1 != NULL, "asset 1 was wrongly evicted");

    PASS();
}

/* ---------- Test: Pinned asset survives eviction ---------- */

static void test_pin_prevents_eviction(void) {
    TEST(pinned_asset_survives_eviction_pressure);
    setup_cache();

    /* Fill slots with assets 1,2,3,4 */
    bool evicted;
    for (uint32_t id = 1; id <= NUM_SLOTS; id++) {
        int s = cache_allocate(&cache, id, &evicted);
        cache_slot_ready(&cache, s, 100);
    }

    /* Pin asset 1 (the oldest) */
    bool pinned = cache_pin(&cache, 1);
    ASSERT(pinned == true, "pin failed");
    ASSERT(cache_is_pinned(&cache, 1), "not reported as pinned");

    /* Load 4 more assets to force heavy eviction pressure */
    for (uint32_t id = 10; id <= 13; id++) {
        int s = cache_allocate(&cache, id, &evicted);
        if (s >= 0) cache_slot_ready(&cache, s, 100);
    }

    /* Asset 1 must survive because it's pinned */
    void *d1 = cache_lookup(&cache, 1);
    ASSERT(d1 != NULL, "pinned asset was evicted!");

    /* Unpin and verify it can now be evicted */
    cache_unpin(&cache, 1);
    ASSERT(!cache_is_pinned(&cache, 1), "still pinned after unpin");
    PASS();
}

/* ---------- Test: Pin reference counting ---------- */

static void test_pin_refcount(void) {
    TEST(pin_is_reference_counted);
    setup_cache();

    bool evicted;
    int s = cache_allocate(&cache, 1, &evicted);
    cache_slot_ready(&cache, s, 100);

    /* Pin twice */
    cache_pin(&cache, 1);
    cache_pin(&cache, 1);
    ASSERT(cache.slots[s].pin_count == 2, "pin count should be 2");

    /* One unpin - still pinned */
    cache_unpin(&cache, 1);
    ASSERT(cache_is_pinned(&cache, 1), "should still be pinned");

    /* Second unpin - now unpinned */
    cache_unpin(&cache, 1);
    ASSERT(!cache_is_pinned(&cache, 1), "should be unpinned now");
    PASS();
}

/* ---------- Test: Flush preserves pinned ---------- */

static void test_flush_preserves_pinned(void) {
    TEST(flush_preserves_pinned_slots);
    setup_cache();

    bool evicted;
    for (uint32_t id = 1; id <= NUM_SLOTS; id++) {
        int s = cache_allocate(&cache, id, &evicted);
        cache_slot_ready(&cache, s, 100);
    }

    /* Pin asset 2 */
    cache_pin(&cache, 2);

    /* Flush */
    cache_flush(&cache);

    /* Asset 2 should survive, others should be gone */
    ASSERT(cache_lookup(&cache, 2) != NULL, "pinned asset flushed");
    ASSERT(cache_lookup(&cache, 1) == NULL, "unpinned asset survived flush");
    ASSERT(cache_lookup(&cache, 3) == NULL, "unpinned asset survived flush");

    PASS();
}

/* ---------- Test: flush_all clears everything ---------- */

static void test_flush_all(void) {
    TEST(flush_all_clears_pinned_slots_too);
    setup_cache();

    bool evicted;
    int s = cache_allocate(&cache, 1, &evicted);
    cache_slot_ready(&cache, s, 100);
    cache_pin(&cache, 1);

    cache_flush_all(&cache);

    ASSERT(cache_lookup(&cache, 1) == NULL, "pinned asset survived flush_all");
    ASSERT(cache.pin_count == 0, "pin count not reset");
    PASS();
}

/* ---------- Test: Hit rate calculation ---------- */

static void test_hit_rate(void) {
    TEST(hit_rate_calculation);
    setup_cache();

    bool evicted;
    int s = cache_allocate(&cache, 1, &evicted);
    cache_slot_ready(&cache, s, 100);

    /* 1 miss (lookup before load) + 3 hits */
    cache_lookup(&cache, 999);  /* miss */
    cache_lookup(&cache, 1);    /* hit */
    cache_lookup(&cache, 1);    /* hit */
    cache_lookup(&cache, 1);    /* hit */

    int rate = cache_hit_rate(&cache);
    ASSERT(rate == 75, "expected 75% hit rate");
    PASS();
}

/* ---------- Test: LZ4 decompression correctness ---------- */

/*
 * Pre-compressed LZ4 block data for testing.
 * This is the LZ4 block encoding of "AAAAAAAAAAAAAAAA" (16 'A's).
 *
 * Token: 0x10 (literal_len=1, match_len=0+4=4... actually let me
 * hand-craft a simple test.)
 *
 * We'll test with a trivially compressible pattern and a known encoding.
 */

static void test_lz4_literals_only(void) {
    TEST(lz4_decodes_literal_only_block);

    /* LZ4 block with just literals, no matches.
     * Token 0xF0 = literal_len 15, then 0x01 = +1 = 16 total literals.
     * Followed by 16 bytes of data. No match (end of block). */
    uint8_t compressed[] = {
        0xF0,                           /* token: lit_len=15, match_len=0 */
        0x01,                           /* additional lit_len: 15+1 = 16 */
        'H','e','l','l','o',' ',        /* 16 literal bytes */
        'W','o','r','l','d','!',
        'A','B','C','D'
    };
    uint8_t output[64];
    memset(output, 0, sizeof(output));

    int decoded = lz4_decode_block(compressed, output,
                                    sizeof(compressed), sizeof(output));
    ASSERT(decoded == 16, "wrong decoded size");
    ASSERT(memcmp(output, "Hello World!ABCD", 16) == 0, "wrong decoded content");
    PASS();
}

static void test_lz4_with_match(void) {
    TEST(lz4_decodes_block_with_match);

    /* LZ4 block: 4 literal bytes "ABCD", then match offset=4, length=4+4=8.
     * Result should be "ABCDABCDABCD" (12 bytes).
     *
     * Token 0x44: lit_len=4, match_len=4 (so actual match_len = 4+4 = 8)
     * Literals: A B C D
     * Offset: 4 (little-endian: 04 00)
     */
    uint8_t compressed[] = {
        0x44,                   /* token: lit=4, match=4 (+4=8) */
        'A', 'B', 'C', 'D',    /* 4 literal bytes */
        0x04, 0x00,             /* match offset = 4 (little-endian) */
    };
    uint8_t output[64];
    memset(output, 0, sizeof(output));

    int decoded = lz4_decode_block(compressed, output,
                                    sizeof(compressed), sizeof(output));
    ASSERT(decoded == 12, "wrong decoded size");
    ASSERT(memcmp(output, "ABCDABCDABCD", 12) == 0, "wrong match expansion");
    PASS();
}

static void test_lz4_short_offset_overlap(void) {
    TEST(lz4_handles_overlapping_match_offset_1);

    /* Match with offset=1 should repeat the last byte.
     * Token 0x14: lit=1, match=4 (+4=8)
     * Literal: 'X'
     * Offset: 1
     * Result: "XXXXXXXXX" (1 literal + 8 match copies = 9 bytes) */
    uint8_t compressed[] = {
        0x14,           /* token: lit=1, match=4 (+4=8) */
        'X',            /* 1 literal */
        0x01, 0x00,     /* offset = 1 */
    };
    uint8_t output[64];
    memset(output, 0, sizeof(output));

    int decoded = lz4_decode_block(compressed, output,
                                    sizeof(compressed), sizeof(output));
    ASSERT(decoded == 9, "wrong decoded size for repeat");
    for (int i = 0; i < 9; i++) {
        ASSERT(output[i] == 'X', "wrong byte in repeat pattern");
    }
    PASS();
}

/* ---------- Test: LZ4 error handling ---------- */

static void test_lz4_truncated_input(void) {
    TEST(lz4_rejects_truncated_input);

    /* Token says 15 literals but only 2 follow */
    uint8_t compressed[] = { 0xF0, 0x00, 'A', 'B' };
    uint8_t output[64];

    int decoded = lz4_decode_block(compressed, output,
                                    sizeof(compressed), sizeof(output));
    ASSERT(decoded == -1, "should reject truncated literal");
    PASS();
}

static void test_lz4_zero_offset(void) {
    TEST(lz4_rejects_zero_match_offset);

    /* Token with match but offset=0 is invalid */
    uint8_t compressed[] = {
        0x14,           /* lit=1, match=4 */
        'A',            /* literal */
        0x00, 0x00,     /* offset = 0 (invalid!) */
    };
    uint8_t output[64];

    int decoded = lz4_decode_block(compressed, output,
                                    sizeof(compressed), sizeof(output));
    ASSERT(decoded == -1, "should reject zero offset");
    PASS();
}

static void test_lz4_output_overflow(void) {
    TEST(lz4_rejects_output_buffer_overflow);

    /* Try to decompress into a tiny buffer */
    uint8_t compressed[] = {
        0xF0, 0x00,                     /* 15 literal bytes */
        1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
    };
    uint8_t output[4];  /* Too small! */

    int decoded = lz4_decode_block(compressed, output,
                                    sizeof(compressed), sizeof(output));
    ASSERT(decoded == -1, "should reject when output too small");
    PASS();
}

/* ---------- Test: Pointer stability contract ---------- */

static void test_pointer_stability_without_pin(void) {
    TEST(pointer_can_go_stale_without_pin);
    setup_cache();

    /* Load asset 1 and get pointer */
    bool evicted;
    int s1 = cache_allocate(&cache, 1, &evicted);
    memset(cache.slots[s1].data, 0xAA, 100);
    cache_slot_ready(&cache, s1, 100);
    void *ptr1 = cache_lookup(&cache, 1);
    ASSERT(ptr1 != NULL, "initial lookup failed");

    /* Fill remaining slots */
    for (uint32_t id = 2; id <= NUM_SLOTS; id++) {
        int s = cache_allocate(&cache, id, &evicted);
        cache_slot_ready(&cache, s, 100);
    }

    /* Load one more - asset 1 should be evicted (oldest) */
    int s_new = cache_allocate(&cache, 99, &evicted);
    cache_slot_ready(&cache, s_new, 100);

    /* ptr1 is now STALE - the slot was reused */
    void *ptr1_again = cache_lookup(&cache, 1);
    ASSERT(ptr1_again == NULL, "asset 1 should have been evicted");
    PASS();
}

static void test_pointer_stability_with_pin(void) {
    TEST(pinned_pointer_stays_valid_under_pressure);
    setup_cache();

    /* Load and PIN asset 1 */
    bool evicted;
    int s1 = cache_allocate(&cache, 1, &evicted);
    memset(cache.slots[s1].data, 0xAA, 100);
    cache_slot_ready(&cache, s1, 100);
    void *ptr1 = cache_lookup(&cache, 1);
    cache_pin(&cache, 1);

    /* Fill remaining slots and cause heavy eviction */
    for (uint32_t id = 2; id <= 20; id++) {
        int s = cache_allocate(&cache, id, &evicted);
        if (s >= 0) cache_slot_ready(&cache, s, 100);
    }

    /* ptr1 should still be valid because asset 1 is pinned */
    void *ptr1_again = cache_lookup(&cache, 1);
    ASSERT(ptr1_again == ptr1, "pinned pointer changed!");
    ASSERT(((uint8_t *)ptr1_again)[0] == 0xAA, "pinned data corrupted");

    cache_unpin(&cache, 1);
    PASS();
}

/* ---------- Test: All slots pinned -> allocation fails ---------- */

static void test_all_pinned_allocation_fails(void) {
    TEST(allocation_fails_when_all_slots_pinned);
    setup_cache();

    /* Fill and pin all slots */
    bool evicted;
    for (uint32_t id = 1; id <= NUM_SLOTS; id++) {
        int s = cache_allocate(&cache, id, &evicted);
        cache_slot_ready(&cache, s, 100);
        cache_pin(&cache, id);
    }

    /* Try to allocate a new asset - should fail */
    int s = cache_allocate(&cache, 99, &evicted);
    ASSERT(s == -1, "should fail when all slots pinned");

    /* Cleanup */
    for (uint32_t id = 1; id <= NUM_SLOTS; id++) {
        cache_unpin(&cache, id);
    }
    PASS();
}

/* ---------- Main ---------- */

int main(void) {
    printf("\n=== Asset Cache Tests ===\n\n");

    test_cache_init();
    test_cold_load();
    test_hot_load();
    test_cache_miss();
    test_lru_eviction();
    test_pin_prevents_eviction();
    test_pin_refcount();
    test_flush_preserves_pinned();
    test_flush_all();
    test_hit_rate();
    test_pointer_stability_without_pin();
    test_pointer_stability_with_pin();
    test_all_pinned_allocation_fails();

    printf("\n=== LZ4 Decoder Tests ===\n\n");

    test_lz4_literals_only();
    test_lz4_with_match();
    test_lz4_short_offset_overlap();
    test_lz4_truncated_input();
    test_lz4_zero_offset();
    test_lz4_output_overflow();

    printf("\n=== Results: %d passed, %d failed ===\n\n",
           tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
