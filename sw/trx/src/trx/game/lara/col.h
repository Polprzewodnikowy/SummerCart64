#pragma once

#include <trx/game/collision.h>
#include <trx/game/lara/enum.h>
#include <trx/game/rooms/types.h>

typedef enum {
    // clang-format off
    EDGE_CATCH_NEG  = -1,
    EDGE_CATCH_NONE = 0,
    EDGE_CATCH_POS  = 1,
    // clang-format on
} EDGE_CATCH;

typedef enum {
    LANDED_OK,
    LANDED_BAD,
    LANDED_HANDLED,
} LANDED_STATE;

typedef enum {
    SWING_CATCH_NONE,
    SWING_CATCH_FAST,
    SWING_CATCH_SLOW,
} SWING_CATCH;

void Lara_Col_Register(
    LARA_TRX_STATE state, void (*handle_func)(ITEM *item, COLL_INFO *coll));
void Lara_Col_Update(ITEM *item, COLL_INFO *coll);
void Lara_Col_GetInfo(const ITEM *item, COLL_INFO *coll);
void Lara_Col_Shift(COLL_INFO *coll);
bool Lara_Col_TestVault(ITEM *item, COLL_INFO *coll);
bool Lara_Col_TestSlide(ITEM *item, COLL_INFO *coll);
bool Lara_Col_TestLadderHang(ITEM *item, const COLL_INFO *coll);
bool Lara_Col_TestCeiling(ITEM *item, const COLL_INFO *coll);
SWING_CATCH Lara_Col_TestHangSwingIn(const ITEM *item, int16_t angle);
EDGE_CATCH Lara_Col_TestEdgeCatch(
    const ITEM *item, const COLL_INFO *coll, int32_t *edge);
bool Lara_Col_Fallen(ITEM *item, const COLL_INFO *coll);
void Lara_Col_DeflectEdgeJump(ITEM *item, COLL_INFO *coll);
LANDED_STATE Lara_Col_LandedBad(ITEM *item);
void Lara_Col_MonkeySwingSnap(ITEM *item);
void Lara_Col_HangTest(ITEM *item, COLL_INFO *coll);
void Lara_Col_ItemPush(
    const ITEM *item, COLL_INFO *coll, bool hit_on, bool big_push);
void Lara_Col_Static3DPush(const STATIC_MESH *mesh, COLL_INFO *coll);
void Lara_Col_WadeSplash(ITEM *item);
