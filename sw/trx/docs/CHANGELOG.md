## [Unreleased](https://github.com/LostArtefacts/TRX/compare/trx-1.4.2...develop) - ××××-××-××
- added remembering of the last played mod
- added a new console command, `/tp enemy`, to cycle Lara through hostile creatures in the current level
- added a new animation command, `ITEM_ACTION_TURN_90`, which will rotate the affected item 90°
- added dynamic mod discovery from the games/ directory using new `extends` and `name` fields in gameflow.json5
- changed `--level` to no longer require `-e/--engine` to work

**TR2**:
- changed the Detonator Box to no longer hard-code dynamic light output; refer to the migration guide for custom levels

**TR3**:
- added Security Laser (Alarm) control
- added Security Laser (Damage) control
- added Security Laser (Kill) control
- added Sentry Gun control
- added Civilian control
- added Detonator Box control
- added Prisoner control
- added MP 1 control
- added MP 2 control
- restored the cutscene at the beginning of High Security Compound



## [1.4.2](https://github.com/LostArtefacts/TRX/compare/trx-1.4.1...trx-1.4.2) - 2026-03-24
- fixed 3D pickups and inventory ring view still affected by fog (regression from 1.4)

**TR2**:
- fixed the Detonator Box being difficult to activate when selecting the key manually from the inventory (#5215, regression from TR2X 1.3)
- fixed the Detonator Box rotating if Lara interacts with it but then doesn't pick the key from the inventory (#5215, regression from TR2X 1.3)



## [1.4.1](https://github.com/LostArtefacts/TRX/compare/trx-1.4...trx-1.4.1) - 2026-03-23
- fixed Lara using the ladder-to-crouch animation in some rare cases despite there being headroom in front of her (regression from 1.2)
- fixed toggle-sprint key failing to cancel sprint mid-run (#5174, regression from 1.4)
- fixed toggle-duck key failing to keep Lara ducked in run-to-duck and sprint-to-duck paths (#5177, regression from 1.4)
- fixed flipped state of "Pause music in inventory", changed to "Enable music in inventory" (regression from 1.4)
- fixed final statistics in City of Khamoon counting the optional PS1 mummy when Restore PS1 enemies is disabled (#5188, regression from 1.1)
- fixed bouncy grenades getting stuck in certain geometry (#5202, regression from 1.2)
- fixed thrown flares getting stuck in certain geometry (#5202, regression from 1.4)

**TR3**:
- fixed door 34 in Thames Wharf closing permanently during the flipmap puzzle (#5170, regression from 1.1)
- fixed Lara being unable to move after grabbing ladders in specific geometry (#5169, regression from 1.1)
- fixed a missing camera shake effect in room 135 in Aldwych (#5183)
- fixed a missing sound effect during the flip map in the Egyptian room in Lud's Gate (#5183)
- fixed potential framerate drops during audio playback when no active sound effects are playing (regression from 1.3)
- fixed some enemies automatically being hostile when triggered if other enemies have been killed (#5203, regression from 1.1)



## [1.4](https://github.com/LostArtefacts/TRX/compare/trx-1.3.1...trx-1.4) - 2026-03-21
Showcase: https://youtu.be/8SavYv2SawI
- added an option to let Lara stay crouched without holding the button (Gameplay → Controls → Toggle crouch) (#5006)
- added an option to let Lara keep sprinting without holding the button (Gameplay → Controls → Toggle sprint) (#5006)
- added three additional outfits for Lara
- added a new console command, `/mod {name}`, to switch between installed game/mod packs without relaunching
- added a new option in the New Game dialog, "Switch Game", to switch between installed game/mod packs without relaunching
- added experimental support for config presets (Gameplay Options → Presets)
     Currently very basic presets available only – looking for help with improving them :)
- added new backgrounds to Inventory Ring / Pause screen / Stats screen styles:
    - Transparent: like TR2 PS1 Pause Screen
    - Black: like the Remasters
    - Monochrome (cool): like TR3 PS1 Inventory Screen
    - Monochrome (warm): like TR3 PS1 Pause Screen
- added support for TR4-style trigger-triggerers (named `O_TRIGGER_GATE` in TRX) for custom levels
- added support for `.wma` music files for broader custom level compatibility
- added support for `.ogv` and `.fmv` FMV extensions, with `.ogv` preferred over `.fmv` for remaster compatibility
- added audio fallback for FMV files that lack an audio stream (e.g. remastered `.ogv`), probing alternative extensions for a companion audio track
- added an option to move Ammo counter location (Graphic Options → UI → Ammo counter location) (#5076)
- added simple level-load caching by introducing a `cache/` folder
- added an option for Lara to wear semi-transparent sunglasses (Graphic Options → Lara's sunglasses)
- added four additional general animating object slots, `O_ANIMATING_7` to `O_ANIMATING_10`
- added `O_FLICKERING_LIGHT`, which is similar to `O_ELECTRICAL_LIGHT` but is permanently flickering
- changed the reflections option to be available in all game modes (Graphic Options → Enable reflections)
- changed the delay in performing a running jump by one frame less, when jump lock mode is set to disabled (Gameplay → Controls → Jump lock mode) (#3841)
- improved rendering line segments (poison darts, rain drops, SWAT laser sights)
- improved level loading times by 15%
- improved FMV audio to play through the game's existing audio mixer instead of opening a separate audio device
- fixed High lighting contrast not attenuating brightness properly in TR1 and TR2 (regression from 1.1)
- fixed the photo mode red frame not covering the full screen when using integer upscaling
- fixed boulders that have moved vertically reactivating for a frame after loading a save (regression from 1.2)
- fixed low fog distances affecting 3D pickups and inventory ring view
- fixed cheat and weapon hotkeys affecting gameplay during demos (#5163, regression from 1.0)

**TR1**:
- added the ability to use flames on Pendulums, similar to TR3
- changed skyboxes in TR1 to be drawn only if the appropriate room flag is set
- changed the scion pickup in Sanctuary of the Scion to not be displayed on-screen briefly before the level ends (#3682)
- fixed Scion taking damage before activation (regression from 1.0)

**TR2**:
- added the ability to use flames on Pendulums, similar to TR3
- changed demos to show accurate gun meshes before Lara draws her pre-selected weapon (#3585)
- fixed Bartoli appearing frozen towards the end of the Opera House cutscene
- fixed pulling the Dagger of Xian from dragon's corpse not counting as a pickup (regression from 1.0)
- fixed thrown flares falling through trapdoors and becoming stuck in the void if thrown underwater near the floor (#3708)
- fixed flamethrowers and Dragon's breath doing weird animation when hitting floor (#5104, regression from 1.3)
- fixed yetis dealing no damage during their charge attack (#5126, regression from TR2X 0.8)

**TR3**:
- added UPV control
- added Train control
- added Patrol Dog control
- added Crow control
- added Sophia control
- added Fuse Box control
- added Electric Cleaner control
- added Punk control, including `O_PUNK_2`, which was unused in OG
- added Security Guard control
- added Security Laser control
- added Propeller control
- added SWAT control
- added Diver control
- added Pendulum control
- added 60 FPS interpolation to:
    - sparks
    - weather effects
    - water effects
    - wake effects
    - explosion rings
    - bat emitters
- changed Ammo Counter to appear in red when the Menu Style is set to PS1
- changed Punks to have friendliness assignable through Lua, so removing the hard-coded behaviour in the Lud's Gate level sequence
- changed Trains to no longer hard-code speed based on the level number and instead take it from their default animation
- changed Pendulums that have flames to be setup by placing a flame emitter at the same position, rather than setting the item's timer via its trigger
- changed Meteorite Artefacts to be exempted from drop tile centering
- fixed a soft lock preventing Lara from picking up the artefact, when saving/loading during boss explosion sequence (regression from 1.2)
- fixed the helicopter in Highland Fling briefly disappearing when crossing room portals (regression from 1.1)
- fixed Lara dying from touching Trains that haven't yet been activated
- fixed harpoons from Divers not spawning blood when they hit Lara
- fixed `O_KILL_ALL_TRIGGERED` removing unused Save Crystals (#5035)
- fixed TR1/TR2-only options showing up in TR3 gameplay settings (#5055)
- fixed Lara stopping against one-click raised slopes when running instead of beginning to slide (#5038)
- fixed rain not spawning in outside rooms in the Thames Wharf cutscene
- added PSX-style underwater water particles to Madubu Gorge, Aldwych, and Lud's Gate
- fixed the punk in the cutscene before Lud's Gate walking through a wall
- fixed Lara appearing frozen at the beginning of the cutscene before City
- fixed incorrect texturing on the fish in City
- fixed the Eye of Isis not showing in the inventory in All Hallows
- fixed too low volume in all FMVs (except logo which used a different codec)
- fixed Lara by default being unable to climb out of water onto steep slopes (change manually in Gameplay → Fixes → Fix water exit)
- fixed thrown flares falling through trapdoors (regression from 1.1)
- fixed some level textures appearing slightly misaligned on room geometry
- fixed potential AI behavioural differences in the South Pacific Mercenary (regression from 1.2)
- fixed smoke from Lara's guns persisting between levels (regression from 1.1)
- fixed sound effects potentially playing after completing a level and entering into the globe select screen (regression from 1.2)
- fixed audio lag and framerate drops near the end of an audio track that's playing when no active sound effects are playing (#5167, regression from 1.3)



## [1.3.1](https://github.com/LostArtefacts/TRX/compare/trx-1.3...trx-1.3.1) - 2026-03-11
- fixed main.sfx resolution being enforced (regression from 1.3)
- fixed the microphone entering underwater mode too eagerly when `Microphone near Lara` is enabled (#5057, #4888)
- fixed save counters sometimes drifting after dying and reloading (#5054, regression from TR1X 4.9 / TRX 1.0)
- fixed flare and gun flash being drawn with a water tint when in shallow water regardless of responsive tint option (#5072, regression from 1.2)
- fixed fade transitions using the wrong picture size when upscaling or borders are enabled (#5081, regression)

**TR2**:
- fixed guns as secret rewards not being converted to the equivalent ammo if Lara already has the gun

**TR3**:
- fixed reverb affecting inventory ring sounds (#5056)
- fixed Pause text color
- fixed the secret sound not playing in some installations, whereby `cdaudio.wad` contains invalid track sizes (#5049)
- fixed mounting a UPV causing Lara's braid to stand upright (OG)



## [1.3](https://github.com/LostArtefacts/TRX/compare/trx-1.2.2...trx-1.3) - 2026-03-06
Showcase: https://youtu.be/FgB9JgDM65E
- added the ability to freely rotate examinable items
- added a color editor dialog for fog and water colors in Graphic Options → Visuals
- added an option for Lara to wear sunglasses (Graphic Options → Visuals → Sunglasses) (#4869)
- added `O_SWITCH_TYPE_WHEEL`, which is similar to `O_SWITCH_TYPE_AIRLOCK` but can be used more than once
- added `O_SMASH_OBJECT_3`, which can only be broken with triggers or the Crash Site gun
- added `O_SMASH_OBJECT_4`, which behaves like `O_SMASH_OBJECT_1` but uses `SFX_SHUTTERS_BREAK`
- added `O_TREX_ALPHA`, which can target raptors and be distracted by flares
- added the ability to trigger dragons independently of Bartoli in custom levels (#5011)
  - place an `O_DRAGON_BACK` item in the editor and trigger it normally
  - the dragon will spawn immediately when triggered and will be one-phase, so no dagger needs to be pulled
- added a new Lua item query helpers, `trx.items.find(query)` and `trx.items.first(query)`, with support for `object_id` and `room_num` filters
- added a new Lua catalog, `trx.catalog.weapons`, for weapon identifiers
- added a new Lua property, `trx.lara.equipped_gun`, to read Lara's currently equipped gun type
- added a new Lua property, `trx.lara.target`, to read Lara's current locked target item
- added a new Lua property, `trx.Item.flags`, to read current item flags (related to triggers)
- added a new Lua property, `trx.Item.timer`, to read current item timer value (related to triggers)
- added support for using more sound slots than originally possible in custom levels (#3898)
- added support for actual quick saves with round-robin quicksave slot cycling. (#1897)  
  Note: This feature is disabled by default and needs the player to manually bind new inputs.
- added quick-save/load command aliases:
  - `/save quick`, `/quicksave`, `/qs`
  - `/load quick [slot]`, `/load q[slot]`, `/quickload [slot]`, `/ql [slot]`
- added blood effects when enemies shoot any other creature (not just Lara)
- added support for the globe-style level selection mechanic in the new game for level builders (#4920)
- added an option to control how Lara swings on thin ledges (Gameplay → Controls → Slow ledge swing) (#3341)
- added `/tp precise {x} {y} {z}` to teleport using raw world-space coordinates (no `/1024` scaling – matches TRView)
- added the ability to use glide cameras when using TR3 camera mode
- added an option to toggle glide cameras (Graphic Options → Visuals → Glide cameras)
- changed PC and PS1 UI colors to no longer be hardcoded by moving it to `ui.json5` (#5003)
- changed Fog start and Fog end to change by 10 by default, with Slow allowing 1-step precision (#5015)
- changed `O_WINDOW_1` and `O_WINDOW_2` to `O_SMASH_OBJECT_1` and `O_SMASH_OBJECT_2` respectively
- changed `O_MINI_COPTER` to no longer hardcode direction
- changed Earthquake to support being reset
- changed loading screens setting to use modes (`disabled`, `always`, `new-games`). Previously, they were hardcoded to not show for saves (#1290)
- changed logs to no longer emit ANSI color characters when the game's output is piped to a file / process
- changed the degenerate static mesh collision check to only apply when all axes have an empty size
- improved error reporting for gameflow issues to now display full key paths for faulty nodes
- fixed Lara teleporting after vaulting 2 or 3 clicks when there is a room below the target position that has no immediately adjoining portal (#4530)
- fixed Lara attempting to jump up (using action) despite the ceiling above her making it impossible to grab any ledge (#3558)
- fixed Lara not being able to grab ledges when under low ceilings (#4093)
- fixed Lara sometimes falling when vaulting 2 or 3 clicks onto a ledge that has triangulation
- fixed NG+ always forcing Lara's default equipped gun at level start even when "remember guns between levels" is enabled (#4711)
- fixed not restoring Lara's back weapon mesh between levels when "remember guns" is enabled and a rifle-type weapon is equipped at level end
- fixed a missing footstep sound when Lara starts to sprint
- fixed Lara's flare undraw animation being skippable on specific late draw frames (#1593)
- fixed UI bar scale option not updating the padding and borders (regression from 1.2)
- fixed Blade stopping in the wrong position when anti-triggered (#4894)
- fixed very distant Boulders causing camera shake (similar to the Tihocan crocodile targeting bug)
- fixed drawing debug triggers using wrong orientation in some triangular geometry
- fixed heavy triggers with no `TO_TARGET` / `TO_CAMERA` resetting cameras
- fixed Lua `trx.catalog` only exposing `objects` and `flip_effects`; it now also exposes `lara_states`, `lara_anims`, `music`, and `samples`
- fixed a freeze if firing a grenade very close to room portals (#4938, regression from 1.2)
- fixed non-deterministic Inventory Ring control (transition speeds depended on v-sync / wall clock timing)
- fixed game logic speeding up while the game was fading out after quitting
- fixed Lara being able to shoot smashable objects located in unreachable overlapping rooms (#4949, regression from TR1X 4.14 / TR2X 1.4)
- fixed touching Lava Wedges causing endless Flame effect spawns when the immunity cheat is on
- fixed touching Lava tiles causing reduced Flame effect when the immunity cheat is on
- fixed collision issues on bridges, trapdoors, breakable tiles and pushblocks if positioned over a triangle portal (regression from 1.0)
- fixed Lara being able to sprint through swamps when responsive sprinting is enabled
- fixed bar borders scaling poorly (off-by-1px errors, regression from 1.2)
- fixed death counter not being preserved in saves after changing levels, causing stopwatch and final statistics to sometimes show 0 deaths

**TR1**:
- added an option to allow Lara to crouch and crawl (Gameplay → Controls → Crawling)
- added support for monkey bars
- changed Lara to be able to grab ealier when performing forward jumps, like TR3
- fixed a very rare case of raptors using an incorrect death animation
- fixed Lara unable to run around in random spots at the bottom of The Great Pyramid's starting pit

**TR2**:
- added an option to allow Lara to crouch and crawl (Gameplay → Controls → Crawling)
- added support for monkey bars
- changed Lara to be able to grab ealier when performing forward jumps, like TR3
- fixed secret reward in Venice giving Magnums ammo instead of Automatic Pistol Clips (#4951, regression from 1.1)
- fixed flickering switches and spike ceilings in Temple of Xian and Floating Islands (#4874)
- fixed Airlock door handles not getting drawn from certain angles (#4886, regression from 1.0)
- fixed loading screens showing before playing FMVs on most levels
- fixed Lara not being able to move after exiting water, having used an underwater lever with the animated interactions setting enabled (#4912, regression from 1.0)
- fixed Bell in room 48 being shootable from room 55 (#4949, regression from TR2X 1.4)
- fixed "Disable T-Rex Collision" option missing from The Golden Mask (there are T-Rex enemies in Nightmare in Vegas)
- removed the requirement to use `main.sfx` in custom levels (#3898)

**TR3**:
- added reverb support
- added Kayak control
- added Compsognathus control
- added Mounted Gun control
- added Tribe Axeman control
- added Tribe Pipeman control
- added Tribe Boss control
- added Lizard control
- added Crocodile control
- added Carcass control (hanging Raptor)
- added T-Rex control
- added Raptor control
- added Raptor Emitter control
- added Bat Emitter control with save/load support
- added South Pacific Mercenary control
- added Smashable Wall control
- added Smashable Shutters control
- added a slide-to-sprint animation state change for Lara, similar to TR1 and TR2
- added a new gameplay option to toggle Lara's crouch roll (Gameplay → Controls → Crouch roll)
- added an option to allow Lara to jump out of crawlspaces (Gameplay → Controls → Crawl exit jump)
- added crouching/crawling enhancements (Gameplay → Controls → Responsive crawling)
  - added the ability to resume crawling more quickly after coming to a stop
  - added transitions from run/sprint to crawl without first coming to a stop
  - added a transition from crawl to crouch-roll without having to manually crouch first
  - added the ability to turn while in the crouch idle state
  - restored an unused pickup animation when in the crawling state, bypassing the crouch transition
- added a transition from ladder to crawlspaces instead of first having to drop and re-grab the ladder (#4954)
- fixed Uzis having wrong clips capacity (was 80, is now 40 – sorry!)
- fixed Lara briefly switching from run back to wade when crossing from 2-click to 1-click water depth
- fixed Lara unable to climb small ledges with low crawlspaces
- fixed Lara using the thin-ledge swing hang animation instead of the normal hang in some 1-click ledge cases
- fixed Lara being unable to transition from slow swing at the base of a ladder to being able to climb the ladder
- fixed Lara's cutscene gun shots not rendering muzzle flashes, gun smoke and shell ejections (e.g., Tony cutscene)
- fixed water ripples triggering z-fighting with 0-click ground surfaces
- fixed footprints rendering with an excessive Y offset
- fixed wheel switches only being usable once
- fixed wheel switch triggers activating too early
- fixed Kayak voiding and teleporting on large slopes
- fixed Kayak wake effects sometimes clipping through complex geometry
- fixed loading screens showing before playing FMVs in Antarctica
- fixed end credits referencing non-existing image file
- fixed Puna to no longer hardcode Lizard locations, and instead use relative offsets
- fixed Puna's summoned Lizards counting towards total level kill count
- fixed Tony briefly appearing for a single frame when loading a save after his death
- fixed Lara sometimes getting stuck when crawling backwards off a tilted ledge (#4956)
- fixed the Tribe Pipeman sometimes not being able to aim darts at Lara correctly (Gameplay → Fixes → Fix Pipeman aim)
- fixed Lara's footprints sometimes spawning when standing on a bridge, trapdoor or pushblock
- fixed Lara being unable to walk or sidestep at times when standing on a bridge that sits over a steep slope (regression from 1.1)
- fixed Lara's left arm elevating when holding a flare and performing a crouch pickup (regression from 1.1)
- removed the limitation of one Carcass instance per level working with Piranhas
- removed the limitation of Piranhas only attacking Carcass instances if the level sequence matches Crash Site's
- restored the ability for Lara to perform grab cancels, like TR1 and TR2
- restored glide camera functionality



## [1.2.2](https://github.com/LostArtefacts/TRX/compare/trx-1.2.1...trx-1.2.2) - 2026-02-13
- fixed a potential `GL_OUT_OF_MEMORY` error that could occur after reloading levels many times (regression from <1.0)



## [1.2.1](https://github.com/LostArtefacts/TRX/compare/trx-1.2...trx-1.2.1) - 2026-02-11
- fixed title ring music inheriting the wrong audio volume (regression from 1.2)
- fixed settings dialog changing size when cycling through non-scrollable tabs (regression from 1.2)
- fixed Play Previous Level feature not restoring Lara's equipment correctly for pre-1.2 saves (regression from 1.2)
- fixed Play Previous Level feature causing Lara to instantly die for pre-1.2 saves, when the Persistent Damage option is on (regression from 1.2)
    Note: for those 1.0/1.1 saves, this feature will restore her health to full, as it was not stored correctly. 1.2 will continue to restore the correct HP value.
- fixed TR2 delayed music triggers not working (regression from 1.1)
- fixed TR3 using delayed music triggers (TR2-only feature)



## [1.2](https://github.com/LostArtefacts/TRX/compare/trx-1.1...trx-1.2) - 2026-02-11
Showcase: https://www.youtube.com/watch?v=jeq8rQONaic
- added globe level selection mechanic
- added Bubble Emitter control (#4629)
- added dynamic light objects:
    - added Red Light control
    - added Green Light control
    - added Blue Light control
    - added Amber Light control
    - added White Light control
    - added Strobe Light control
    - added Pulse Light control
    - added Beacon Light control
    - added On/Off Light control
- added the ability in Lua to hook into control loop events during cutscenes
- added an option to change Lara's outfit, with 20 variants included by default; custom levels can provide up to 32 outfits (Visuals → General → Lara's outfit) (#4383)
- added an option to control UI brightness (Graphic Options → Rendering → UI brightness); renamed "Brightness" to "Game brightness"
- added an option to allow Lara's underwater mesh tint to be more responsive based on position, as per TR3 (Visuals → General → Responsive mesh tint)
- added the ability for custom levels to define Lara's braid position relative to her head (#110)
- added the ability to disable manual camera (Gameplay → Controls → Manual camera)
- added the ability to enable bouncy grenades (Gameplay → General → Enable bouncy grenades)
- added the ability to toggle TR1/2 and TR3 projectile area damage – TR3 often deals double damage (Gameplay → Mods → Projectile Area Damage)
- added the ability to hide pickup notifications in the bottom-right corner (Graphic Options → UI → Pickups overlay)
- added a new Lua event, `trx.events.on_game_start`, which fires when the level finishes loading and the game is about to start
- added a new Lua function, `trx.rooms.flip()`, to toggle the flip map (#4704)
- added a new Lua function, `trx.rooms.flip_effect()`, to set the active flip effect with an optional timer (#4704)
- added a new Lua catalog, `trx.catalog.flip_effects` for name-based flip effect catalog IDs
- added a new Lua music play mode, `trx.music.PlayMode.OVERLAY` for playing on top of currently played track
- added new Lua catalogs for Lara states, Lara anims, music, and samples
- added a new Lua module, `trx.camera`, with camera getters and `trx.camera.shake()`
- added a new Lua property, `trx.rooms.Room.num`
- added support for cross-fades to the title screen
- added visual previews of bar colors (Graphic Options → Bars)
- added the ability to change PS1 bar colors
- added shadow rendering to all cutscene actors
- added endless sprint (available previously via the `/restless` command) to the UI settings (Gameplay options → Mods → Endless sprint)
- added endless flare time cheat (Gameplay options → Mods → Endless flare time)
- added `O_VULTURE` for custom levels
- added `O_ROLLING_BALL_4` (giant Temple of Puna boulder) for custom levels
- added an option to control whether or not moving boulders should shake the camera (Gameplay options → General → Enable boulder shake)
- added an option to make Lara stumble if she hops backwards and there is a slope behind her (Gameplay options → Controls → Backwards slope stumble)
- added `/trigger` and `/untrigger` console commands, with support for targeting by item ID, item name, or object name
- added the ability to seek backwards through cutscenes with left button
- added the ability to trigger collapsible tiles from heavy triggers, regardless of Lara's position (#4807)
- added floor height change detection for boulders when stopped, so they will drop if the floor below them drops (#4808)
- added splash effects to neutral twists and rolls (#4793)
- improved rendering performance
- improved the ability to seek through cutscenes to support even faster seeks (Slow = ±1 s, default = ±5 s, new: Draw = ±15 s)
- improved `/tp` to accept `room`/`item` prefixes and `rN`/`iN` shortcuts
- improved inventory ring active item highlight for smoother appearance
- improved savegame file size by reducing it about 20–30%.
- improved indentation for nested bullets in the UIs
- changed `debug.debug_cuboids` option name from "debug cuboids" to "debug bounding boxes" (`/debug bounding-boxes` or `/set debug-bounding-boxes 1`)
- changed `debug.enable_debug_pos` option to split into `enable_debug_pos` and `enable_debug_anim`
- changed `debug.enable_invulnerability` option to only show the marker if the setting `enable_debug_status` is on (off by default) (#4631)
- changed `audio.load_music_triggers` (Gameplay → Fixes → Fix one-shot music triggers) to be enabled by default
- changed photo mode to no longer show "Entering photo mode" in the console
- changed photo mode to always display a red frame around the game view when active (not visible in screenshots)
- changed stats dialog to include allies in kill count if they turn hostile. This applies to all levels that follow, and the final stats screen.
- changed rooms-to-draw tracking to no longer stop at the 100-room limit
- changed boulders to stop if the ceiling height is lower than their height
- changed all UI bar colors from hardcoded to configurable via `cfg/ui.json5`, enabling some customization for PS1 bars
- changed `/debug [0|1]` command to no longer spam about settings that aren't changed
- changed `/set` command to always use hyphens for enum option values, and accept both underscores and hyphens
- changed Lua catalog keys to strip `O_` prefixes and use lowercase
- changed Lua event callback names to be more consistent:
    - `on_level_init` → `before_level_file`
    - `on_level_start` → `after_level_file`
    - `on_level_load` → `after_level_state`
    - `on_control` → `before_control`
    - `on_control_post` → `after_control`
- changed turbo cheat to auto‑reset to normal speed if pushed past limit, making it easier for new players to recover from accidental changes
- changed Blades to support being reset
- changed the barefoot SFX option toggle in TR2 to no longer require reloading the level for changes to take effect
- changed triggers that target pickup items to support antitriggers, switches and bitmasks
- removed support for legacy (TombATI / TR2 GOG/Steam) and pre-1.0 (TR1X/TR2X) savegame files
- fixed random face dropouts on levels with more than 32k textures
- fixed a small hiccup when launching the game on certain GPUs
- fixed inconsistent music volume in the statistics screens (#4499)
- fixed shadows to support 60 FPS interpolation
- fixed soft static mesh collision not working right with statics that appear in overlapping rooms
- fixed drawing debug triggers using random tint near water sources
- fixed drawing debug triggers glitching through triangular portals
- fixed Lara being force-resurfaced near split-triangle water portals in certain spots
- fixed custom levels that contain invalid room static mesh references not being able to load (#4770)
- fixed the tip of Lara's braid using an invalid offset position on the first frame of a level (#4821)
- fixed drawing shadows twice when item intersects a portal (#4640, regression from 1.0)
- fixed drawing circle/octagon shadows in TR2/TR3 cutscenes using wrong positions
- fixed being unable to use the manual camera in TR3 camera mode when Lara is idle (#4670, regression from 1.1)
- fixed grenades not killing more than a single enemy
- fixed running `/title` and similar commands leaving the "Examine" button briefly visible in the key items ring (old regression)
- fixed running `/title` and similar commands when examining an item causing incorrect item rotation next time the ring opens (old regression)
- fixed endless sprint cheat setting not retained between game relaunches
- fixed Cobras not being counted in level kill count
- fixed stats dialog retaining friendly status for allies that become enemy types in later levels, causing them to get excluded from kill count
- fixed targeting hostile ex-allies not working if "Enable ally targeting" option is off
- fixed `/play` and similar commands fading out instead of running instantly on stats/title screens
- fixed `/play` and similar commands sometimes preserving cutscene camera tilt if invoked while a cutscene was paused
- fixed Cheats description showing arrows in the indented bullets (#4753, regression from TRX 1.1)
- fixed game freezing on exit on certain platforms when there are no active sound devices (SDL bug)
- fixed Lara twitching when trying to step back onto death tiles
- fixed Lara's look head rotation/tilt limits being hardcoded to the engine version rather than camera mode
- fixed Lara rotating around an incorrect origin in photo mode during cutscenes
- fixed pushblocks being able to fall into rooms below despite no portals being present (#4788, regression from TR1X 4.15/TR2X 1.5)
- fixed one-shot triggers for hidden pickup items making the items permanently invisible (#4784)
- fixed secret tracks played at low quality when "fix secrets killing music" option is on
- fixed secret tracks not restored from the savegame when "fix secrets killing music" option is on
- fixed slow-forward seeking through cutscenes (right+slow) not working (regression from 1.0)
- fixed statics marked collidable but with zero‑size hitboxes causing phantom collisions
- fixed Lara being displaced during the sprint-slide animation if she tried to pick up an item at the same time (#4843, regression from TR1X 4.14, TR2X 1.4)

**TR1**:
- added Unfinished Business loading screens (#1310, thanks to rockahub)
- fixed save crystal reflections rendering upside down (regression from 4.14)
- fixed underwater wobble effect acting twitchy with camera movement
- fixed several texture issues on each of Lara's outfits and guns
- fixed gun injections overwriting Lara's footstep SFX in all levels (#4733, regression from 1.1)
- fixed pushblocks in Natla's Mines becoming unusable after loading a save made in earlier versions (#4735, regression from 1.1)
- fixed low-quality texture palette on injected TR2/3 weapons and flares
- fixed baddie speeches played at low quality when "fix speeches killing music" option is on
- fixed baddie speeches not restored from the savegame when "fix speeches killing music" option is on

**TR2**:
- added "Sound Options → Misc → Layered secret music" option
- added "Gameplay → Fixes → Fix one-shot music triggers" option
- changed Assault Course stats to show scroll indicators (#3510)
- changed statistics screen rows to be more compact
- fixed wrong line played when finishing the Assault Course for the first time (#4667, regression from 1.1)
- fixed underwater wobble effect acting twitchy with camera movement
- fixed several texture issues on each of Lara's outfits and guns
- fixed a deviation in water current behaviour that could result in Lara stopping too early (#4706, regression from TR2X 1.1)
- fixed gun injections overwriting Lara's footstep SFX in underwater levels (#4733, regression from 1.1)
- fixed exploding Armed Snowmobile not disappearing the vehicle (#4762)
- fixed the polar bear in Furnace of the Gods twitching if killed when in its reared state (#4624)
- fixed incorrect textures on the MP5 when equipped or on Lara's back

**TR3**:
- added "Sound Options → Misc → Layered secret music" option
- added "Gameplay → Fixes → Fix one-shot music triggers" option
- added new UI bar appearances, "TR3 PC" and "TR3 PS1" (Graphic Options → Bars → Bars appearance)
- added new water currents
- added new blood effects
- added underwater blood spills
- added poison mechanic
- added heal crystals
- added animated puzzle holes support
- added new creature explosions effects
- added meteorite artifacts support
- added examine item feature for certain items
- added Monkey control
- added Shiva control
- added Tony control
- added Spikes animation in Coastal Village and Madubu Gorge
- added Electric Fence control
- added Aldwych Drill control (Spike Ceiling with timer=1 to descend faster)
- added TR3 behavior patterns to Tiger control
- added Kill All Triggered control
- added Vulture control
- added Boulder control
- added Poison Dart control
- added Earthquake control
- added dynamic light objects:
    - added Red Light control
    - added Green Light control
    - added Blue Light control
    - added Amber Light control
    - added White Light control
    - added Strobe Light control
    - added Pulse Light control
    - added Beacon Light control
    - added On/Off Light control
- added Lara's backwards-hop stumble if there is a slope behind her
- added "Sound Options → Misc → Layered secret music" option
- improved look camera stability to reduce idle-breathing camera bobbing/roll
- improved Monkeys to no longer hardcode hostility status based on Tiger presence
- changed Assault Course stats to show scroll indicators (#3510)
- changed statistics screen rows to be more compact
- changed hostile Monkeys to share hostility status, like TR2 Barkhang monks (the original TR3 behavior can be restored in Gameplay → General → Ally hostility policy)
- changed enemy drops to appear at the tile center, to conform with the OG
- fixed several texture issues on each of Lara's outfits and guns
- fixed actors jumping to their start frame at the end of cutscenes
- fixed Flame in Cutscene 4 and 6 appearing static
- fixed Swamp Map rotation
- fixed seaweed disappearing too quickly in certain levels
- fixed Hand of Rathmore not rotating in Sleeping with the Fishes
- fixed Icicles not having sound
- fixed Spike Walls not having sound
- fixed colored exhaust smokes on Quad Bike for 1 frame
- fixed Cobras and Rattlesnakes being immune to explosives in their sleeping state
- fixed Quad Bikes not restoring their state from savegames properly
- fixed exploding Assault Targets in Lara's Home counting as penalties
- fixed surface and underwater effects simulation speed
- fixed underwater wobble effect amplitude
- fixed animated textures speed
- fixed inconsistent Meteor Artifacts names
- fixed wrong item selection sound in the inventory ring
- fixed flame emitters not getting restored when loading from a save
- fixed Lara holding onto ledges after dying if the Action key wasn't released
- fixed Shiva death smoke effects getting misplaced if the player saves and reloads mid-battle
- fixed Grenade, Rocket Launcher, and Harpoons damage
- fixed being unable to antitrigger Poison Dart Emitters
- fixed ally Lua API not working with most of the TR3 enemies supported so far
- fixed one-shot antitriggers / antipads behavior
- fixed Blades in Coastal Village not respecting antitrigger
- fixed some Poison Darts disappearing 1 frame early
- fixed running down an enemy with a Quad not counting as a kill
- fixed killing Cobras with a manually-aimed projectile not counting as a kill
- fixed smoke and spark rotation snapping at 180° instead of rotating smoothly
- fixed Lara burning instead of getting electrocuted when touching the top of the electric fence
- fixed driving over Winston with a Quad Bike or shooting him with the Harpoon Gun causing him to bleed
- fixed driving over Assault Target with a Quad Bike or shooting it with the Harpoon Gun causing it to spawn blood
- fixed skybox data in Scotland TR3:LA levels to show correct top and bottom colors



## [1.1](https://github.com/LostArtefacts/TRX/compare/trx-1.0.3...trx-1.1) - 2026-01-17
Showcase: https://www.youtube.com/watch?v=veVYyr--H1A
- added a fade-in and fade-out effect to patterned inventory backgrounds
- added the ability to use monochrome image for inventory and statistic screens backgrounds
- added the ability to use very dark image for inventory and statistic screens backgrounds (#4469)
- added the ability to change pause screen background
- added the ability to control whether or not allies are hostile towards Lara via Lua (#3873)
- added the ability to control via Lua which enemies are allies and which are ones that will fight with allies (#3873)
- added the ability to control Lara's air timer via Lua (#4592)
- added the ability to fine-tune the fade effects between the inventory ring, the pause screen, and the stats screen (Graphic Options → UI → Inventory/Pause/Stats fade effects)
- added gamma control (TR3-style) to all games (Graphic Options → Rendering → Gamma)
- added support for TR3 weather effects to all games (#3881)
- added support for 3D secret objects, and provided defaults for OG levels in TR2 (#4380)
- added catalog object IDs to Lua
- added the ability to swap meshes in Lua
- added support for locked cameras, similar to TR4+ (#2040)
- added support to use `O_DINO_WARRIOR` and `O_FISH` as aliases for `O_TREX` and `O_BARRACUDA` respectively
- added the ability to define gun types, flash shade and offset positions in `cfg/weapons.json5`
- added the ability to define ammo pickup quantities per weapon in `cfg/weapons.json5` (#4518)
- added a new input, that lets the player toggle in-game textures on/off, available by default under F8
- added a new console command, `/textures`, that lets the player toggle in-game textures on/off
- added a new console command, `/weather`, that lets the player control the weather
- added a new console command, `/spawn`, that lets the builder spawn an entity of their choice to test things around
- added Animating Item 1-6 control
- added the option to use TR3 sprite-based shadows (Visuals → Shadows shape)
- added an option for soft static mesh collision; this also allows for arbitrary mesh rotation in custom levels and retaining accurate collision (Gameplay → Controls → Soft mesh collision) (#3654)
- added an option to use the TR3 camera (Visuals → Camera Mode)
- improved a fade-in and fade-out effect on loading screens – they now smoothly transition to the game screen
- improved fog behavior to be less dependent on camera rotation
- changed the 3D pickups option to try the simplified 3D meshes first, if available, before falling back to inventory items
- changed the 2D and 3D statics limit from 256 to unlimited
- changed the lighting contrast key binding to F9
- changed underwater statics to be affected by caustics, even if they don't get merged into level geometry (#4430)
- changed Magnums and Automatic Pistols to be separate objects, so both can appear in the same level (#4475)
- changed the M16 and MP5 to be separate objects, so both can appear in the same level
- changed the swinging axe to be defined separately from other pendulums (use object `O_SWINGING_AXE` in catalogs)
- changed the following trap types to support being reset (#3993)
  - collapsible tiles
  - Damocles swords
  - ember emitters
  - falling ceiling
  - hooks
  - icicles
  - lava wedge
  - pendulums
  - pushblocks (via timed triggers only)
  - spike ceilings
- changed the fonts to no longer use hardcoded character widths
- changed the fonts to use dedicated sprites for accented characters instead of composing them at runtime
- changed the fonts to use dedicated sprites for similar-looking characters instead of using aliases
- changed the reset keybindings bars appearance to be more visible
- changed the default exposure bar PC color to blue 2
- changed lua music `PlayMode` constant names
- removed the `scripting/trx` directory – internal TRX LUA scripts now get embedded in the exe
- fixed broken final statistic counters (#4432, regression from 1.0)
- fixed undefined behavior (crashes and/or texture glitches) in levels with a lot of textures
- fixed a crash if a pickup aid spawns against an item whose 3D model isn't present
- fixed Bacon Lara not always being drawn perfectly in sync with Lara's animation (#4210)
- fixed gondolas not being drawn with an underwater tint when they have sunk (#4428)
- fixed the teleport-to-item command not succeeding if used in succession with the same type and an out of bounds item is encountered (#4468)
- fixed skybox faces with transparent pixels always rendering in front of all other faces (#4351, regression from 1.0)
- fixed unbound inputs not being saved between game launches (#4360, regression from TR1X 4.14/TR2X 1.4)
- fixed Lara drawing a flare when the draw weapons input is pressed, and she already has an active flare but no weapons (#4361, regression from TR2X 1.4)
- fixed wading splashes spawning when using the fly cheat (#4400, regression from 1.0)
- fixed grenades not exploding floating water creatures (#4399, regression from TR2X 1.3)
- fixed water enemies not getting tinted when dead and floating (#4407, regression from 1.0)
- fixed Lara not colliding with mines/gondolas when underwater (#4424, regression from TR2X 1.3)
- fixed flare box pickups containing only one flare if Lara has none in her inventory at that time (#4423, regression from 1.0)
- fixed water enemies appearing untinted for a frame after dying and moving to the water surface (#4420, regression from TR2X 0.1)
- fixed the interactive fly cheat breaking with animated interactions enabled (#4444, regression from TR1X 4.14)
- fixed switch triggers using an incorrect state check, which could result in fixed camera behavior that deviated from OG (#4456, regression from 1.0)
- fixed ambient music triggers to no longer kill active normal music tracks (#4463)
- fixed game crashing when Lara passes through light sources in certain levels
- fixed waterfall mist not brightening when holding a flare (#4486)
- fixed resetting camera in the photo mode not clearing the underwater tint
- fixed developer console text editing (backspace, moving the caret) doing weird things with Unicode characters
- fixed Lara jumping if player holds the swim button when exiting the fly cheat (#4470)
- fixed game refusing to load savegames made with the JP mode (#4558)

**TR1**:
- added the ability to change inventory and statistics background styles (pattern + wave are not implemented in TR1)
- added Automatic Pistols, the Desert Eagle, the MP5, and the Rocket Launcher to the `/moreguns` console command
- fixed Lara standing two clicks below `O_FALLING_BLOCK_3` items rather than directly on top (#4374)
- fixed missing menu guns SFX in Lara's Home
- fixed several OG texture issues in Caves (rooms 0, 1, 2, 6, 24, 30 and 32)
- fixed Lara automatically being given TR2 weapons in NG+ when playing the OG levels (#4365, regression from 1.0)
- fixed Lara's pistol holster meshes appearing in NG+ in place of her Uzi holster meshes (#4368, regression from 1.0)
- fixed Lara's footstep sounds being very quiet when weapons are equipped (#4451, regression from 1.0)
- fixed the grenade blast SFX not always playing in succession (#4628, regression from 1.0)

**TR2**:
- added unused gym voice line at level start if Lara has any logged assault course attempts (#2822)
- added high-resolution 16:9 and 4:3 loading screens
- added high-resolution 16:9 and 4:3 game end screen  
    To download the new images ahead of a stable release, please see the [TRX data](https://github.com/LostArtefacts/TRX-data) repository.
- added Magnums, the Desert Eagle, the MP5, and the Rocket Launcher to the `/moreguns` console command
- changed Tibetan Foothills to have snow (you can disable this via Graphic Options → Visuals → Weather)
- changed ember emitters to use the `SFX_LAVA_FOUNTAIN` sample (#4376)
- fixed the scuba diver's death SFX not playing (#4386)
- fixed a missing trigger for tiger 6 in Ice Palace (#4390)
- fixed missing music triggers in Venice room 11 and Floating Islands room 80
- fixed a missing death tile in Floating Islands room 91
- fixed vertex lighting and stretched textures in Lara's Home room 28 and Home Sweet Home room 27
- fixed z-fighting on fences in Barkhang Monastery and gondola poles in Venice
- fixed missing oxygen tanks in Offshore Rig room 82
- fixed the monk in the Diving Area cutscene not having a complete death animation
- fixed demos not using loading screens
- fixed reading room lights for custom TR2 levels (regression from 1.0)
- fixed the switch in room 46 of Opera House randomly disappearing
- fixed game crashing when Lara passes through light sources in levels compiled with dxtre3D
- fixed Snowmobile music not getting resumed (#4519)
- fixed Stopwatch position in the inventory ring (#2014)
- fixed static lighting on broken ice/windows (#4506, regression from 1.0)

**TR3**:

A lot of our TR3 work builds on *TOMB3*, which Troye and ChocolateFan kindly let us dive into and expand on.
Their hard work gave us the perfect base to push TRX further, and made the climb a lot less vertical!

- added support for monkey bar mechanics
- added support for crawlspace mechanics
- added RGB lighting system support
- added flame effects
- added swamp and water surfaces wave effect
- added underwater caustics
- added proper bubbles
- added water splash and ripple effects
- added waterfall mist effect
- added per-mesh underwater tinting (Lara only)
- added `cdaudio.wad` music playback support
- added weather effects
- added sprite-based shadows
- added footprints
- added surface-based step sounds
- added cold breath effects
- added gun shells
- added gun projectiles
- added gun smoke effects
- added new ricochets
- added flare lighting and sparks
- added monochrome inventory backgrounds
- added TR3 inventory ring lighting
- added high-resolution 16:9 and 4:3 loading screens
- added high-resolution 16:9 and 4:3 title and game end screens
- added high-resolution 16:9 and 4:3 credit images
    To download the new images ahead of a stable release, please see the [TRX data](https://github.com/LostArtefacts/TRX-data) repository.
- added support for the serif font
- added support for colored text
- added Assault Course and Race Track course mechanics
- added Quad Bike control
- added Animating Item 1-6 control
- added Electrical Light control
- added Smoke Emitters control
- added Steam Emitter control
- added Flame Emitter 1-3 and Side Flame Emitter control
- added Piranhas and Tropical Fish control
- added Desert Eagle control
- added MP5 control
- added Rocket Launcher control
- added Magnums, the Automatic Pistols, and the M16 to the `/moreguns` console command
- added all weapons to Lara's Home (accessible with cheats or via the console only)
- added Assault Course target control
- added Assault Course penalty system
- added an option to fix the MP5 accuracy while running
- added TR3 camera control and look functionality
- improved run-to-crawl transition
- improved text colors of the Assault Course statistics and timers
- improved Assault Course targets to spawn ricochets
- changed The River Ganges, City and All Hallows to have rain
- fixed sample reading to support correct pitch and volume
- fixed pool edges shifting along with the water effect
- fixed Lara's thigh being drawn when a flare is in Lara's hand or has been discarded
- fixed gun flashes being drawn in white
- fixed disabling lighting system not working
- fixed skybox data to show correct top and bottom colors
- fixed Assault Course timer remaining indefinitely on screen
- fixed Quad Bike low visibility of exhaust smokes at high speeds
- fixed Quad Bike wheels appearing to spin backwards at high speeds
- fixed the skybox's blue lid for the Thames Wharf and City cutscenes
- fixed fish schools to no longer swim at supersonic speeds if their triggers do not have timers set, or reuse the same timer
- fixed Lara letting go of some ledges
- fixed shadow sizes dependent on Lara's placement instead of their owner's

## [1.0.3](https://github.com/LostArtefacts/TRX/compare/trx-1.0.2...trx-1.0.3) - 2025-11-27
- fixed the conveyor belt fuse in Natla's Mines not appearing after using the nearby switch (#4349, regression from 1.0)

## [1.0.2](https://github.com/LostArtefacts/TRX/compare/trx-1.0.1...trx-1.0.2) - 2025-11-26
- fixed Lara being unable to interact with keyholes after picking up an item if animated interactions are enabled (#4342, regression from 1.0)

## [1.0.1](https://github.com/LostArtefacts/TRX/compare/trx-1.0...trx-1.0.1) - 2025-11-25
- changed default master volume to 80% in TR2 to match TR1 (#4337)
- fixed 2D sprites not appearing in the UI (#4338, regression since 1.0)

## [1.0](https://github.com/LostArtefacts/TRX/compare/76109a8855da99f3304ca4d9a3f5882dada2dd40...trx-1.0) - 2025-11-23
Showcase: https://youtu.be/vVU9vbUXTXc
**Common**:
- added LUA scripting engine
    Supports basic events, item interactions, teleporting and much more.
    See [the documentation](../trx/lua) for details.
- added a game flow option for cold water in custom levels, similar to TR3 (#4021)
- added a splash effect when Lara jumps in wading depth water, similar to TR3+ (#3975)
- added bounding box debugging (`/debug 1` or `/set debug-cuboids 1`)
- added support for object, music, sound, flip effects, Lara state, and Lara animation slots overrides through CSV catalogs  
    Lets builders link hardcoded logic to slots of their choice, allowing object sharing between games (for example, use TR1 bats in TR2).  
    This feature is experimental — some objects may not behave correctly. Please report any bugs encountered! 🩷  
    See [the documentation](../CATALOGS.md) for details.
- added `enable_debug_camera` setting that shows camera position in realtime (reachable via `/debug` and `/set`)
- added the ability to fast-forward through cutscenes with the right button (+5 s) or with slow+right (+1 s)
- added support for dark theme on Windows
- added support for triangular geometry
- added support for additive blending in textures
- added support for quicksand rooms
- improved bilinear filtering for smoother edge blending when multiple objects overlap in depth
- improved rendering of statics and items in overlapping rooms (#2005)
- improved ricochets placement
    - fixed dart and disc ricochets being placed mid-air (#4063)
    - fixed ricochets not showing on slopes
- improved bar setting UIs in various ways
    - added two new options: "Show bars" on/off, and "Flash bars" on/off
    - changed the bars options to be placed in its own tab
    - changed the appearance labels to better align with expectations (#4025)
    - removed the look modes for every bar (except enemy bars that retain the "boss only" setting)
    - fixed health bar flicker on medi packs when cycling the inventory ring (#4211, regression from TR1X 4.14 / TR2X 1.4)
- changed the `/debug` command to accept optional option name argument (for example: `/debug pos 1`)
- changed dart emitters and disc emitters to have separate slots (so with catalogs, both can be used in the same level simultaneously)
- changed the debug position UI to no longer be hidden in photo mode
- changed the unrestricted look mode option to include Lara being able to look freely while shooting an enemy (#4090)
- changed the `ambient_tracks` property to be only available on the root level
- changed music triggers that match the level's default ambient track to automatically be treated as ambient if omitted from `ambient_tracks` (#4181)
- changed the `-q`/`--quiet` argument to no longer silence warnings
- changed the `Remember Guns between Levels` option to also apply to whether or not Lara starts with those guns equipped
- changed the FOV formula to be consistent between games
    - changed the FOV default increment from 10 to 5 (#4026)
    - removed "Vertical FOV" option
    - removed "Use PS1 FOV" option
- fixed missing footstep sound effects when Lara climbs off a ladder and when she finishes a handstand (#4030)
- fixed a crash in custom levels if a flip effect that expects to act on an item is used in a regular trigger (#4085)
- fixed a crash if trying to kill an enemy by name but there is no naming definition for that object
- fixed photo mode camera clipping through overlapping rooms (#1674)
- fixed bogus warnings about resume info in logs when playing cutscenes and in the title level
- fixed title bar size being too small on HiDPI screens on Windows platform (#2837)
- fixed statics and items not getting rendered when all portals leading to them are offscreen (#2005)
- fixed Lara's arms getting stuck in the M16 gun firing animation while she dies (#4130)
- fixed Lara jittering in the QWOP state
- fixed doors and trapdoors not interpolating when using the door cheat
- fixed credit images and loading images showing black screen if the file is missing (#4325)
- fixed caustics not affecting underwater plant sprites (#4317)

**TR1**:
- added a new easter egg command
- added support for flares (for OG levels, use `/give flare`) (#4121)
- added support for TR2 weapons (for OG levels, use `/give moreguns`)
- added support for custom levels to use Lara's extra animations from TR2
- added new hidden settings (available via LUA and the `/set` command):
    - `flow.lockout_option_ring`
    - `flow.load_save_disabled`
    - `flow.play_any_level`
    - `flow.cheat_keys`
- added the ability for the sound system to use Lara's position instead of camera's position (#1438) (Sound Options → Misc → Microphone near Lara)
- added an option to use TR2-style inventory ring backgrounds in custom levels (Graphic Options → UI → Inventory background) (#4264)
- added an option to use TR2-style statistics dialog backgrounds in custom levels (Graphic Options → UI → Stats background) (#4264)
- improved the positions of some 3D pickup items, such as the scion that Pierre drops
- improved the quality of the PS1 Uzi SFX (#4024)
- changed the following game flow options to become hidden settings (available via LUA and the `/set` command):
    - `flow.demo_delay`
    - `gameplay.enable_killer_pushblocks`
- changed Select Level and Story So Far features placement to the New Game menu
- changed the input buffering option to separately tackle F-keys and Inventory (Gameplay → Input → Buffering (F-keys), Gameplay → Input → Buffering (Inventory))
- changed exploded meshes to trigger a splash effect when they hit water, similar to TR2
- changed LOS algorithm to TR2+ implementation
- changed save crystal collision to make them easier to activate
- changed cutscene data (e.g. `cut1.phd`, as opposed to in-game cinematics) to match TR2 format, where Lara (as `O_LARA`) must be defined as an item in the level file
- changed the Remove shotguns, Remove Uzis and Remove Magnums into a single "Remove extra guns" option
- changed toggling Lara's braid in-game to swap out her head and torso meshes appropriately without the need to reload the level (#2399)
- removed the `Enhanced shotgun targeting` option in favour of using the common weapon lock mode (Gameplay → Controls → Weapon lock mode)
- fixed Lara being able to push blocks through toggle opacity 1 portals (#4129)
- fixed Lara drifting during the Atlantis cutscene while the camera focuses in on Natla (#4153)
- fixed Lara retaining her hit animation if nudged by an enemy at the same time as starting a special animation such as picking up a scion (#4212)
- fixed Lara being drawn if the explosion cheat has been used and Bacon Lara is active (#4148)
- fixed ambient music not playing in demo levels (#4046, regression from 4.13)
- fixed caustics stopping after spending roughly 12 minutes in a level (#4109, regression from 4.10)
- fixed legacy UB crashing the game (#4113, regression from 4.12)
- fixed select level feature to also be available to games started with `/play`
- fixed select level feature slot status not updated on save
- fixed "Story So Far..." showing loading screens
- fixed matrix stack overflow crash when moving through overlapping or dome portals (#2685)
- fixed the gun-draw SFX playing when holstering the shotgun (#3755)
- fixed Lara's braid remaining reflective if the fly cheat is used to resurrect her on the Midas hand
- fixed invulnerability cheat not getting disabled during the demos (regression from 4.13)
- fixed crash when loading OG saves made in City of Khamoon, while the "Restore PS1 enemies" option is on (#4217, regression from 2.16)
- fixed the jump lock mode UI option remaining visible when responsive jumping is disabled (#4027, regression from 4.13)
- fixed a slight misalignment in the PDA in its open state (#4247)
- fixed Lara being unable to exit the water in (for example) room 41 of Return to Egypt (#4315, regression from 4.12)

**TR2**:
- added loading screens (Gameplay Options → General → Loading screens) (#1620)  
    Tomb Raider II 3×2 upscales done by Arsunt.  
    Tomb Raider II: The Golden Mask images done by Lito Perezito.
- added Restart Level option when Lara dies (#1555)
- added Play Previous Levels feature (available in the New Game screen)
- added Story So Far… feature (available in the New Game screen)
- added game mode selection to the Play Any Level feature
- added support for custom levels to use Lara's extra animations from TR1
- added an option to use TR1-style inventory ring backgrounds (Graphic Options → UI → Inventory background) (#3923)
- added an option to use TR1-style statistics dialog backgrounds (Graphic Options → UI → Stats background) (#3923)
- added extended statistics support (#2578)
    - added pickup count and death count support in the stats screen (Graphic Options → UI → Statistics details)
    - added max pickup, secret and kills support (Graphic Options → UI → Statistics details)
    - added deaths counter support (Gameplay Options → General → Count Lara's death)
    - added unobtainable secrets, pickups and kills stats support in the gameflow
- added an option to disable final statistics (Gameplay options → General → Final statistics screen)
- added an option to disable all medipacks (Gameplay options → Mods → Remove medipacks)
- added an option to disable all guns except Pistols (Gameplay options → Mods → Remove extra guns)
- added an option for pickup aids, which will show an intermittent twinkle when Lara is nearby pickup items (Graphic Options → Visuals → Pickup aids) (#4057)
- added an option for animated interactions with pickups and switches (Gameplay → Controls → Animated interactions) (#4067)
- added an option to change max savegame slot count (Gameplay → General → Number of save slots)
- added an option to turn off Inventory input buffering (Gameplay → Input → Buffering (Inventory))
- added an option to turn on TR1-style F-keys input buffering (Gameplay → Input → Buffering (F-keys))
- added an option to draw Shotgun flashes (Graphic Options → Visuals → Shotgun flash)
- added support for TR1-like secret triggers (#2047)
- added support to disable wading, like TR1 (Gameplay → Controls → Wading) (hidden by default)
- added support to disable responsive running jumps, like TR1 (Gameplay → Controls → Responsive jumping)
- added support to disable responsive swim cancel, like TR1 (Gameplay → Controls → Responsive swim cancel)
- added support for game-flow defined enemy item drops, similar to OG TR1 levels; regular level-defined drops will continue to work normally
- improved the quality of the PS1 barefoot SFX (#4024)
- changed the following game flow options to become hidden settings (available via LUA and the `/set` command):
    - `flow.lockout_option_ring`
    - `flow.load_save_disabled`
    - `flow.play_any_level`
    - `flow.demo_delay`
    - `flow.cheat_keys`
    - `gameplay.enable_killer_pushblocks`
- changed the Pause key to no longer work when Lara's dead (similar to TR1)
- changed sprites to respect the water tint if placed underwater
- removed the following game flow options:
    - `cmd_init`
    - `cmd_title`
    - `cmd_death_in_demo`
    - `cmd_death_in_game`
    - `cmd_demo_end`
    - `cmd_demo_interrupt`
    - `single_level`
    - `is_demo_version`
- fixed the new game modes dialog requiring the Action key to show up (TR2 dialogs don't need this.)
- fixed Lara's pistols not being removed from holsters when she equips during a cutscene (#4136)
- fixed potential softlocks in custom levels with enemies who have end-level flip effects but the player uses the grenade launcher to kill them (#4261)
- fixed Lara not having holstered pistols after she changes costumes in the Diving Area cutscene (#4142)
- fixed ambient music not playing in demo levels (#4046, regression from 1.3)
- fixed twists not adhering to original game movement (#4078, regression from 1.4)
- fixed legacy saves in Opera House and Vegas crashing the game (#4103, regression from 1.5)
- fixed caustics stopping after spending roughly 12 minutes in a level (#4109, regression from 1.4)
- fixed Lara being able to push blocks through toggle opacity 1 portals (#4129, regression from 1.5)
- fixed pistols disappearing from Lara's holsters in the cutscene following The Great Wall (#4145, regression from 0.9)
- fixed Lara's thigh meshes defaulting if entering the fly cheat while holding a flare and she doesn't currently have holstered weapons (#4143, regression from 1.3)
- fixed wrong lighting of exploded body parts
- fixed weird clipping when moving through overlapping or dome portals (#2685)
- fixed Lara reloading the harpoon gun if she draws the weapon and does not have any harpoons (#4259)
- fixed Lara holding on to the grenade launcher for too long when undrawing it (#3474)
- fixed the holster SFX playing when drawing rifle type weapons (#3755)
- fixed missing SFX in the harpoon drawing and undrawing animations (#3755)
- fixed invulnerability cheat not getting disabled during the demos (regression from 1.3)
- fixed disable targeting allies option not working (#4184, regression from 1.5)
- fixed Lara losing forward momentum on springboards when the wall glitch mode option is set to `Fixed` (#4187, regression from 1.2)
- fixed the M16 accuracy option not taking effect until restarting the game (#4227, regression from 0.3)
- fixed incorrect keys object orientation in the inventory ring (#4239, regression from 0.3)
- fixed underwater hum when Microphone near Lara option is on (#2188)
- fixed 3D pickups not rendering if the associated sprite is not present in the level file (#4275, regression from 0.6)

**TR3**:
- added basic TR3 level loader (nothing is working yet!)
