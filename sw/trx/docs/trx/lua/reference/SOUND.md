---
title: Sound
order: 7
---

## Sound module

## Functions

- [lua]`trx.sound.is_available(id)`  
    Returns `true` if the specified sound sample is available.
- [lua]`trx.sound.stop(id)`  
    Stops the specified sound effect.
- [lua]`trx.sound.play(id[, opts])`  
    Plays specified sound effect. `opts.pos` may be a `{ x=, y=, z= }` table for position.  
    Examples:
    - [lua]`trx.sound.play(99)`  
      Plays the sound 99 (in TR1, this is an explosion, in TR2 this is a tiger's roar) at full volume.
    - [lua]`trx.sound.play(99, { pos = { x = 100, y = 200, z = 50 } })`  
      Plays the same sound at world position (100,200,50), applying pan and volume accordingly.
- [lua]`trx.sound.stop_all()`  
    Stops all currently playing sound effects.
