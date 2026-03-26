---
title: Events
order: 2
---

## Events module

Lua scripts can listen for game events using the global `events` API.

### API

- [lua]`trx.events.before_level_file(callback)`
- [lua]`trx.events.after_level_file(callback)`
- [lua]`trx.events.after_level_state(callback)`
- [lua]`trx.events.on_game_start(callback)`
- [lua]`trx.events.on_pickup(callback)`  
- [lua]`trx.events.before_control(callback)`  
- [lua]`trx.events.after_control(callback)`  
  Register a handler for a game event. Returns `listener_id`.
- [lua]`trx.events.detach(listener_id)`  
  Remove a previously registered event handler.

### Events

#### `before_level_file`
Happens prior to loading the level file.

Arguments:
- `level_num`

#### `after_level_file`
Happens after the level finishes loading, prior to loading information from a
savegame.

Arguments:
- `level_num`

#### `after_level_state`
Happens after the level finishes loading, after loading information from a
savegame. If the game is started normally, this duplicates `after_level_file`.

Arguments:
- `level_num`

#### `on_game_start`
Happens after the level finishes loading and the game is about to start.
The difference from `after_level_file` and `after_level_state` is that this waits for the fade-to-black / cross-fade effects to
finish, and is suitable to play sound effects and run game logic.

Arguments:
- `level_num`
- `is_save`

#### `on_pickup`
Happens just after Lara picks up an item.

Arguments:
- `item_num`

#### `before_control`
Happens on every logical game frame, before executing main game logic.

Arguments: none  

#### `after_control`
Happens on every logical game frame, after executing main game logic.

Arguments: none  

### Examples

```lua
trx.events.before_level_file(function(level_num)
  -- handle pre-file-load setup
end)

trx.events.after_level_state(function(level_num)
  -- handle post-savegame state restore
end)

trx.events.on_pickup(function(item_num)
  trx.console.log(trx.items[item_num].object_id)
end)

local control_handler = trx.events.before_control(function()
  -- handle control loop event
end)
-- detach a handler
trx.events.detach(control_handler)
```
