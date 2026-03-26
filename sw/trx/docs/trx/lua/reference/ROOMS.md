---
title: Rooms
order: 10
---

## Rooms module

Module for inspecting all rooms in the current level.

### Structures

- [lua]`trx.rooms.fn.FlipStatus`:
    - `trx.rooms.fn.FlipStatus.NONE`  
        This is a normal room.
    - `trx.rooms.fn.FlipStatus.UNFLIPPED`  
        This room is currently reachable by Lara.
    - `trx.rooms.fn.FlipStatus.FLIPPED`  
        This room is currently inactive and unreachable by Lara.

- [lua]`trx.rooms.fn.Room`

    Represents a room.

    Properties:
    - **`num`**: 1-based room number.
    - **`underwater`**: Whether the room is underwater or not.
    - **`wind`**: Whether the room has breeze enabled or not. (Requires the player to have breeze enabled in the game settings).
    - **`bounds`**: a table with world-coordinate bounds of the room. The table contains:
      - **`min_x`**: minimum x coordinate.
      - **`min_y`**: minimum y coordinate.
      - **`min_z`**: minimum z coordinate.
      - **`max_x`**: maximum x coordinate.
      - **`max_y`**: maximum y coordinate.
      - **`max_z`**: maximum z coordinate.
    - **`internal_bounds`**: similar to `bounds`, but excludes the outer sector.
    - **`flip_status`**: current room flip status (see `trx.rooms.fn.FlipStatus`).
    - **`flipped_room`**: linked flip room of this room.

    Writable properties:
    - `underwater`
    - `wind`

### Functions

-- Uses Lua length operator on the rooms table:
- [lua]`#trx.rooms`  
  Returns the total number of rooms.

- [lua]`trx.rooms[num]`  
  Retrieves the [lua]`trx.rooms.fn.Room` at the given 1-based index, or `nil` if out of range.

- [lua]`trx.rooms.fn.get(arg)`  
  Alias of `trx.rooms[arg]`.

- [lua]`trx.rooms.flip()`  
  Flips the current room map.

- [lua]`trx.rooms.flip_effect(effect_id, [timer])`  
  Sets the flip effect id (0-based), and optionally the flip timer.  
  Use `effect_id=-1` to disable the current effect.
