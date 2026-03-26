#pragma once

#include <trx/game/music/types.h>

MUSIC_BACKEND *Music_Backend_CDAudio_Factory(
    const char *path, const char *control_path);
