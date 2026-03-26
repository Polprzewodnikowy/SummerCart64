#include <trx/game/lua/common.h>
#include <trx/game/music/common.h>

#include <lauxlib.h>

// trxc.music.get_track()
static int M_L_MusicGetTrack(lua_State *const L)
{
    const MUSIC_ID track = Music_GetCurrentPlayingTrack();
    if (track < 0) {
        lua_pushnil(L);
    } else {
        lua_pushinteger(L, (lua_Integer)track);
    }
    return 1;
}

// trxc.music.play_track(id[, opts])
static int M_L_MusicPlayTrack(lua_State *const L)
{
    const lua_Integer id = luaL_checkinteger(L, 1);
    const MUSIC_PLAY_MODE mode = luaL_checkinteger(L, 2);
    if (!Music_Play_Direct((MUSIC_ID)id, mode)) {
        return luaL_error(
            L, "invalid music track or mode (id=%d, mode=%d)", id, mode);
    }
    return 0;
}

// trxc.music.pause()
static int M_L_MusicPause(lua_State *const L)
{
    Music_Pause();
    return 0;
}

// trxc.music.unpause()
static int M_L_MusicUnpause(lua_State *const L)
{
    Music_Unpause();
    return 0;
}

// trxc.music.stop()
static int M_L_MusicStop(lua_State *const L)
{
    Music_Stop();
    return 0;
}

void LUA_CreateMusic(lua_State *const L)
{
    lua_getglobal(L, "trxc");
    lua_newtable(L);

    lua_newtable(L);
    lua_pushinteger(L, MPM_ONCE);
    lua_setfield(L, -2, "ONCE");
    lua_pushinteger(L, MPM_LOOP);
    lua_setfield(L, -2, "LOOP");
    lua_pushinteger(L, MPM_DELAY);
    lua_setfield(L, -2, "DELAY");
    lua_pushinteger(L, MPM_NO_REPEAT);
    lua_setfield(L, -2, "NO_REPEAT");
    lua_pushinteger(L, MPM_OVERLAY);
    lua_setfield(L, -2, "OVERLAY");
    lua_setfield(L, -2, "PlayMode");

    lua_pushcfunction(L, M_L_MusicGetTrack);
    lua_setfield(L, -2, "get_track");
    lua_pushcfunction(L, M_L_MusicPlayTrack);
    lua_setfield(L, -2, "play_track");
    lua_pushcfunction(L, M_L_MusicPlayTrack);
    lua_setfield(L, -2, "play");
    lua_pushcfunction(L, M_L_MusicPause);
    lua_setfield(L, -2, "pause");
    lua_pushcfunction(L, M_L_MusicUnpause);
    lua_setfield(L, -2, "unpause");
    lua_pushcfunction(L, M_L_MusicStop);
    lua_setfield(L, -2, "stop");

    lua_setfield(L, -2, "music");
    lua_pop(L, 1);
}
