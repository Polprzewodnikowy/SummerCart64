#pragma once

#include <trx/core/math.h>

typedef struct {
    int32_t left;
    int32_t right;
    int32_t top;
    int32_t bottom;
    int16_t height;
    int16_t overlap_index;
} BOX_INFO;

typedef struct {
    int16_t exit_box;
    uint16_t search_num;
    int16_t next_expansion;
    int16_t box_num;
} BOX_NODE;

typedef enum {
    LOT_SETUP_DEFAULT,
    LOT_SETUP_BEAST,
    LOT_SETUP_QUADRUPED,
    LOT_SETUP_JUMPER,
    LOT_SETUP_CLIMBER,
    LOT_SETUP_FLYER,
} LOT_SETUP_TYPE;

typedef struct {
    int16_t step;
    int16_t drop;
    int16_t fly;
    uint16_t block_mask;
} LOT_SETUP;

typedef struct {
    LOT_SETUP setup;
    BOX_NODE *node;
    int16_t head;
    int16_t tail;
    uint16_t search_num;
    int16_t zone_count;
    int16_t target_box;
    int16_t required_box;
    XYZ_32 target;
} LOT_INFO;

typedef enum {
    TARGET_NONE = 0,
    TARGET_PRIMARY = 1,
    TARGET_SECONDARY = 2,
} TARGET_TYPE;
