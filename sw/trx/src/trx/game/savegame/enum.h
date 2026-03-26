#pragma once

typedef enum {
    SAVEGAME_STAGE_BEFORE_LOAD,
    SAVEGAME_STAGE_AFTER_LOAD,
    SAVEGAME_STAGE_BEFORE_SAVE,
} SAVEGAME_STAGE;

typedef enum {
    SG_VERSION_LEGACY = -1,
    SG_VERSION_1 = 1,

    // Before TRX 1.0
    SG_VERSION_13 = 13,

    // Separated Magnums and Automatic Pistols.
    SG_VERSION_14 = 14,

    // Replaced Lara mesh pointers with outfits
    SG_VERSION_15 = 15,

    // Music save format switched to stream list with play modes.
    SG_VERSION_16 = 16,

    // Carried-item drops are persisted with truthful statuses/positions.
    SG_VERSION_17 = 17,

    SG_MIN_SUPPORTED_VERSION = SG_VERSION_13,
    SG_CURRENT_VERSION = SG_VERSION_17,
} SAVEGAME_VERSION;
