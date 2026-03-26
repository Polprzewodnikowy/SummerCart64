---
title: CLI options
order: 1
---

# Command line options

Currently the following command line interface options are available:

- `-g`, `--gold` (legacy: `-gold`)  
  Runs the Unfinished Business or the Golden Mask expansion pack, depending
  on the game.

- `--demo-pc` (legacy: `-demo_pc`) (TR1 only)  
  Runs the PC demo level.

- `-l <path|num>`, `--level <path|num>`  
  Runs the game immediately launching it into the specified level. If `<path>`
  is provided, runs the custom level located in the specified location, which
  should be absolute. Internally, this option uses `tr*-level/gameflow.json5`
  as a template instructing it how to run the game. If `<num>` is an integer,
  plays the level with the given number within the main game flow (1-based).

- `-s <num>`, `--save <num>`:  
  Runs the game immediately loading a specific save slot. The first save starts
  at `num=1`. This option can be combined with `-l`/`--level`.

- `--test-record <PATH>`:  
  Records gameplay events to an external text file.

  `--test-replay <PATH>`, `--test-play <PATH>`:  
  Replays gameplay events from an external text file.

  `--headless`:  
  Runs the game in command line only. Only available with `--test-replay`.

- `--headless-fps <num>`:  
  In headless mode, forces the simulation to run at a constant FPS. If omitted
  or zero, uses the FPS from the configuration.

- `--debug-render-performance`:  
  Outputs diagnostic information related to GPU usage and throughput.

- `-q`, `--quiet`  
  Suppresses most of output to the standard output, keeping only errors.
  The log file is written to normally.

> [!NOTE]
> Gameplay capture is considered an internal testing tool, and may be
> subject to breaking changes without warnings.
