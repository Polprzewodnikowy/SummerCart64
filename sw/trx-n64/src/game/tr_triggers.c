/*
 * TRX-N64: Floor data parser and trigger system
 *
 * Parses TR1's floor data to determine:
 *   - Tilted floor/ceiling heights
 *   - Room portals (wall/pit/sky connections)
 *   - Gameplay triggers (switches, pads, pickups, etc.)
 *   - Environmental hazards (lava/death sectors)
 *   - Climbable walls (ladders)
 *
 * All bit layouts, mask values, and parsing algorithms are exact
 * reproductions from TRX (rooms/floor_data.c, rooms/geometry.c).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "tr_game_shim.h"

/* -------------------------------------------------------------------
 * Floor data bit extraction macros (exact TRX values)
 * ------------------------------------------------------------------- */

#define FD_DONE(x)          ((x) & 0x8000)
#define FD_ENTRY_TYPE(x)    ((x) & 0x1F)
#define FD_TRIG_TYPE(x)     (((x) & 0x7F00) >> 8)
#define FD_TRIG_MASK(x)     ((x) & 0x3E00)
#define FD_TRIG_ONE_SHOT(x) (((x) & 0x100) != 0)
#define FD_TRIG_TIMER(x)    ((x) & 0xFF)
#define FD_TRIG_CMD_TYPE(x) (((x) & 0x7C00) >> 10)
#define FD_TRIG_CMD_ARG(x)  ((x) & 0x3FF)
#define FD_LADDER_TYPE(x)   (((x) & 0x7F00) >> 8)

/* Floor types */
#define FT_FLOOR    0
#define FT_DOOR     1
#define FT_TILT     2
#define FT_ROOF     3
#define FT_TRIGGER  4
#define FT_LAVA     5
#define FT_CLIMB    6

/* Trigger types */
#define TT_TRIGGER      0
#define TT_PAD          1
#define TT_SWITCH       2
#define TT_KEY          3
#define TT_PICKUP       4
#define TT_HEAVY        5
#define TT_ANTIPAD      6
#define TT_COMBAT       7
#define TT_DUMMY        8
#define TT_ANTITRIGGER  9

/* Trigger command types */
#define TO_OBJECT       0
#define TO_CAMERA       1
#define TO_SINK         2
#define TO_FLIPMAP      3
#define TO_FLIPON       4
#define TO_FLIPOFF      5
#define TO_TARGET       6
#define TO_FINISH       7
#define TO_CD           8
#define TO_FLIPEFFECT   9
#define TO_SECRET       10
#define TO_BODY_BAG     11

/* Item flags */
#define IF_ONE_SHOT     0x0100
#define IF_CODE_BITS    0x3E00
#define IF_REVERSE      0x4000
#define IF_KILLED       0x8000

/* Tilt height calculation macros (exact TRX values) */
#define WALL_MASK       (1024 - 1)
#define NEG_TILT(T, H)  (((T) * ((H) & WALL_MASK)) >> 2)
#define POS_TILT(T, H)  (((T) * ((WALL_MASK - (H)) & WALL_MASK)) >> 2)

/* -------------------------------------------------------------------
 * Parsed floor data result for a sector
 * ------------------------------------------------------------------- */

typedef struct {
    int32_t floor_height;
    int32_t ceiling_height;
    bool    is_lava;
    bool    is_climb;
    uint8_t ladder_dirs;    /* LADDER_NORTH | EAST | SOUTH | WEST */
    bool    has_trigger;
    int16_t trigger_type;
    int16_t trigger_mask;
    int16_t trigger_timer;
    bool    trigger_one_shot;
} floor_data_result_t;

/* -------------------------------------------------------------------
 * Parse floor data for a sector, applying tilt modifications
 *
 * This replaces the basic floor height lookup in tr_floor.c with
 * the full floor data parser that handles tilted floors and triggers.
 * ------------------------------------------------------------------- */

void game_parse_floor_data(const game_state_t *gs,
                            const tr_sector_t *sector,
                            int32_t x, int32_t y, int32_t z,
                            floor_data_result_t *result) {
    result->floor_height = sector->floor * -STEP_L;
    result->ceiling_height = sector->ceiling * -STEP_L;
    result->is_lava = false;
    result->is_climb = false;
    result->ladder_dirs = 0;
    result->has_trigger = false;

    uint16_t fd_idx = sector->floor_data_idx;
    if (fd_idx == 0) return;  /* No floor data */

    const int16_t *fd = gs->level->floor_data;
    if (!fd) return;

    /* Walk the floor data chain */
    while (fd_idx > 0 && fd_idx < gs->level->floor_data_size) {
        int16_t entry = fd[fd_idx];
        int type = FD_ENTRY_TYPE(entry);

        switch (type) {
            case FT_TILT: {
                /* Tilted floor: next word contains X/Z tilt */
                fd_idx++;
                int16_t tilt_data = fd[fd_idx];
                int8_t x_off = (int8_t)(tilt_data & 0xFF);
                int8_t z_off = (int8_t)((tilt_data >> 8) & 0xFF);

                /* Apply tilt based on position within sector */
                if (z_off < 0)
                    result->floor_height -= NEG_TILT(z_off, z);
                else if (z_off > 0)
                    result->floor_height += POS_TILT(z_off, z);

                if (x_off < 0)
                    result->floor_height -= NEG_TILT(x_off, x);
                else if (x_off > 0)
                    result->floor_height += POS_TILT(x_off, x);
                break;
            }

            case FT_ROOF: {
                /* Tilted ceiling */
                fd_idx++;
                int16_t tilt_data = fd[fd_idx];
                int8_t x_off = (int8_t)(tilt_data & 0xFF);
                int8_t z_off = (int8_t)((tilt_data >> 8) & 0xFF);

                /* Ceiling tilts are inverted */
                if (z_off < 0)
                    result->ceiling_height += NEG_TILT(z_off, z);
                else if (z_off > 0)
                    result->ceiling_height -= POS_TILT(z_off, z);

                if (x_off < 0)
                    result->ceiling_height += NEG_TILT(x_off, x);
                else if (x_off > 0)
                    result->ceiling_height -= POS_TILT(x_off, x);
                break;
            }

            case FT_DOOR: {
                /* Portal to another room — skip room number word */
                fd_idx++;
                break;
            }

            case FT_LAVA: {
                result->is_lava = true;
                break;
            }

            case FT_CLIMB: {
                result->is_climb = true;
                result->ladder_dirs = FD_LADDER_TYPE(entry);
                break;
            }

            case FT_TRIGGER: {
                /* Parse trigger */
                result->has_trigger = true;
                result->trigger_type = FD_TRIG_TYPE(entry);

                fd_idx++;
                int16_t trig_setup = fd[fd_idx];
                result->trigger_mask = FD_TRIG_MASK(trig_setup);
                result->trigger_one_shot = FD_TRIG_ONE_SHOT(trig_setup);
                result->trigger_timer = FD_TRIG_TIMER(trig_setup);

                /* Skip trigger item index for switch/key/pickup types */
                if (result->trigger_type == TT_SWITCH ||
                    result->trigger_type == TT_KEY ||
                    result->trigger_type == TT_PICKUP) {
                    fd_idx++;
                }

                /* Skip command chain */
                while (1) {
                    fd_idx++;
                    if (fd_idx >= gs->level->floor_data_size) break;
                    int16_t cmd = fd[fd_idx];
                    int cmd_type = FD_TRIG_CMD_TYPE(cmd);

                    /* Camera commands have an extra word */
                    if (cmd_type == TO_CAMERA) {
                        fd_idx++;
                    }

                    if (FD_DONE(cmd)) break;
                }
                break;
            }

            default:
                break;
        }

        if (FD_DONE(entry)) break;
        fd_idx++;
    }
}

/* -------------------------------------------------------------------
 * Execute trigger commands when Lara enters a trigger sector
 * ------------------------------------------------------------------- */

void game_execute_trigger(game_state_t *gs,
                           const tr_sector_t *sector) {
    uint16_t fd_idx = sector->floor_data_idx;
    if (fd_idx == 0) return;

    const int16_t *fd = gs->level->floor_data;
    if (!fd) return;

    /* Find the FT_TRIGGER entry */
    while (fd_idx > 0 && fd_idx < gs->level->floor_data_size) {
        int16_t entry = fd[fd_idx];
        int type = FD_ENTRY_TYPE(entry);

        if (type == FT_TRIGGER) {
            int trig_type = FD_TRIG_TYPE(entry);

            fd_idx++;
            int16_t trig_setup = fd[fd_idx];
            int16_t mask = FD_TRIG_MASK(trig_setup);
            bool one_shot = FD_TRIG_ONE_SHOT(trig_setup);
            int16_t timer = FD_TRIG_TIMER(trig_setup);

            /* For switch/key/pickup: check the activating item */
            int16_t trigger_item = -1;
            if (trig_type == TT_SWITCH || trig_type == TT_KEY ||
                trig_type == TT_PICKUP) {
                fd_idx++;
                trigger_item = FD_TRIG_CMD_ARG(fd[fd_idx]);
                (void)trigger_item;
            }

            /* Only process pad/trigger types for Lara stepping on them.
             * Switch, key, pickup need explicit activation (M6+). */
            if (trig_type != TT_TRIGGER && trig_type != TT_PAD &&
                trig_type != TT_HEAVY)
                break;

            /* Process command chain */
            while (1) {
                fd_idx++;
                if (fd_idx >= gs->level->floor_data_size) break;
                int16_t cmd = fd[fd_idx];
                int cmd_type = FD_TRIG_CMD_TYPE(cmd);
                int cmd_arg = FD_TRIG_CMD_ARG(cmd);

                switch (cmd_type) {
                    case TO_OBJECT: {
                        /* Activate/deactivate an item */
                        if (cmd_arg >= 0 && cmd_arg < gs->item_count) {
                            game_item_t *target = &gs->items[cmd_arg];

                            /* Apply trigger mask */
                            if (trig_type == TT_ANTITRIGGER || trig_type == TT_ANTIPAD) {
                                target->flags &= ~mask;
                            } else {
                                target->flags |= mask;
                            }

                            /* Check if fully activated */
                            if ((target->flags & IF_CODE_BITS) == IF_CODE_BITS) {
                                if (!(target->flags & IF_ONE_SHOT) || !one_shot) {
                                    /* Activate the item */
                                    if (!target->active) {
                                        target->active = true;
                                        target->status = IS_ACTIVE;
                                        if (timer > 0) target->timer = timer * 30; /* Convert to frames */
                                    }
                                }
                                if (one_shot) {
                                    target->flags |= IF_ONE_SHOT;
                                }
                            }
                        }
                        break;
                    }

                    case TO_CAMERA: {
                        /* Fixed camera — skip extra data word */
                        fd_idx++;
                        /* TODO: implement fixed camera activation */
                        break;
                    }

                    case TO_FLIPMAP: {
                        /* Toggle flip map (alternate room geometry) */
                        gs->flip_status = !gs->flip_status;
                        gs->flip_effect = cmd_arg;
                        break;
                    }

                    case TO_FLIPON: {
                        gs->flip_status = true;
                        gs->flip_effect = cmd_arg;
                        break;
                    }

                    case TO_FLIPOFF: {
                        gs->flip_status = false;
                        gs->flip_effect = cmd_arg;
                        break;
                    }

                    case TO_FINISH: {
                        /* Level complete! */
                        /* TODO: trigger level transition */
                        break;
                    }

                    case TO_CD: {
                        /* Play music track */
                        /* TODO: audio system (M7) */
                        (void)cmd_arg;
                        break;
                    }

                    case TO_SECRET: {
                        /* Register secret found */
                        /* TODO: secret tracking */
                        break;
                    }

                    default:
                        break;
                }

                if (FD_DONE(cmd)) break;
            }

            break;  /* Only process first trigger entry */
        }

        if (type == FT_TILT || type == FT_ROOF || type == FT_DOOR) {
            fd_idx++;  /* These have an extra data word */
        }

        if (FD_DONE(entry)) break;
        fd_idx++;
    }
}

/* -------------------------------------------------------------------
 * Updated floor height query with tilt support
 *
 * Replaces the basic version in tr_floor.c
 * ------------------------------------------------------------------- */

int32_t game_get_floor_height_full(const game_state_t *gs,
                                    const tr_sector_t *sector,
                                    int32_t x, int32_t y, int32_t z) {
    floor_data_result_t result;
    game_parse_floor_data(gs, sector, x, y, z, &result);

    int32_t height = result.floor_height;

    /* Follow portal down (pit room) */
    if (sector->pit_room != 255) {
        int16_t below_room = sector->pit_room;
        if (below_room >= 0 && below_room < gs->level->room_count) {
            const tr_sector_t *below = game_get_sector(gs, below_room, x, z);
            if (below) {
                floor_data_result_t below_result;
                game_parse_floor_data(gs, below, x, y, z, &below_result);
                if (below_result.floor_height > height)
                    height = below_result.floor_height;
            }
        }
    }

    return height;
}
