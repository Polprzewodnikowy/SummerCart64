/**
 * SC64 Asset Streaming - Debug Diagnostics
 *
 * Compile with -DASSET_DEBUG to enable runtime diagnostics:
 *   - Eviction poisoning (use-after-evict detection)
 *   - Pin/unpin tracking with leak detection
 *   - Unbalanced unpin warnings
 *   - Event logging via user-provided callback
 *
 * All debug code compiles to nothing when ASSET_DEBUG is not defined.
 *
 * Usage:
 *
 *   #define ASSET_DEBUG
 *   #include "asset_debug.h"
 *
 *   // Set your log callback before asset_init:
 *   asset_debug_set_log(my_printf_func);
 *
 *   // ... use the library normally ...
 *
 *   // On shutdown, check for leaked pins:
 *   int leaks = asset_debug_check_leaks(&mgr);
 *   // leaks > 0 means you have unbalanced pin/unpin calls
 */

#ifndef ASSET_DEBUG_H__
#define ASSET_DEBUG_H__

#include <stdint.h>

/* Poison pattern written to evicted slot memory.
 * If you see 0xDEADF00D in your textures/models, you're reading
 * from an evicted slot. */
#define ASSET_DEBUG_POISON      (0xDEADF00D)

/* Maximum number of pin/unpin events to track */
#define ASSET_DEBUG_MAX_EVENTS  (256)

#ifdef ASSET_DEBUG

/* Debug log callback type. Signature matches printf. */
typedef int (*asset_debug_log_fn)(const char *fmt, ...);

/* Pin/unpin event record */
typedef struct {
    uint32_t    asset_id;
    int         delta;      /* +1 = pin, -1 = unpin */
    uint32_t    pin_count;  /* Pin count after this operation */
} asset_debug_pin_event_t;

/* Debug state (global, one instance) */
typedef struct {
    asset_debug_log_fn      log_fn;
    asset_debug_pin_event_t pin_events[ASSET_DEBUG_MAX_EVENTS];
    uint32_t                pin_event_count;
    uint32_t                eviction_count;
    uint32_t                poison_checks_passed;
    uint32_t                poison_checks_failed;
} asset_debug_state_t;

/* Global debug state */
extern asset_debug_state_t asset_debug;

/**
 * Set the debug log callback. NULL disables logging.
 */
void asset_debug_set_log(asset_debug_log_fn fn);

/**
 * Called internally when a slot is evicted. Poisons the memory.
 */
void asset_debug_on_evict(uint8_t *data, uint32_t size, uint32_t asset_id);

/**
 * Called internally on pin. Logs and tracks the event.
 */
void asset_debug_on_pin(uint32_t asset_id, uint32_t new_pin_count);

/**
 * Called internally on unpin. Logs, tracks, and warns on unbalanced unpin.
 */
void asset_debug_on_unpin(uint32_t asset_id, uint32_t old_pin_count);

/**
 * Called internally on asset load. Logs cache hit/miss.
 */
void asset_debug_on_load(uint32_t asset_id, int cache_hit, int slot_idx);

/**
 * Check a pointer for poison pattern. Returns true if the first N words
 * match the poison pattern (meaning the data was evicted and not reloaded).
 */
int asset_debug_is_poisoned(const void *data, uint32_t check_bytes);

/**
 * Check for leaked pins on shutdown. Logs any assets still pinned.
 * Returns number of leaked pin slots.
 */
struct asset_manager_t;  /* Forward declaration */
int asset_debug_check_leaks(void *mgr);

/**
 * Print a summary of debug statistics.
 */
void asset_debug_print_stats(void);

/* Internal macros used by cache/manager code */
#define ASSET_DBG_ON_EVICT(data, size, id)  asset_debug_on_evict(data, size, id)
#define ASSET_DBG_ON_PIN(id, cnt)           asset_debug_on_pin(id, cnt)
#define ASSET_DBG_ON_UNPIN(id, cnt)         asset_debug_on_unpin(id, cnt)
#define ASSET_DBG_ON_LOAD(id, hit, slot)    asset_debug_on_load(id, hit, slot)
#define ASSET_DBG_LOG(...)                  do { if (asset_debug.log_fn) asset_debug.log_fn(__VA_ARGS__); } while(0)

#else /* !ASSET_DEBUG */

/* All debug macros compile to nothing */
#define ASSET_DBG_ON_EVICT(data, size, id)  ((void)0)
#define ASSET_DBG_ON_PIN(id, cnt)           ((void)0)
#define ASSET_DBG_ON_UNPIN(id, cnt)         ((void)0)
#define ASSET_DBG_ON_LOAD(id, hit, slot)    ((void)0)
#define ASSET_DBG_LOG(...)                  ((void)0)

/* Stub functions that do nothing in release */
static inline void asset_debug_set_log(void *fn) { (void)fn; }
static inline int asset_debug_check_leaks(void *mgr) { (void)mgr; return 0; }
static inline int asset_debug_is_poisoned(const void *data, uint32_t n) {
    (void)data; (void)n; return 0;
}
static inline void asset_debug_print_stats(void) {}

#endif /* ASSET_DEBUG */

#endif /* ASSET_DEBUG_H__ */
