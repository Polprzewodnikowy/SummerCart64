---
title: Commands
order: 3
---

## Game flow commands

The command allows you to modify the original game flow, but please note that
deviations from the original script may result in unexpected behavior. If you
encounter any bugs, we encourage you to report your experience by opening an
issue on GitHub. The overall structure is as follows:

```json5
{
  "command": "play_level",
  "param": 5,
}
```

Currently the following commands are available.

<table>
  <tr>
    <th>Command</th>
    <th>Description</th>
    <th>Parameter</th>
  </tr>
  <tr>
    <td><code>noop</code></td>
    <td>Continue the flow as normal.</td>
    <td>N/A</td>
  </tr>
  <tr>
    <td><code>play_level</code></td>
    <td>Play a specific level.</td>
    <td>Level to play.</td>
  </tr>
  <tr>
    <td><code>load_saved_game</code></td>
    <td>Load a specific savegame.</td>
    <td>Save slot number to use</td>
  </tr>
  <tr>
    <td><code>play_cutscene</code></td>
    <td>Play a specific cutscene.</td>
    <td>Cutscene number to play</td>
  </tr>
  <tr>
    <td><code>play_demo</code></td>
    <td>Play a specific demo.</td>
    <td>Demo number to play.</td>
  </tr>
  <tr>
    <td><code>play_fmv</code></td>
    <td>Play a specific movie.</td>
    <td>Movie number to play.</td>
  </tr>
  <tr>
    <td><code>exit_to_title</code></td>
    <td>Return the game to the title screen.</td>
    <td>N/A</td>
  </tr>
  <tr>
    <td><code>level_complete</code></td>
    <td>
      End the current sequence inside level sequences, do nothing otherwise.
    </td>
    <td>N/A</td>
  </tr>
  <tr>
    <td><code>exit_game</code></td>
    <td>Exit the game to desktop.</td>
    <td>N/A</td>
  </tr>
  <tr>
    <td><code>select_level</code></td>
    <td>Play a specific level (and reset inventory).</td>
    <td>Level number to play.</td>
  </tr>
  <tr>
    <td><code>restart_level</code>¹</td>
    <td>Restart the currently played level.</td>
    <td>N/A</td>
  </tr>
  <tr>
    <td><code>story_so_far</code>¹</td>
    <td>Play the movies and cutscenes up until the currently played level.</td>
    <td>Save slot number to use</td>
  </tr>
</table>

**¹** Tomb Raider 1 only.

Additional notes:
- All numbers (levels, cutscenes, ...) start with 0.
