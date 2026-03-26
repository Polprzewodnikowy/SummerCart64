---
title: Bonus levels
order: 5
---

## Bonus levels

The game flow supports bonus levels, which are unlocked only when the player
collects all secrets in the game's normal levels. These bonus levels behave just
like normal levels, so you can include FMVs, cutscenes in-between and so on.

Statistics are maintained separately, so normal end-game statistics are shown
once, and then separate bonus level statistics are shown on completion of those
levels.

Following is a sample level configuration with three normal levels and two bonus
levels. After the end-game credits are played following level 3, if the player
has collected all secrets, they will then be taken to level 4. Otherwise, the
game will exit to title.

<details>
<summary>Show example setup</summary>

```json5
{
    "levels": [
        {
            // gym level definition
        },

        {
            "path": "data/level1.phd",
            "music_track": 57,
            "sequence": [
                {"type": "loop_game"},
                {"type": "level_stats"},
                {"type": "level_complete"},
            ],
        },

        {
            "path": "data/level2.phd",
            "music_track": 57,
            "sequence": [
                {"type": "loop_game"},
                {"type": "level_stats"},
                {"type": "level_complete"},
            ],
        },

        {
            "path": "data/level3.phd",
            "music_track": 57,
            "sequence": [
                {"type": "loop_game"},
                {"type": "level_stats"},
                {"type": "play_music", "music_track": 19},
                {"type": "display_picture", "path": "data/end.pcx", "display_time": 7.5},
                {"type": "display_picture", "path": "data/cred1.pcx", "display_time": 7.5},
                {"type": "display_picture", "path": "data/cred2.pcx", "display_time": 7.5},
                {"type": "display_picture", "path": "data/cred3.pcx", "display_time": 7.5},
                {"type": "total_stats", "background_path": "data/install.pcx"},
                {"type": "level_complete"},
            ],
        },

        {
            "path": "data/bonus1.phd",
            "type": "bonus",
            "music_track": 57,
            "sequence": [
                {"type": "play_fmv", "fmv_path": "snow.avi"},
                {"type": "loop_game"},
                {"type": "play_cutscene", "cutscene_id": 0},
                {"type": "level_stats"},
                {"type": "level_complete"},
            ],
        },

        {
            "path": "data/bonus2.phd",
            "type": "bonus",
            "music_track": 57,
            "sequence": [
                {"type": "loop_game"},
                {"type": "level_stats"},
                {"type": "play_music", "music_track": 14},
                {"type": "total_stats", "background_path": "data/install.pcx"},
                {"type": "exit_to_title"},
            ],
        },
    ],

    "cutscenes": [
        {
            "path": "data/bonuscut1.phd",
            "music_track": 23,
            "sequence": [
                {"type": "loop_game"},
            ],
        },
    ],
}
```
</details>
