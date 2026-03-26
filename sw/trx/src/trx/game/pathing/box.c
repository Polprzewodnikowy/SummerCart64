#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/game_buf.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/version.h>

#define BOX_OVERLAP_BITS 0x3FFF
#define BOX_SEARCH_NUMBER 0x7FFF
#define BOX_END_BIT 0x8000
#define BOX_NUMBER_BITS 0x7FFF // = ~BOX_END_BIT

#define BOX_MAX_EXPANSION 5
#define BOX_BIFF (WALL_L / 2) // = 0x200 = 512
#define BOX_CLIP_LEFT 1
#define BOX_CLIP_RIGHT 2
#define BOX_CLIP_TOP 4
#define BOX_CLIP_BOTTOM 8
#define BOX_CLIP_ALL                                                           \
    (BOX_CLIP_LEFT | BOX_CLIP_RIGHT | BOX_CLIP_TOP | BOX_CLIP_BOTTOM) // = 15
#define BOX_CLIP_SECONDARY 16

static int32_t m_BoxCount = 0;
static BOX_INFO *m_Boxes = nullptr;
static int16_t *m_Overlaps = nullptr;
static int16_t *m_FlyZone[2] = {};
static int16_t *m_GroundZone[MAX_ZONES][2] = {};

void Box_InitialiseBoxes(const int32_t num_boxes)
{
    m_BoxCount = num_boxes;
    m_Boxes = num_boxes == 0
        ? nullptr
        : GameBuf_Alloc(sizeof(BOX_INFO) * num_boxes, GBUF_BOXES);

    if (num_boxes == 0) {
        return;
    }

    for (int32_t i = 0; i < 2; i++) {
        for (int32_t j = 0; j < MAX_ZONES; j++) {
            m_GroundZone[j][i] =
                GameBuf_Alloc(sizeof(int16_t) * num_boxes, GBUF_GROUND_ZONE);
        }
        m_FlyZone[i] =
            GameBuf_Alloc(sizeof(int16_t) * num_boxes, GBUF_FLY_ZONE);
    }
}

int16_t *Box_InitialiseOverlaps(const int32_t num_overlaps)
{
    m_Overlaps = num_overlaps == 0
        ? nullptr
        : GameBuf_Alloc(sizeof(int16_t) * num_overlaps, GBUF_OVERLAPS);
    return m_Overlaps;
}

int32_t Box_GetCount(void)
{
    return m_BoxCount;
}

BOX_INFO *Box_GetBox(const int32_t box_idx)
{
    // TODO: in many cases, NO_BOX is blindly passed here and goes unchecked.
    // Update each instance to handle NO_BOX safely.
    return m_Boxes == nullptr ? nullptr : &m_Boxes[box_idx];
}

int16_t Box_GetOverlap(const int32_t overlap_idx)
{
    return m_Overlaps == nullptr ? -1 : m_Overlaps[overlap_idx];
}

int16_t *Box_GetFlyZone(const bool flip_status)
{
    return m_FlyZone[flip_status];
}

int16_t *Box_GetGroundZone(const bool flip_status, const int32_t zone_idx)
{
    return m_GroundZone[zone_idx][flip_status];
}

int16_t *Box_GetLotZone(const LOT_INFO *const lot)
{
    const bool flip_status = Room_GetFlipStatus();
    return lot->setup.fly != 0
        ? Box_GetFlyZone(flip_status)
        : Box_GetGroundZone(flip_status, BOX_ZONE(lot->setup.step));
}

bool Box_SearchLOT(LOT_INFO *const lot, const int32_t expansion)
{
    const int16_t *const zone = Box_GetLotZone(lot);
    const bool use_fixed_fly_zone = g_TRVersion == 3 && lot->setup.fly != 0;
    const int16_t search_zone =
        use_fixed_fly_zone ? BOX_FIXED_FLY_ZONE : zone[lot->head];

    for (int32_t i = 0; i < expansion; i++) {
        if (lot->head == NO_BOX) {
            if (g_TRVersion >= 2) {
                lot->tail = NO_BOX;
            }
            return false;
        }

        BOX_NODE *const node = &lot->node[lot->head];
        const BOX_INFO *const head_box = Box_GetBox(lot->head);

        bool done = false;
        int32_t index = head_box->overlap_index & BOX_OVERLAP_BITS;
        while (!done) {
            int16_t box_num = Box_GetOverlap(index++);
            if ((box_num & BOX_END_BIT) != 0) {
                done = true;
                box_num &= BOX_NUMBER_BITS;
            }

            if (!use_fixed_fly_zone && search_zone != zone[box_num]) {
                continue;
            }

            const BOX_INFO *const box = Box_GetBox(box_num);
            const int32_t change = box->height - head_box->height;
            if (change > lot->setup.step || change < lot->setup.drop) {
                continue;
            }

            BOX_NODE *const expand = &lot->node[box_num];
            const int16_t node_search_num =
                node->search_num & BOX_SEARCH_NUMBER;
            const int16_t expand_search_num =
                expand->search_num & BOX_SEARCH_NUMBER;
            const bool node_search_blocked =
                (node->search_num & BOX_BLOCKED_SEARCH) != 0;
            const bool expand_search_blocked =
                (expand->search_num & BOX_BLOCKED_SEARCH) != 0;
            if (node_search_num < expand_search_num) {
                continue;
            }

            if (node_search_blocked) {
                if (expand_search_num == node_search_num) {
                    continue;
                }
                expand->search_num = node->search_num;
            } else {
                if (expand_search_num == node_search_num
                    && !expand_search_blocked) {
                    continue;
                }

                if ((box->overlap_index & lot->setup.block_mask) != 0) {
                    expand->search_num = node->search_num | BOX_BLOCKED_SEARCH;
                } else {
                    expand->search_num = node->search_num;
                    expand->exit_box = lot->head;
                }
            }

            if (expand->next_expansion == NO_BOX && box_num != lot->tail) {
                lot->node[lot->tail].next_expansion = box_num;
                lot->tail = box_num;
            }
        }

        lot->head = node->next_expansion;
        node->next_expansion = NO_BOX;
    }

    return true;
}

bool Box_UpdateLOT(LOT_INFO *const lot, const int32_t expansion)
{
    if (lot->required_box == NO_BOX || lot->required_box == lot->target_box) {
        goto end;
    }

    lot->target_box = lot->required_box;
    BOX_NODE *const expand = &lot->node[lot->target_box];
    if (expand->next_expansion == NO_BOX && lot->tail != lot->target_box) {
        expand->next_expansion = lot->head;
        if (lot->head == NO_BOX) {
            lot->tail = lot->target_box;
        }
        lot->head = lot->target_box;
    }
    lot->search_num++;
    expand->search_num = lot->search_num;
    expand->exit_box = NO_BOX;

end:
    return Box_SearchLOT(lot, expansion);
}

void Box_TargetBox(LOT_INFO *const lot, int16_t box_num)
{
    box_num &= BOX_NUMBER_BITS;
    const BOX_INFO *const box = Box_GetBox(box_num);

    // TODO: determine if the shift is essential
    const int32_t shift = g_TRVersion >= 2 ? 1 : 0;
    lot->target.z = box->left + WALL_L / 2
        + (Random_GetControl() * (box->right + shift - box->left - WALL_L)
           >> 15);
    lot->target.x = box->top + WALL_L / 2
        + (Random_GetControl() * (box->bottom + shift - box->top - WALL_L)
           >> 15);
    lot->required_box = box_num;
    if (lot->setup.fly != 0) {
        lot->target.y = box->height - STEP_L * 3 / 2;
    } else {
        lot->target.y = box->height;
    }
}

bool Box_StalkBox(
    const ITEM *const item, const ITEM *const enemy, const int16_t box_num)
{
    if (enemy == nullptr) {
        return false;
    }

    const BOX_INFO *const box = Box_GetBox(box_num);

    // TODO: determine if the shift is essential
    const int32_t shift = g_TRVersion >= 2 ? 1 : 0;
    const int32_t z = ((box->left + box->right + shift) >> 1) - enemy->pos.z;
    const int32_t x = ((box->top + box->bottom + shift) >> 1) - enemy->pos.x;
    const int32_t x_range = g_TRVersion >= 2
        ? box->bottom + shift - box->top + CREATURE_STALK_DIST
        : CREATURE_STALK_DIST;
    const int32_t z_range = g_TRVersion >= 2
        ? box->right + shift - box->left + CREATURE_STALK_DIST
        : CREATURE_STALK_DIST;
    if (x > x_range || x < -x_range || z > z_range || z < -z_range) {
        return false;
    }

    const int32_t enemy_quad = (enemy->rot.y >> 14) + 2;
    const int32_t box_quad = (z > 0) ? ((x > 0) ? DIR_SOUTH : DIR_EAST)
                                     : ((x > 0) ? DIR_WEST : DIR_NORTH);
    if (enemy_quad == box_quad) {
        return false;
    }

    const int32_t baddie_quad = item->pos.z > enemy->pos.z
        ? (item->pos.x > enemy->pos.x ? DIR_SOUTH : DIR_EAST)
        : (item->pos.x > enemy->pos.x ? DIR_WEST : DIR_NORTH);

    return enemy_quad != baddie_quad || ABS(enemy_quad - box_quad) != 2;
}

bool Box_EscapeBox(
    const ITEM *item, const ITEM *const enemy, const int16_t box_num)
{
    const BOX_INFO *const box = Box_GetBox(box_num);

    // TODO: determine if the shift is essential
    const int32_t shift = g_TRVersion >= 2 ? 1 : 0;
    const int32_t x = ((box->top + box->bottom + shift) >> 1) - enemy->pos.x;
    const int32_t z = ((box->left + box->right + shift) >> 1) - enemy->pos.z;
    if (x > -CREATURE_ESCAPE_DIST && x < CREATURE_ESCAPE_DIST
        && z > -CREATURE_ESCAPE_DIST && z < CREATURE_ESCAPE_DIST) {
        return false;
    }

    return ((z > 0) == (item->pos.z > enemy->pos.z))
        || ((x > 0) == (item->pos.x > enemy->pos.x));
}

bool Box_ValidBox(
    const ITEM *item, const int16_t zone_num, const int16_t box_num)
{
    const CREATURE *const creature = item->creature_data;
    const int16_t *const zone = Box_GetLotZone(&creature->lot);
    const bool use_fixed_fly_zone =
        g_TRVersion == 3 && creature->lot.setup.fly != 0;
    if (!use_fixed_fly_zone && zone[box_num] != zone_num) {
        return false;
    }

    const BOX_INFO *const box = Box_GetBox(box_num);
    if ((box->overlap_index & creature->lot.setup.block_mask) != 0) {
        return false;
    }

    // TODO: determine if the shift is essential
    const int32_t shift = g_TRVersion >= 2 ? 1 : 0;
    return !(
        item->pos.z > box->left && item->pos.z < box->right + shift
        && item->pos.x > box->top && item->pos.x < box->bottom + shift);
}

TARGET_TYPE Box_CalculateTarget(
    XYZ_32 *const target, const ITEM *const item, LOT_INFO *const lot)
{
    Box_UpdateLOT(lot, BOX_MAX_EXPANSION);

    *target = item->pos;

    int32_t box_num = item->box_num;
    if (box_num == NO_BOX) {
        return TARGET_NONE;
    }

    int32_t bottom = 0;
    int32_t top = 0;
    int32_t right = 0;
    int32_t left = 0;

    const BOX_INFO *box = nullptr;
    int32_t prime_free = BOX_CLIP_ALL;
    do {
        box = Box_GetBox(box_num);
        if (lot->setup.fly != 0) {
            CLAMPG(target->y, box->height - WALL_L);
        } else {
            CLAMPG(target->y, box->height);
        }

        if (item->pos.z >= box->left && item->pos.z <= box->right
            && item->pos.x >= box->top && item->pos.x <= box->bottom) {
            left = box->left;
            right = box->right;
            top = box->top;
            bottom = box->bottom;
        } else {
            if (item->pos.z < box->left) {
                if ((prime_free & BOX_CLIP_LEFT) != 0 && item->pos.x >= box->top
                    && item->pos.x <= box->bottom) {
                    CLAMPL(target->z, box->left + BOX_BIFF);
                    if ((prime_free & BOX_CLIP_SECONDARY) != 0) {
                        return TARGET_SECONDARY;
                    }
                    CLAMPL(top, box->top);
                    CLAMPG(bottom, box->bottom);
                    prime_free = BOX_CLIP_LEFT;
                } else if (prime_free != BOX_CLIP_LEFT) {
                    target->z = right - BOX_BIFF;
                    if (prime_free != BOX_CLIP_ALL) {
                        return TARGET_SECONDARY;
                    }
                    prime_free |= BOX_CLIP_SECONDARY;
                }
            } else if (item->pos.z > box->right) {
                if ((prime_free & BOX_CLIP_RIGHT) != 0
                    && item->pos.x >= box->top && item->pos.x <= box->bottom) {
                    CLAMPG(target->z, box->right - BOX_BIFF);
                    if ((prime_free & BOX_CLIP_SECONDARY) != 0) {
                        return TARGET_SECONDARY;
                    }
                    CLAMPL(top, box->top);
                    CLAMPG(bottom, box->bottom);
                    prime_free = BOX_CLIP_RIGHT;
                } else if (prime_free != BOX_CLIP_RIGHT) {
                    target->z = left + BOX_BIFF;
                    if (prime_free != BOX_CLIP_ALL) {
                        return TARGET_SECONDARY;
                    }
                    prime_free |= BOX_CLIP_SECONDARY;
                }
            }

            if (item->pos.x < box->top) {
                if ((prime_free & BOX_CLIP_TOP) != 0 && item->pos.z >= box->left
                    && item->pos.z <= box->right) {
                    CLAMPL(target->x, box->top + BOX_BIFF);
                    if ((prime_free & BOX_CLIP_SECONDARY) != 0) {
                        return TARGET_SECONDARY;
                    }
                    CLAMPL(left, box->left);
                    CLAMPG(right, box->right);
                    prime_free = BOX_CLIP_TOP;
                } else if (prime_free != BOX_CLIP_TOP) {
                    target->x = bottom - BOX_BIFF;
                    if (prime_free != BOX_CLIP_ALL) {
                        return TARGET_SECONDARY;
                    }
                    prime_free |= BOX_CLIP_SECONDARY;
                }
            } else if (item->pos.x > box->bottom) {
                if ((prime_free & BOX_CLIP_BOTTOM) != 0
                    && item->pos.z >= box->left && item->pos.z <= box->right) {
                    CLAMPG(target->x, box->bottom - BOX_BIFF);
                    if ((prime_free & BOX_CLIP_SECONDARY) != 0) {
                        return TARGET_SECONDARY;
                    }
                    CLAMPL(left, box->left);
                    CLAMPG(right, box->right);
                    prime_free = BOX_CLIP_BOTTOM;
                } else if (prime_free != BOX_CLIP_BOTTOM) {
                    target->x = top + BOX_BIFF;
                    if (prime_free != BOX_CLIP_ALL) {
                        return TARGET_SECONDARY;
                    }
                    prime_free |= BOX_CLIP_SECONDARY;
                }
            }
        }

        if (box_num == lot->target_box) {
            if ((prime_free & (BOX_CLIP_LEFT | BOX_CLIP_RIGHT)) != 0) {
                target->z = lot->target.z;
            } else if ((prime_free & BOX_CLIP_SECONDARY) == 0) {
                CLAMP(target->z, box->left + BOX_BIFF, box->right - BOX_BIFF);
            }

            if ((prime_free & (BOX_CLIP_TOP | BOX_CLIP_BOTTOM)) != 0) {
                target->x = lot->target.x;
            } else if ((prime_free & BOX_CLIP_SECONDARY) == 0) {
                CLAMP(target->x, box->top + BOX_BIFF, box->bottom - BOX_BIFF);
            }

            target->y = lot->target.y;
            return TARGET_PRIMARY;
        }

        box_num = lot->node[box_num].exit_box;
        if (box_num != NO_BOX
            && (Box_GetBox(box_num)->overlap_index & lot->setup.block_mask)
                != 0) {
            break;
        }
    } while (box_num != NO_BOX);

    if ((prime_free & (BOX_CLIP_LEFT | BOX_CLIP_RIGHT)) != 0) {
        target->z = box->left + WALL_L / 2
            + (((box->right - box->left - WALL_L) * Random_GetControl()) >> 15);
    } else if ((prime_free & BOX_CLIP_SECONDARY) == 0) {
        CLAMP(target->z, box->left + BOX_BIFF, box->right - BOX_BIFF);
    }

    if ((prime_free & (BOX_CLIP_TOP | BOX_CLIP_BOTTOM)) != 0) {
        target->x = box->top + WALL_L / 2
            + (((box->bottom - box->top - WALL_L) * Random_GetControl()) >> 15);
    } else if ((prime_free & BOX_CLIP_SECONDARY) == 0) {
        CLAMP(target->x, box->top + BOX_BIFF, box->bottom - BOX_BIFF);
    }

    if (lot->setup.fly != 0) {
        target->y = box->height - STEP_L * 3 / 2;
    } else {
        target->y = box->height;
    }

    return TARGET_NONE;
}

bool Box_BadFloor(
    const int32_t x, const int32_t y, const int32_t z, const int32_t box_height,
    const int32_t next_height, int16_t room_num, const LOT_INFO *const lot)
{
    const SECTOR *const sector =
        Room_GetSector((XYZ_32) { x, y, z }, &room_num);
    if (sector->box == NO_BOX) {
        return true;
    }

    const BOX_INFO *const box = Box_GetBox(sector->box);
    if ((box->overlap_index & lot->setup.block_mask) != 0) {
        return true;
    }

    const int32_t height = box->height;
    if (box_height - height > lot->setup.step
        || box_height - height < lot->setup.drop) {
        return true;
    }
    if (box_height - height < -lot->setup.step && height > next_height) {
        return true;
    }
    if (lot->setup.fly != 0 && y > height + lot->setup.fly) {
        return true;
    }
    return false;
}

int32_t Box_GetZoneCount(void)
{
    return g_TRVersion == 1 ? 2 : 4;
}
