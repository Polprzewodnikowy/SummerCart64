---
title: Items
order: 4
---

## Items module

Module for controlling all moveables behavior.

### Structures

- [lua]`trx.items.Item`

    Represents an item, also known as a moveable.

    Properties:
    - **`pos`**: A table with fields `x`, `y`, `z` representing position.
    - **`rot`**: A table with fields `x`, `y`, `z` representing rotation.
    - **`anim`**: Current animation number (0-indexed).
    - **`frame`**: Current frame number (0-indexed).
    - **`room_num`**: room number.
    - **`room`**: [`trx.rooms.Room`] object for the room containing this item.
    - **`status`**: Integer representing the item's status.
    - **`flags`**: Integer representing the item's trigger-related flags.
    - **`timer`**: Integer representing the item's trigger-related timer value.
    - **`hit_points`**: Integer representing the item's hit points.
    - **`max_hit_points`**: Integer representing the item's hit points.
    - **`object_id`**: Integer ID of the item's object type.
    - **`name`**: String name of the item, or `nil` if none.

    Writable properties:
    - `pos` (updating this also updates `room` and `room_num`)
    - `rot`
    - `anim`
    - `frame`
    - `hit_points` (updating this also may increase `max_hit_points`)
    - `max_hit_points`
    - `name` (string identifier; setting duplicates raises an error)

### Functions

- [lua]`#trx.items`  
  Returns the total number of allocated items.

- [lua]`trx.items[num]`  
  [lua]`trx.items["name"]`  
  Retrieves the [lua]`trx.items.Item` at the given 1-based index or with the given `name`, or `nil` if out of range/not found.  

  Example:
  ```lua
  local item = trx.items[1]
  item.name = "lara"
  local lara = trx.items["lara"]
  ```

- [lua]`trx.items.fn.get(arg)`  
  Alias of `trx.items[arg]`.

  Example:
  ```lua
  local item_hp = trx.items.fn.get(17).hit_points
  local lara_hp = trx.items.fn.get("lara").hit_points
  ```

- [lua]`trx.items.find(query)`  
  Finds all items matching the query and returns a list of [lua]`trx.items.Item`.

  Supported query fields:
  - `object_id`
  - `room_num`

  Unknown query fields are ignored and logged as warnings.

  Example:
  ```lua
  local wolves = trx.items.find({ object_id = trx.catalog.objects.wolf })
  local wolves_in_room_7 = trx.items.find({
    object_id = trx.catalog.objects.wolf,
    room_num = 7,
  })
  ```

- [lua]`trx.items.first(query)`  
  Finds the first item matching the query and returns a [lua]`trx.items.Item`, or `nil` if none match.

  Supported query fields:
  - `object_id`
  - `room_num`

  Unknown query fields are ignored and logged as warnings.

  Example:
  ```lua
  local first_natla = trx.items.first({
    object_id = trx.catalog.objects.natla,
  })
  ```
