---
title: Game
order: 11
---

## Game module

Module for retrieving game version and level tables.

### Structures

- [lua]`trx.game.Level`

    Represents a level entry.

    Properties:

    - **`num`**: Integer ordinal number of the level.
    - **`name`**: String title of the level.
    - **`path`**: String file path of the level data.
    - **`type`**: Integer type identifier of the level.

- [lua]`trx.game.Settings`

    Represents global engine settings.

    Properties:
    - **`lockout_option_ring`**: Whether to disallow the player from using the option ring in-game.
    - **`load_save_disabled`**: Whether to disable saving and loading the game.
    - **`play_any_level`**: Whether to show a full list of all levels in place of the New Game passport page.
    - **`demo_delay`**: The number of seconds to pass in the main menu before playing the demo.
    - **`cheat_keys`**: Whether to enable original game cheats (the ones where Lara turns around three times).

    Writable properties:
    - `lockout_option_ring`
    - `load_save_disabled`
    - `play_any_level`
    - `demo_delay`
    - `cheat_keys`

### Functions

- [lua]`trx.game.version`  
  Returns the current game version integer. This is guessed from the level data.
- [lua]`trx.game.trx_version`  
  Returns the current TRX version string.

- [lua]`trx.game.current_level`  
  Retrieves the [lua]`trx.game.Level` that's currently loaded or `nil`.

- [lua]`#trx.game.settings`  
  Accesses the global engine settings.

- [lua]`#trx.game.levels`  
  [lua]`#trx.game.demos`  
  [lua]`#trx.game.cutscenes`  
  Returns the number of levels of the specific type.

- [lua]`trx.game.levels[num]`  
  [lua]`trx.game.demos[num]`  
  [lua]`trx.game.cutscenes[num]`  
  Retrieves the [lua]`trx.game.Level` of the specific type at the given index,
  or `nil` if out of range.

- [lua]`trx.game.play_level(num)`  
  Plays the specified level via game flow override or errors if invalid.
  If the Gym level is available, it's the level 1.
- [lua]`trx.game.play_cutscene(num)`  
  Plays the specified cutscene via game flow override or errors if invalid.
- [lua]`trx.game.play_demo(num)`  
  Plays the specified demo via game flow override or errors if invalid.
