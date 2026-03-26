---
title: Getting started
order: 0
---

## Getting started

You'll need:

- A TR1 or TR2 TRX build from the latest develop branch that adds Lua scripting.
- Familiarity with the game flow JSON format.

### Quick steps

Add per-level scripts in a level object:

```json5
{
    "levels": [
        // …,
        {
            "script": "data/scripts/level1.lua",
            // …,
        },
        // …
    ],
    // …
}
```

Create a file `data/scripts/level1.lua` folder in your project and put the
following content:

```lua
trx.events.after_level_state(function(level)
  trx.log.info("hello from level 1!")
end)
```

Start the game. In the logs, you should see the following:

```
INF | 2025-10-04 12:12:23.155 [data/scripts/level1.lua:2:?] hello from level 1!
```

---

Optionally, you can also load a global script by adding a global property to
your game flow configuration:

```json5
{
    // Optional global Lua script file
    "main_script": "data/scripts/global.lua",
}
```

### Interactive commands

You can also try out short lua commands in-game with the `/lua` console command:

```text
/lua
```
