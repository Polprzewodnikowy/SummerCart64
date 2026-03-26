---
title: User configuration
order: 4
---

## User Configuration

TRX allows the players to configure the game to their taste. The ingame setting
dialogs write to `cfg/TR1X.json5` and `cfg/TR2X.json5`. As a level builder, you
may however wish to enforce some settings to match how your level is designed.

As an example, let's say you do not wish to add save crystals to your level, and
as a result you wish to prevent the player from enabling that option in the
config tool. To achieve this, open `cfg/tr1/gameflow.json5` in a suitable text
editor and add the following.

```json5
{
  // …
  "enforced_config": {
    "enable_save_crystals": false,
  }
}
```

This means that the game will enforce your chosen value for this particular
config setting. If the player tries to edit the settings, the option to toggle
save crystals will be disabled.

You can add as many settings within the `enforced_config` section as needed.
Refer to the key names within `cfg/TR1X.json5` and `cfg/TR2X.json5` for
reference.

Note that you do not need to ship a full configuration with your level, and
indeed it is not recommended to do so if you have, for example, your own custom
keyboard or controller layouts defined.

If you do not have any requirement to enforce settings, you can omit the
`enforced_config` section from your game flow altogether.

### Hiding settings from the in-game UI

If you prefer to hide certain configuration options entirely (rather than
merely enforce their values), you can add a `hidden_config` section in your
game flow JSON. Any setting listed here will be omitted from the settings
dialogs. For example:

```json5
{
  // …
  "hidden_config": [
    "enable_legal",
    "enable_save_crystals",
  ]
}
```

When all settings in a given tab are hidden, that tab will also be removed from
the UI.
