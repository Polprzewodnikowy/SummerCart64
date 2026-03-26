---
title: Examples
order: 1
---

## LUA script examples

### Adjusting enemy HP

This example sets all bats to start with 20 hitpoints, and all wolves to start
with 30 hitpoints. Simply adjust the lookup table to fit your needs. Refer to
[this section](../../ENEMY_DEFAULTS.md) as a reference for original values.

```lua
-- Lookup table mapping object IDs to HP
local hp_lut = {
  [trx.catalog.objects.wolf] = 30,
  [trx.catalog.objects.bat] = 20,
}

-- Adjust HP of enemies when the level loads
trx.events.after_level_state(function(level)
  for i = 1, #trx.items do
    local item = trx.items[i]
    local hp = hp_lut[item.object_id]
    if hp ~= nil then
      item.hit_points = hp
      item.max_hit_points = hp
    end
  end
end)
```

### Teleporting Lara upon picking up a medipack

This will teleport Lara back to the starting point in TR1 Caves.

```lua
trx.events.on_pickup(function(pickup_item)
  local lara = trx.lara.item
  lara.pos = {
    x = 73.5 * 1024,
    y = 3 * 1024,
    z = 3.5 * 1024,
  }
end)
```

### Running code every control loop

This will run the provided function once every logical frame, meaning the
function will always run at 30 FPS regardless of the player's FPS settings.

```lua
trx.events.before_control(function()
  -- handle per-frame control logic
end)
```

### Changing water color in concrete rooms

This will change the color to crimson red if Lara is in room 15, and
demonstrates how to throttle updates to only happen if Lara goes from one room
to another.

```lua
local last_room = 0

trx.events.before_control(function()
  local lara = trx.lara.item
  if lara.room_num ~= last_room then
    last_room = lara.room_num
    if lara.room_num == 15 then
      trx.config.set("visuals.water_color", "ff0000")
    else
      trx.config.set("visuals.water_color", "0000ff")
    end
  end
end)
```
