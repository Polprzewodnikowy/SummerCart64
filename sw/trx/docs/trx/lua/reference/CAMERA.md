---
title: Camera
order: 16
---

## Camera module

Module for inspecting the active camera state.

### Properties

- **`pos`**: current camera position as `{ x, y, z }`.
- **`room_num`**: 1-based room number of the camera position, or `nil` if unknown.
- **`room`**: [lua]`trx.rooms.Room` for the camera position, or `nil` if unknown.
- **`target_pos`**: current camera target position as `{ x, y, z }`.
- **`target_room_num`**: 1-based room number of the camera target, or `nil` if unknown.

### Functions

- [lua]`trx.camera.shake(intensity)`  
  Sets camera shake intensity by updating the camera bounce value.  
  Positive values shake the camera upward; negative values shake it downward.  
  Example:
  ```lua
  trx.camera.shake(200)
  ```
