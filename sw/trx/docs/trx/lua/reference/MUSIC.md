---
title: Music
order: 6
---

## Music module

## Functions

- [lua]`trx.music.get_track()`  
    Returns current playing track ID, or `nil` if none.
- [lua]`trx.music.play(id[, opts])`  
    Plays specified track. `opts.mode` selects a play mode constant. Errors if the track ID or mode is invalid.  
    Examples:
    - [lua]`trx.music.play(1)`  
      Plays track 1 once.
    - [lua]`trx.music.play(2, { mode = trx.music.PlayMode.LOOP })`  
      Plays track 2 as a looped track.
- [lua]`trx.music.pause()`  
    Pauses the music.
- [lua]`trx.music.unpause()`  
    Resumes paused music.
- [lua]`trx.music.stop()`  
    Stops all music.

## Play mode constants

- `trx.music.PlayMode.ONCE`  
    Plays the track once; after it finishes, any active looped track resumes.
- `trx.music.PlayMode.LOOP`  
    Plays the track in looped mode continuously. This track becomes the ambient track.
- `trx.music.PlayMode.NO_REPEAT`  
    Plays the track once but prevents retriggering if it's already playing.
- `trx.music.PlayMode.DELAY`  
    Schedules the track for later playback without starting it immediately.
- `trx.music.PlayMode.OVERLAY`  
    Schedules the track on top of current music track.
