## [Unreleased](https://github.com/LostArtefacts/TRX/compare/tr1-4.15.1...develop) - ××××-××-××
See [/docs/CHANGELOG.md].

## [4.15.1](https://github.com/LostArtefacts/TRX/compare/tr1-4.15...tr1-4.15.1) - 2025-10-10
- changed the examine dialog to be usable with non-puzzle items (#4009)
- fixed a crash on game exit if specifying "ambient_tracks" in the game flow root (regression from 4.11)
- fixed alternate ambient tracks being lost on reload in custom levels (#3997, regression from 4.14)
- fixed Lara at times not being able to grab pushblocks despite being in the correct position to do so (#4005, regression from 0.9.1)
- fixed Lara appearing flat for a frame during the neutral twist, controlled drop and ledge jump back animations (#4012, regression from 4.14)
- fixed the pickup embed glitch when Lara is below a steeply sloped ceiling not being optional (Gameplay → Fixes → Fix pickup embed glitch) (#4020, regression from 4.10)

## [4.15](https://github.com/LostArtefacts/TRX/compare/tr1-4.14.2...tr1-4.15) - 2025-10-04
Showcase: https://youtu.be/BwZXWL0WULg
- added an option to use TR2-style UI bars (Graphics → UI → Bars look)
- added an option to use PS1-style UI bars (Graphics → UI → Bars look) (#1637)
- added a new `/cls` / `/clear` console command to quickly clear console logs
- added support for ladders (#3124)
- improved PS1-style UI faithfulness
- improved sound settings:
    - added tabs (Volume and Misc)
    - added a dedicated option to control master volume (Sound options → Volume → Master volume)
    - added a dedicated option to control cutscenes volume (Sound options → Volume → Cutscenes volume) (#3490)
    - added a dedicated option to control FMV volume (Sound options → Volume → FMV volume) (#3490)
    - added a dedicated option to control general ambient volume (Sound options → Volume → Ambient volume) (#3707)
    - improved volume settings to accept slow input for finer adjustments
    - fixed changing sound volume not updating certain ambient sound sources while in the inventory ring (#3970)
- changed OG glitch-related config options to be on/fixed by default (#3929)
- changed the UI style to use the PS1 look by default (Graphics → UI → Menu style)
- changed pickup aids to be enabled by default (Graphics → Visuals → Pickup aids)
- changed responsive jumping to be enabled by default (Gameplay → Controls → Responsive jumping)
- changed lean jumping to be enabled by default (Gameplay → Controls → Lean jumping)
- changed smooth swimming to be enabled by default (Gameplay → Controls → Smooth swimming)
- changed responsive swim cancel to be enabled by default (Gameplay → Controls → Responsive swim cancel)
- changed idle pose timeout from 15 to 60 seconds by default (Gameplay → Controls → Idle pose timeout)
- changed idle pose camera to be disabled by default (Gameplay → Controls → Idle pose camera)
- changed PS1 uzi sound to be enabled by default (Sound → Misc → PS1 uzi sound)
- changed max pickup scale to 200% (#3952)
- fixed pickup scale being greyed out if the 3D pickups option is enabled (#3952)
- fixed certain ambient sounds volume scaling wrong on non-100% volumes
- fixed trapdoor type 3 (object #67) not functioning (#3895)
- fixed gameplay settings UI displaying eagerly after the first use (#3583, regression from 4.13)
- fixed changing FPS after advancing frames in photo mode causing the game to speed up (#3605, regression from 4.13)
- fixed CPU spike during playing FMVs (#3908, regression from 4.6)
- fixed `/play` command likely to skip opening FMVs when inventory buffering is enabled (#3910, regression from 3.0)
- fixed `/pos` command crashing in cutscenes (#3944, regression from 4.10)
- fixed loading a save made in the gym with the item cheat resulting in Lara's meshes appearing broken (#3917, regression from 4.7)
- fixed resumed music tracks playing briefly track start upon savegame load (#3916)
- fixed loading TombATI saves with shotgun equipped causing weird Lara's animation (#3920, regression from 4.12)
- fixed numerous interactions with movable blocks, trapdoors, drawbridges, bridges, sliding pillars, and falling blocks for custom levels (#2758):
    - added the ability for movable blocks to move on trapdoors, drawbridges, bridges, sliding pillars, and falling blocks
    - added the ability for stacks of movable blocks to fall and land on trapdoors, drawbridges, bridges, sliding pillars, and falling blocks
    - added the ability for stacks of movable blocks to fall when on opened trapdoors and drawbridges
    - fixed various bugs with falling movable blocks
- fixed pushblocks becoming unusable when on the same sector as a door that does not sit on a room portal (#3814)
- fixed pushblocks that fall from a great height potentially causing a crash (#3969)
- fixed recordings replaying commands twice (regression from 4.14)
- fixed the fix for the sticky corner glitch not being optional - now linked to Gameplay → Fixes → Wall glitch mode (#3957, regression from 4.14)
- fixed Lara retaining guns if drawn during wade to float transition (#3979, regression from 4.13)
- fixed -s/--save argument no longer working with -l/--level (#3990, regression from 4.14)

## [4.14.2](https://github.com/LostArtefacts/TRX/compare/tr1-4.14.1...tr1-4.14.2) - 2025-09-07
- fixed broken rendering in MacOS releases (#3880, regression from 4.14)
- fixed images from MacOS releases (#3892, regression from 4.14)

## [4.14.1](https://github.com/LostArtefacts/TRX/compare/tr1-4.14...tr1-4.14.1) - 2025-08-30
- fixed missing shader and configuration files from MacOS releases (#3870, regression from 4.14)
- fixed zero byte at the end of config files (#3875, regression from 4.14)
- fixed stacked sprites flickering (#3872, regression from 4.14)

## [4.14](https://github.com/LostArtefacts/TRX/compare/tr1-4.13.2...tr1-4.14) - 2025-08-23
Showcase: https://www.youtube.com/watch?v=iV8G9lhxVQ8

>[!WARNING]
>Attention level builders: this version introduces backwards incompatible changes to the file structure.
>Please refer to the [migration guide](../trx/MIGRATING.md) to see how to update your levels.

- added lighting contrast option (Graphic options → Rendering → Lighting contrast)
- added new command switches:
    - `--test-record` and `--test-replay` for automated playthroughs with (internal tool – the recording file format may be subject to changes)
    - `--headless`: runs the game offscreen with no audio and at unlocked simulation speed
    - -q`, `--quiet`: outputs only error messages to the terminal, with log files being written to normally
- added new hotkeys: F7 for toggling the wireframe mode, F8 for cycling the lighting contrast
- added ability to move Lara around in photo mode
- added additional poses for photo mode
- added an option to allow Lara to sprint (Gameplay → Controls → Sprinting) (#3711)
- added an option to use Lara's slide-to-run animation from TR3+ (Gameplay → Controls → Slide-to-run) (#1089)
- added an option to use Lara's neutral jump-twist from early TR1 betas (Gameplay → Controls → Neutral twists) (#1392)
- added an option to allow Lara to turn around and grab a ledge she has just stepped off (Gameplay → Controls → Controlled drops) (#3621)
- added an option to allow Lara to jump up or back when hanging from a ledge (Gameplay → Controls → Ledge jumps) (#3683)
- added an option to have Lara pose after standing idle for a certain time (Gameplay → Controls → Idle pose timeout) (#3727)
- added an option to keep sprites upright (Graphic options → Rendering → Sprites lock mode)
- added an option to scale the 3D pickups in the UI (Graphic options → UI → Pickup scale)
- added an option to control fog color (Graphic options → Visuals → Fog transparency and Fog color) (#712, #3618)
- added Russian translation
- added German translation
- added skyboxes to The Cistern and Tomb of Tihocan (#2143)
- added a new `/lua` console command (for now, [it cannot do much](../trx/lua/))
- added a new `/restless` console command, which enables or disables infinite sprint
- added debug room clip feature (`/debug 1`)
- improved object loading error messages when an invalid object ID is detected
- improved frames in Lara's jump-twist animations
- improved lighting, projection and sizing of 3D pickups in the UI
- improved PS1 menu style border offsets and frames to match PC style
- improved drawing shadows in no-clip camera mode (they're no longer double-sided)
- improved wireframe mode to show text and UI normally
- improved bilinear filter edge blending (#587)
- improved window resize performance in the title inventory ring
- changed the brightness filter to also work on title inventory ring background
- changed the brightness filter option to work in smaller increments (10% reduced to 5%); added support for slow increments by 1% (hold Walk key)
- changed the text and bar scale option to work in smaller increments (10% reduced to 5%); added support for slow increments by 1% (hold Walk key)
- changed the game flow and game strings file placement
- changed the skybox option to allow toggling in-game without the need to reload the level
- changed the texture page limit from 128 to unlimited (#3517)
- changed the `/set` console command to report boolean values as `0` or `1`, language-agnostic
- changed waterfall objects to always be drawn when active rather than only when Lara is within a 10 sector range (#3598)
- changed `-l`/`--level` switch to accept the level number on top of the level path
- changed settings dialogs to show a suitable message if a level builder has hidden all options within that dialog (#3637)
- changed the fly cheat to allow Lara to interact with switches and pickups (#3665)
- removed the option in Unfinished Business to fix animated sprites as it is irrelevant there
- fixed glide camera behaviour and position in room 101 in Temple of the Cat (#3533)
- fixed French translations containing Italian text in some cases (#3567)
- fixed the camera remaining locked on moving lava if it touches Lara when she is immune (#3578)
- fixed several issues with door data
    - fixed missing door/trapdoor sound effects (#3408, #3374, #3617, #3619)
    - fixed animation frames in doors in St. Francis' Folly, Tomb of Tihocan and Sanctuary of the Scion (#3661)
    - fixed the cameras for doors 81 in Tomb of Tihocan and 1 in Sanctuary of the Scion only showing once (#3661)
- fixed the passport having an invisible back page, noticeable when opening/closing it (#2051)
- fixed z-fighting on the front of the passport (#3584)
- fixed setting description dialog missing borders with PS1 UI style (#3714, regression from 4.12)
- fixed being unable to activate waterfall objects with code bits (#3589)
- fixed skippable triggers for waterfall objects in Lost Valley (#3593)
- fixed incorrectly rotated 3D pickup items in several levels (#2147)
- fixed incorrect room mesh structure in Vilcabamba room 41, causing disappearing polygons (#3613)
- fixed missing textures on the statues in Vilcabamba and Tomb of Qualopec (#3629)
- fixed missing textures in Atlantis rooms 7, 9, 13, 14, 95, 96 (#3657)
- fixed missing double-sided textures in The Cistern rooms 9 and 12
- fixed texture clipping in Atlantean Stronghold when looking into room 18, and missing textures in rooms 5, 6, 18 and 74 (#3668)
- fixed the collision box on the tall statues in Tomb of Qualopec e.g. room 20 (#3629)
- fixed the mesh structure on the cat statue in Egyptian levels to standardize its position (#3634)
- fixed the collision box on some static meshes in Egypt to prevent the camera shaking when Lara walks by them (#762)
- fixed incorrect transparent pixels on room textures in St. Francis' Folly and Temple of the Cat (#3659)
- fixed the positions of big pods in Atlantean levels and cutscenes (#3670)
- fixed several texture issues in Lara's Home, Vilcabamba, Lost Valley, St. Francis' Folly and Natla's Mines (#3860)
- fixed a missing transition animation between Lara jumping forward and entering freefall (#3815)
- fixed incorrect wet footstep sounds in some of Lara's climb-up animations (#3607, regression from 4.6)
- fixed the `/kill` command potentially causing a crash if used in a level with pods that don't hatch creatures (#3628, regression from 4.12)
- fixed Lara's animation not being restored correctly on load if a save was made during a special animation, such as using the Midas Hand (#3625, regression from 4.9)
- fixed emitted darts moving in the wrong direction when reloading a save (#3677, regression from 2.11)
- fixed backslash/grave key/less-than character on some keyboards shown as ???? – now it's shown as backslash (#3713)
- fixed wireframe mode rendering as mostly white (#3649, regression from 4.13.2)
- fixed wireframe mode not working in the inventory ring (#3651, regression from 4.10)
- fixed the boulder in room 78 getting drawn in the overlapping room 74 in Tomb of Tihocan (#3761, regression from 4.10)
- fixed shadow rendering
    - fixed Y component not interpolated in 60 FPS (#1314)
    - fixed shadows being rendered partially opaque near room portals (#879)
    - fixed Bacon Lara shadow rendered transparent when she's standing on a trapdoor (#3666)
- fixed potentially being able to reactivate an already used puzzle slot's trigger (#3849, regression from 4.13)
- fixed being unable to cycle poses in photo mode if cheats were disabled (#3726, regression from 4.13)
- fixed Lara exiting the fly cheat if the walk key is used during photo mode (#3753, regression from 4.13)
- fixed being able to issue certain console commands that target Lara during loading screens (#3662, regression from 4.13)
- fixed flame SFX being audible underwater (#3830, regression from 4.13)
- fixed z-fighting of doors near walls
- fixed Lara walking backwards off ledges into lava (#3745)
- fixed room scheduling algorithm sometimes drawing overlapping rooms (#3774, regression from 4.1)
- fixed exiting photo mode on a controller conflicting with the roll input (#3842, regression from 4.8)
- fixed resuming non-ambient music tracks when loading a savegame (#3845, regression from 4.13)
- fixed quick draw button not working until after Lara equipped guns by other methods with certain saves (#3844, regression from 4.13)

## [4.13.2](https://github.com/LostArtefacts/TRX/compare/tr1-4.13.1...tr1-4.13.2) - 2025-07-20
- fixed savegame scanner only seeing all-lowercase file names (#3518, regression from 4.9)
- fixed drawing UI text with bilinear filter and PS1 UI (#3548, regression from 4.13)
- fixed dynamic fire light being generated despite the flame object not being present in the level (#3539, regression from 4.13)
- fixed the first camera frame when starting or loading a level being inaccurate (#3537, regression from 4.12.3)

## [4.13.1](https://github.com/LostArtefacts/TRX/compare/tr1-4.13...tr1-4.13.1) - 2025-07-18
- fixed Lara's first pose in photo mode at times being skipped (#3522, regression from 4.13)
- fixed Lara's arms being drawn inaccurately when posing in photo mode with dual weapons equipped (#3520, regression from 4.13)
- fixed Lara being unable to use key items at times with animated interactions enabled (#3524, regression from 4.13)

## [4.13](https://github.com/LostArtefacts/TRX/compare/tr1-4.12.3...tr1-4.13) - 2025-07-14
Showcase: https://youtu.be/YKI7u2QOolU
- reworked screen resolutions
    - removed "screen resolution" option
    - removed "window size" rendering mode, enforcing the FBO rendering method (#3332)
    - added aspect ratio mode (Graphic options → Rendering → Aspect mode)
    - added window border option (Graphic options → Rendering → Borders)
    - added integer upscaling option (Graphic options → Rendering → Upscaling factor)
    - renamed "FBO filter" option to "Upscaling filter" (Graphic options → Rendering → Upscaling filter)
    - greatly improved text and other UI rendering with upscaling turned on (#1944)
    - changed screenshots to always produce images at desktop resolution
- added French translation
- added Gaelic translation
- added Italian translation to the installer
- added dedicated British English translation (#3212)
- added the ability to advance individual frames to the photo mode
- added the ability to skip end game credits (#3266)
- added the ability to hide specific game settings (#3242)
- added the ability to cycle UI tabs with sidestep keys (#3272)
- added the ability to skip consecutive credit images by holding the action / escape keys
- added the ability to cycle between a list of predefined Lara poses in the photo mode
- added a `/lighting` console command to let the player turn lighting system on/off
- added an `/immune` console command to make Lara impervious to damage
- added support for underwater save crystals in custom levels (#3356)
- added an option to have dynamic lights generated by flames (Graphic options → Visuals → Fire lighting) (#3336)
- added an option to control responsive jumping lock behaviour (Gameplay settings → Controls → Jump lock mode) (#3389)
- added an option to display level counter in the statistics dialog (Graphic options → UI → Level counter) (#1087)
- added an option to control playing of certain animation sound effects such as doors when underwater (Sound options → Underwater animation SFX) (#3385)
- added an option to allow the audio to play when the game is out of focus (Sound options → Mute audio when focus lost, #3333)
- added an option to make the quick gun equip keys also holster the active gun (Gameplay settings → UI → Quick gun keys) (#828)
- added an option to control texture filter for UI alone (Graphic options → Rendering → UI filter)
- added the ability to use the dev console during FMVs
- improved the `/tp` command to orient Lara towards keyholes and doors
- improved handling of animation sound effects when in shallow water (#3385)
- improved performance when resizing the window
- improved error messages for game flow and string edit mistakes to include path of the problematic file
- changed statistics details mode to be placed in the UI section
- changed controls dialog to remember the player's preferred input method
- changed UI to show icons relevant to the chosen input method
- changed death timer skip to only trigger with Action and Inventory keys
- changed the examine dialog to be close-able with Look button (#3225)
- changed some settings to be hidden when they're only applicable to specific games or custom levels (#3242)
- changed some settings to be dimmed when they're not taking effect due to other settings (#3166)
- changed photo mode help dialog to show icons for inputs
- changed settings to retain their active position until exiting to title or starting a new level (#3271)
- changed the dev console to accept compound characters (#2938)
- changed save crystal collision to be more lenient for custom levels (#3343)
- changed the walk-run-jump bug fix for responsive jumping to be optional (Gameplay settings → Fixes → Fix walk run jump) (#3389)
- changed the enhanced look option to allow choosing between original TR1, original TR2 or unrestricted modes (Gameplay settings → Controls → Look mode) (#3403)
- changed `/secret give` and `/secret take` to give or take all valid secrets when no index is specified
- removed config tool (we have ingame setting dialogs now)
- removed the "Enable numeric keys" option (it was added when these keys were not changeable)
- fixed several more OG texture issues (refer to README for details) (#3352)
- fixed Lara not saying 'no' near receptacles if she doesn't carry any items (#3337, regression from 4.0)
- fixed Lara not saying 'no' near complete receptacles (#3337, regression from 4.0)
- fixed key items getting consumed at the start of the interaction with receptacles (#3399)
- fixed certain commands (such as `/load` or `/play`) not working as expected while in the key use inventory screen (#3338)
- fixed the camera resetting if Lara is looking and then draws her guns (OG behaviour retained when using restricted look mode) (#3406)
- fixed game window getting misplaced in windowed mode between game relaunches on certain systems (#3418)
- fixed Lara using the wrong hit animation under certain scenarios based on her hit angle (#3424)
- fixed already playing samples not getting muted when the game window goes out of focus
- fixed the `/play` command starting the level with wrong items sometimes (#3147, regression from 4.11)
- fixed the `/tp` command breaking the photo mode
- fixed the `/tp` command misbehaving when giving fractional coordinates
- fixed Compass label in Gym not appearing when holding arrows during inventory spin-out (#3460)

## [4.12.3](https://github.com/LostArtefacts/TRX/compare/tr1-4.12.2...tr1-4.12.3) - 2025-06-24
- fixed game crashing when the expected resources are missing (#3310, regression from 4.12.2)
- fixed restore default pop-up requiring all 3 water color options to be adjusted instead of just one (#3314, regression from 4.12)
- fixed pause screen rendered without background overlay if fade effects are disabled (#3316, regression from 4.11)
- fixed `/pos` command crashing when the level title is not set (regression from 4.12)

## [4.12.2](https://github.com/LostArtefacts/TRX/compare/tr1-4.12.1...tr1-4.12.2) - 2025-06-22
- fixed depth buffer problems when closing the inventory ring with fade effects disabled (#3267, regression from 4.8)
- fixed Lara's braid not being reflective (on Midas' hand) (#3257, regression from 4.9)
- fixed turbo cheat causing audio desync in cutscenes (#3263)
- fixed support for non-linear secret flags in custom levels (#3262, regression from 4.12)
- fixed movable blocks getting stuck in midair if the game is saved and loaded while they are falling (#3274)
- fixed PS touchpad input missing an icon (#3288, regression from 4.12)
- fixed inability to use unbind key / reset layout buttons with controllers (#3290, regression from 4.12)
- fixed inventory ring consuming too many items under severe frame drop conditions (#3295, regression from 4.8)
- fixed screenshots stripping accented characters (#3238)
- fixed accented lowercase `i` characters retaining the superscript dot (#3298)
- reverted the partial fix for wrong audio device reinitialization (#3251, regression from 4.12)

## [4.12.1](https://github.com/LostArtefacts/TRX/compare/tr1-4.12...tr1-4.12.1) - 2025-06-18
- fixed certain secrets not registering (#3252, regression from 4.12)

## [4.12](https://github.com/LostArtefacts/TRX/compare/tr1-4.11.2...tr1-4.12) - 2025-06-17
Showcase: https://www.youtube.com/watch?v=IqjVuXTVI4A
- added builtin support for ingame string translations
    - changed duplicate game strings between TR1 and TR2 to be placed in a single file TRX_common_strings.json5
    - added a new setting, `enable_review_markers`, which display which text requires review (only available via `/set`)
    - added Italian translation
    - added Polish translation
    - added support for non-breaking spaces
    - fixed game crashing when trying to word-wrap unknown characters
- added UI for all config tool settings
- added ingame help for all settings
- added support for object name aliases; added aliases for dev commands
- added an optional breeze effect for Lara's braid in appropriate outside rooms (#3090)
- added keyboard and controller input icons to the controls settings dialog
- added an option to adjust music and ambient volume while in inventory
- added a `/secret` console command for easier debugging of secrets
- added `enable_debug_pos` setting that shows Lara's position in realtime (reachable via `/debug`, fine-tuned `/set`)
- added an option to control whether or not Lara responds to hitting a wall while wading (#3138)
- added an option for smooth wall deflection when Lara comes to a stop at a wall, similar to TR2 (#3148)
- added an option to fix the step glitch where Lara can be pushed into walls (#3148)
- added an option to have Lara always roll off one-click steps rather than boosting forward (#3149)
- added an option to toggle allowing Lara to exit from water horizontally, below, or climbing out onto non-standable slopes (#3154)
- added an option to toggle random enemy initial angle adjustment (#3129)
- improved the teleport cheat if used when Lara is in a special animation, such as grabbing the Scion
- improved the dev console commands documentation
- changed the maximum number of 2D static mesh slots (room sprites) from 50 to 256 (#3200)
- changed the wall glitch config option to a selection of being fixed, using TR1 behaviour or TR2 behaviour (#3153)
- changed sound and music volumes to be displayed as percentage instead of 0-10
- changed the graphic settings dialog to use tabs
- changed the setting dialogs to respect the UI wraparound setting
- changed the `/tp` command to align Lara to switches and pickups
- changed the `/set` command to accept `-`, which will restore the given setting to its default state
- changed the music track slot limit from 64 to 1024 (#3101)
- changed text kerning to a smaller value
- changed the underwater music volume setting to separate ambient and music volume sliders
- changed logs format to include timestamps
- fixed a game crash in custom levels if centaur statues exploded without having centaur objects in the level file (#3155)
- fixed being unable to re-purpose some gym music tracks in custom levels (#3164)
- fixed Lara not catching fire after reloading a save made when she was on fire and enhanced saves are disabled (applies to new saves only) (#3157)
- fixed 3D pickups misplacing or hiding UI elements with render mode set to window size and the game windowed (#3067, regression from 4.10)
- fixed the `/play` command crashing when the game has only ATI saves (#3137, regression from 4.10)
- fixed the `/play` command taking resume information from the highlighted slot (#3137, regression from 4.10)
- fixed text glyphs having cut off right and bottom borders (regression from 4.7)
- fixed unbind key option being available when it shouldn't (#3111, regression from 4.11)
- fixed not saving screen resolution (regression from 4.11)
- fixed vertical FOV option not working properly (#3120, regression from 4.10)
- fixed Lara's position on a ledge after grabbing it extremely late (#3132, regression from 2.2.1)
- fixed a rare crash when editing certain dev console history entries (#2913, regression from 4.10)
- fixed a desync in the Vilcabamba demo if the wall glitch fix option was enabled (#3172, regression from 1.3)
- fixed demos being affected if Lara's starting HP has been altered (#3180, regression from 2.6)
- fixed Lara's health bar showing at the start of cutscenes (#3182, regression from 4.11)
- fixed broken playback of mono music tracks (regression from 2.0)
- fixed hot-plugging certain audio devices causing glitchy playback (partial fix; regression from 2.0)
- fixed being unable to toggle fullscreen mode during FMV sequences (#3188, regression from 4.6)
- fixed examine hint text lingering on screen when moving to a different item in the inventory (#3228, regression from 4.8)
- fixed controls settings dialog missing trapezoid filter option (#3246, regression from 4.9)
- fixed logging not outputting anything on Windows terminals

## [4.11.2](https://github.com/LostArtefacts/TRX/compare/tr1-4.11.1...tr1-4.11.2) - 2025-05-24
- improved word wrapping algorithm in the dev console
- changed examine item descriptions to remove extra blank lines
- fixed examine item overlapping with other UI elements at large text scales
- fixed a crash related to carried items if using saves made prior to 4.11 (#3052, regression from 4.11)

## [4.11.1](https://github.com/LostArtefacts/TRX/compare/tr1-4.11...tr1-4.11.1) - 2025-05-23
- fixed "Load Game" bottom text arrows jumping when entering the load game dialog (regression from 4.11)
- fixed missing arrows around focused navigation elements in the controls dialog (#3042, regression from 4.11)
- fixed text outline being a bit laggy when rebinding the inputs in the controls dialog
- fixed crashes in the save dialog on Linux (#3046, regression from 4.11)

## [4.11](https://github.com/LostArtefacts/TRX/compare/tr1-4.10.2...tr1-4.11) - 2025-05-21
Showcase: https://www.youtube.com/watch?v=JVtcZoNoeRM
- added the ability to trigger a flip effect without having to also trigger the flip map, in line with TR2 (#2921)
- added a /help command (#2917)
- added an option to toggle between TR1 and TR2 camera modes (#2990)
- added the ability to trigger different ambient tracks in custom levels, which will loop and be remembered between saves (#811)
- changed the all items cheat to include the lead bar if present in the level (#3008)
- changed the design of the controls dialog to use pages, making it match the new TR2X controls dialog
- changed the pause screen to have a darker black overlay transparency (#2252)
- fixed Lara's braid pointing straight down when swimming below sloped ceilings (#1600)
- fixed enemy hitpoints being doubled in demo mode as a result of NG+ (#2904)
- fixed an illegal reachable slope in Lost Valley room 58, which could lead to Lara becoming softlocked (#2900)
- fixed some pickup sprites being too far embedded into the floor (#2903)
- fixed vase room sprites in Return to Egypt and Temple of the Cat being embedded in the floor (#2095)
- fixed the camera behaving erratically in rooms/sectors that have no pathfinding data (#2946)
- fixed the game crashing when editing long dev console history entries (#2913, regression from 4.10)
- fixed FPS counter turning off after a game relaunch (#2911)
- fixed falling ceiling and Damocles Sword traps not falling through stacked rooms (#2924)
- fixed health bar in top center position covering inventory text
- fixed the save crystal animation skipping a frame in 60 FPS (#1528)
- fixed Lara unable to equip pistols after getting the Shotgun wet while wading (#2994)
- fixed select level dialog not reacting to the menu back key (#2918, regression from 4.9)
- fixed carried items falling from flying enemies not animating in 60 FPS (#2954, regression from 4.0)
- fixed items carried by the Qualopec mummy spawning early after save/load (#2956, regression from 4.6)
- fixed potential memory corruption if `/kill all` is used with a Qualopec mummy that is carrying items (#2957, regression from 4.6)
- fixed a crash when portal debugging is enabled in rooms that have no portals (#2968, regression from 4.8)
- fixed rats/voles and crocodiles/alligators at times not assuming the correct death pose after reloading a save (#2960, regression from 0.12)
- fixed incorrect camera shifts when some fixed cameras return to normal view (#2971, regression from 4.9)
- fixed Lara not having weapons when playing a level with -l/--level (#2995, regression from 4.9)
- fixed inventory ring items not being animated when the ring is rotating (#2964, regression from 4.9)
- fixed a hole appearing in the floor in Natla's Mines room 84 after exploding the TNT box (#3007, regression from 4.9)
- fixed button mashing causing quick save/load to misbehave on a specific passport animation frame (#3021, regression from 4.10)
- fixed save level numbers being replaced incorrectly if Lara dies in a level and the last save was in the previous level (#3026, regression from 4.9)
- fixed ambient music not looping correctly after reloading a save with the option to reload ambient timestamps enabled (#3032, regression from 4.6)

## [4.10.2](https://github.com/LostArtefacts/TRX/compare/tr1-4.10.1...tr1-4.10.2) - 2025-05-15
- fixed animated textures not working the right way in flipped rooms (#2966, regression from 4.10)
- fixed the final statistics always showing zero deaths regardless of the actual total (#2965, regression from 4.10)

## [4.10.1](https://github.com/LostArtefacts/TRX/compare/tr1-4.10...tr1-4.10.1) - 2025-04-30
- fixed water caustics appearance (#2896, regression from 4.10)

## [4.10](https://github.com/LostArtefacts/TRX/compare/tr1-4.9...tr1-4.10) - 2025-04-30
Showcase: https://www.youtube.com/watch?v=qJPq9obD6Cc
- added an ability to customize the fog distances (#634)
- added an ability to customize the water color [see the reference](../WATER_COLORS.md) (#1532)  
- added support for a hex water color notation (eg. `#80FFFF`) in the game flow file
- added support for antitriggers, like TR2+ (#2580)
- added support for aspect ratio-specific images (#1840)
- added an option to wraparound when scrolling UI dialogs, such as save/load (#2834)
- added aliases to CLI options (`-gold` becomes `-g/--gold`, `-demo_pc` becomes `--demo-pc`)
- improved bubble appearance (#2672)
- improved rendering performance
- improved pause exit dialog - it can now be canceled with escape
- improved the `/set` console command to display available options if given an unknown argument
- added a `--help` CLI option (may not output anything on Windows machines – OS bug)
- changed the `draw_distance_fade` and `draw_distance_max` to `fog_start` and `fog_end`
- changed `Select Detail` dialog title to `Graphic Options`
- changed the number of static mesh slots from 50 to 256 (#2734)
- changed the "enable EIDOS logo" option to disable the Core Design and Bink Video Codec FMVs as well; renamed to "enable legal" (#2741)
- changed sprite pickups to respect the water tint if placed underwater (#2673)
- changed save to take priority over load when both inputs are held on the same frame, in line with OG (#2833)
- changed the sound dialog appearance (repositioned and added text labels)
- changed The Unfinished Business strings to default to the OG strings file for the main tables (#2847)
- changed the dev console to no longer add duplicate entries to the history
- removed the pretty pixels options (it's now always enabled, #2258)
- fixed the bilinear filter to not readjust the UVs (#2258)
- fixed disabling the cutscenes causing the game to exit (#2743, regression from 4.8)
- fixed anisotropy filter causing black lines on certain GPUs (#902)
- fixed mesh faces not being drawn under some circumstances (#2452, #2438)
- fixed objects disappearing too early around screen edges (#2005)
- fixed the trapezoid filter being toggled if Alt-F4 (either left or right) is used to close the game (#2690)
- fixed enemies in one-click high water appearing with a water tint, and not making any animation sounds (#2753)
- fixed the scale of the four keys in St. Francis' Folly (#2652)
- fixed the panther at times not making a sound when it dies, and restored Skate Kid's death SFX (#2647)
- fixed pushblocks being rotated when Lara grabs them, most noticeable if asymmetric textures have been used (#2776)
- fixed Lara becoming clamped if she picks up an item under a steeply sloped ceiling (#2879)
- fixed a crash when 3D pickups are disabled and Lara crosses a trigger to look at a pickup item (#2711, regression from 4.8)
- fixed trapezoid filter warping on faces close to the camera (#2629, regression from 4.9)
- fixed Mac builds crashing upon start (regression from 4.9)
- fixed sprites rendering black if no shade value is assigned in the level (#2701, regression from 4.9)
- fixed being stuck on the Restart Level page if using save crystals and F5 is pressed when no saves are present (#2700, regression from 4.8.2)
- fixed being stuck on the Exit to Title page if using save crystals and a new save is made when there were previously none, and then F5 is pressed (#2700, regression from 4.9)
- fixed the sprite UVs to restore the right and bottom edge pixels (#2672, regression from 4.8)
- fixed sprites missing the fog effect (regression from 4.9)
- fixed the camera going out of bounds in 60fps near specific invalid floor data (known as no-space) (#2764, regression from 4.9)
- fixed wrong PS1-style title bar color for the end of the level stats dialog (regression from 4.9)
- fixed Story So Far showing up even when there's nothing to play (#2611, regression from 2.10)
- fixed Story So Far not playing the opening FMV, `cafe.rpl` (#2779, regression from 2.10)
- fixed Lara at times ending up in incorrect rooms when using the teleport cheat (#2486, regression from 3.0)
- fixed the `/pos` console command reporting the base room number when Lara is actually in a flipped room (#2487, regression from 3.0)
- fixed clicks in audio sounds (#2846, regression from 2.0)
- fixed Lara being killed if she enters the void in a level that uses the `disable_floor` sequence in the game flow (#2874, regression from 4.9)
- fixed game crashing if the music folder was not present (#2887, regression from 4.9)
- fixed the camera jumping if going from a look at trigger to a fixed camera (#3033, regression from 4.8)
- fixed game crashing on unknown sequencer events

## [4.9](https://github.com/LostArtefacts/TRX/compare/tr1-4.8.3...tr1-4.9) - 2025-03-31
Showcase: https://www.youtube.com/watch?v=AYVpnsYQNno
- added quadrilateral interpolation (#354)
- added `/flood` and `/drain` console commands
- added support for `-l`/`--level` argument to play a single level
- added support for `-s`/`--save` argument to immediately start a saved game
- added support for custom levels to use `disable_floor` in the gameflow, similar to TR2's Floating Islands (#2541)
- added drawing of object mesh spheres to the `/debug` console command
- added TR2+ stats if the full stat detail mode option is enabled (#2561):
    - ammo hits / used
    - health packs used
    - distance travelled
- added a TR2+ style bordered stat box to the end of level stats if the full stat detail mode option is enabled (#2658)
- changed the Controls screen to hide the reset and unbind texts when changing a key (#2103)
- changed injections to a new file format with a smaller footprint and improved applicability tests (#1967)
- changed the `/pos` command to show `Demo` and `Cutscene` instead of `Level` when relevant
- changed the `/pos` command to show demo and cutscene numbers starting at 1, in line with `/play`
- changed the `/play` and `/pos` commands to always treat the gym level as the level 0 – even if it's not included
- changed sprites to respect the water tint if placed underwater (#2093)
- changed the optional `Deaths` stat to be placed last in the stats menu
- fixed delays when scanning available save games (#2610, #1335, regression from <3.0)
- fixed several instances of the camera going out of bounds (#1034)
- fixed issues with stacked, floating and flipmap pushblocks in custom levels
- fixed issues with fixed cameras in 60 FPS shifting before settling on their target (#1186)
- fixed missiles from mutants/centaurs/Natla jittering in 60 FPS (#1314)
- fixed the bear AI fix option being applied in the Vilcabamba demo (#2559, regression from 4.8)
- fixed extremely large item quantities crashing the game (#2497, regression from 0.3)
- fixed Lara's meshes not resetting after using the fly cheat (#2565, #2572, regressions from 4.8)
- fixed the select level feature not giving Lara her items (#2617, regression from 4.8)
- fixed guns appearing in Lara's hands if the draw input is held when unarmed and while picking up a gun item (#2577, regressions from 0.8/4.3)
- fixed being able to play with Lara invisible after using the explosion cheat then the fly cheat (#2584, regression from 4.8)
- fixed the `/pos` command not working in cutscenes
- fixed the `/pos` command not showing demo and cutscene titles
- fixed the embedded bats fix causing problems inside rooms with trapdoors (regression from 4.6)
- fixed cutscene music looping (#2591, regression from 4.8)
- fixed saves created before version 2.15 causing a crash on load (#2654, regression from 4.8)
- fixed the console opening when remapping its key (#2641)
- removed perspective filter toggle (it had no effect; repurposed to trapezoid interpolation toggle)
- improved camera mode navigation:
    - improved support for pivoting
    - improved roll support
    - expanded world bounding box by 5 tiles in each direction
    - added support for 60 FPS

## [4.8.3](https://github.com/LostArtefacts/TRX/compare/tr1-4.8.2...tr1-4.8.3) - 2025-02-17
- fixed some of Lara's speech in the gym not playing in response to player action (#2514, regression from 4.8)
- fixed passport text disappearing too quickly (#2512, regression from 4.8.2)
- fixed inability to navigate to Select Level menu (#2518, regression from 4.8.2)
- fixed NG+ flag causing problems with loading non-NG+ savegames (#2515, regression from 2.8)

## [4.8.2](https://github.com/LostArtefacts/TRX/compare/tr1-4.8.1...tr1-4.8.2) - 2025-02-15
- improved memory usage by shedding ca. 100-110 MB on average
- changed default FPS value to 60 (#2501)
- changed passport to be more responsive to player inputs (#1328)
- fixed Story So Far not skipping over levels (#2506, regression from 4.8)
- fixed resolving paths (especially to music files) on case-sensitive filesystems (#1934, #2504)

## [4.8.1](https://github.com/LostArtefacts/TRX/compare/tr1-4.8...tr1-4.8.1) - 2025-02-14
- fixed loading non-Caves saves triggering a new save prompt when save crystals are enabled (#2498, regression from 4.8)

## [4.8](https://github.com/LostArtefacts/TRX/compare/tr1-4.7.1...tr1-4.8) - 2025-02-14
Showcase: https://www.youtube.com/watch?v=td2Qz3nbRVo
>[!WARNING]
>Attention level builders: this version introduces backwards incompatible changes to the game flow file.
>Please refer to the following documents to see how to update your levels:
>- [Migration guide](../trx/MIGRATING.md)
>- [Game flow documentation](../trx/game_flow/)
>- [Game strings documentation](../trx/GAME_STRINGS.md)

- added the ability to hold left/right to move through menus more quickly (#2298)
- added an option for pickup aids, which will show an intermittent twinkle when Lara is nearby pickup items (#2076)
- added an optional demo number argument to the `/demo` command
- added pause screen support to demos
- added a fade-out effect when exiting the pause screen to the inventory
- added exit fade-out effects (#2348)
- added optional dynamic lighting for gun flashes and explosions, similar to TR2+ (#2357)
- added a `/cut` (alias: `/cutscene`) console command for playing cutscenes
- added a `/gym` (alias: `/home`) console command for playing Lara's Home
- added a `/music` console command that plays a specific music track
- added a `/debug` console command that shows all triggers and portals
- added a console log when using the `/demo` command
- improved pause screen compatibility with PS1 (#2248)
- improved level loading times with respect to injection processing
- improved wireframe mode appearance around screen edges
- ⚠️ changed the game data to use a separate strings file for text information, removing it from the game flow file
- ⚠️ changed the game flow file internal structure
- changed the object texture limit from 2048 to unlimited (within game's overall memory cap)
- changed the sprite texture limit from 512 to unlimited (within game's overall memory cap)
- changed demo to be interrupted only by esc or action keys
- changed the turbo cheat to also affect ingame timer (#2167)
- changed the pause screen to wait before yielding control during fade out effect
- changed the compass and final stats to use two columns, similar to TR2 (doesn't apply to end-of-level "bare" stats)
- changed the fix for transparent eyes on wolves to use black instead of off-white (#2252)
- changed the `/kill` command with no arguments to look for enemies within 5 tiles (#2297)
- fixed blood spawning on Lara from gunshots using incorrect positioning data (#2253)
- fixed ghost meshes appearing near statics in custom levels (#2310)
- fixed photo mode switching to the wrong flipmap rooms at times (#2362)
- fixed the teleporting command sometimes putting Lara in invalid flipmap rooms (#2370)
- fixed teleporting to an item on a ledge sometimes pushing Lara to the room below (#2372)
- fixed secret and enemy speech not playing if the sound effects are missing from the level file (#2458)
- fixed being unable to load a level that contains no sound effect data (#2460)
- fixed input controller remaps not being saved across game relaunches (#2422, regression from 4.6)
- fixed the upside-down camera fix to no longer limit Lara's vision (#2276, regression from 4.2)
- fixed being unable to load some old custom levels that contain certain (invalid) floor data (#2114, regression from 4.3)
- fixed a desync in the Lost Valley demo if responsive swim cancellation was enabled (#2113, regression from 4.6)
- fixed the game hanging when Lara is on fire and enters the fly cheat on the same frame as reaching water (#2116, regression from 0.8)
- fixed Lara activating triggers one frame too early (#2208, regression from 4.3)
- fixed wrong underwater caustics speed with the turbo cheat (#2231)
- fixed 1-frame UI flicker on pause screen exit confirmation
- fixed the game crashing if a cinematic is triggered but the level contains no cinematic frames (#2413)
- fixed missing ricochet sprites in the gym (#2462)
- fixed being able to use keys and puzzle items in keyholes/slots that have already been used (#2256, regression from 4.0)
- fixed textures animating during demo fade-outs (#2217, regression from 4.0)
- fixed waterfall mist not animating during demo (#2218, regression from 3.0)
- fixed sound option arrows disappearing with specific volumes chosen (#2295, regression from 2.7)
- fixed wireframe mode discarding transparent pixels (#2315, regression from 4.2)
- fixed sprite pickup not being paused in the pause/inventory screen (#2319, regression from 4.1)
- fixed 3D pickups not being paused in the pause/inventory screen (#2319, regression from 2.16)
- fixed incorrect sprite sequences potentially animating after visiting a level with valid animating sprites (#2309, regression from 4.0)
- fixed `/kill all` command destroying Scion and causing a soft lock in The Great Pyramid (#2329, regression from 4.4)
- fixed health bar continuing to show when the inventory ring rotates (#1991, regression from 4.0)
- fixed header and arrows disappearing when the inventory ring rotates (#2352, regression from 4.4)
- fixed Story So Far feature not playing opening FMVs from the current level (#2360, regression from 4.2)
- fixed `/play` command crashing the game when used after loading a level (#2411, regression)
- fixed various death counter problems (existing saves will have the count reset) (#2264/#2412, regression from 2.6)
- fixed `/demo` command crashing the game if no demos are present (regression from 4.1)
- fixed Lara not being able to jump or stop swimming if the related responsive config options are enabled, but enhanced animations are not present (#2397, regression from 4.6)
- fixed Lara not being able to jump if responsive jumping is disabled via the console in-level in certain scenarios (#2444, regression from 4.6)
- fixed Lara being unable to climb or use guns after using an underwater lever and then entering the wading state (#2416, regression from 4.6)
- fixed Eidos logo briefly flashing prior to the initial fade-in effect (#1388, regression from 4.1)
- fixed Lara's meshes being incorrectly swapped in various scenarios using the fly cheat (#2461, regression from 4.7)

## [4.7.1](https://github.com/LostArtefacts/TRX/compare/tr1-4.7...tr1-4.7.1) - 2024-12-21
- changed the inventory examine UI to auto-hide if the item description is empty (#2097)
- fixed falling pickup items not being drawn when they land in rare cases (#2088)
- fixed unbinding keys not working for controllers (#2090, regression from 4.6)
- fixed hiding game UI causing the reset progressbar UI element to not show (regression from 4.7)

## [4.7](https://github.com/LostArtefacts/TRX/compare/tr1-4.6.1...tr1-4.7) - 2024-12-20
Showcase: https://www.youtube.com/watch?v=ThXt0I2j_QI
- added support for Wayland in binary Linux builds (#1927)
- added support for Unicode in gameflow JSON (#386, #636, #1919 and #1928)
    Expanding on the 4.6's added support for named sequences, we now support
    most of the characters the following Unicode planes:

    - Basic Latin
    - Cyrillic
    - Greek and Coptic
    - Latin-1 Supplement
    - Latin Extended A

    The sprites were created by Arsunt originally posted in the TRF topic here:
    https://www.tombraiderforums.com/showthread.php?p=8396039

    This should be enough to let gameflow editors provide full localisation for
    the following languages:

    Basque, Belarusian, Bosnian, Bulgarian, Catalan, Croatian, Czech, Danish,
    Dutch, English, Estonian, Faroese, Finnish, French, Galician, German,
    Greek, Hungarian, Icelandic, Indonesian, Irish, Italian, Latvian,
    Lithuanian, Macedonian, Malay, Maltese, Northern Sami, Norwegian, Polish,
    Portuguese, Romanian, Russian, Serbian, Slovak, Slovenian, Spanish,
    Swedish, Turkish and possibly more.

    Importantly, Asian and Arabic languages remain unsupported at the moment.
- added the ability for falling pushblocks to kill Lara outright if one lands directly on her (#2035)
- fixed clock drift accumulating with time (#1935, regression from 4.0)
- fixed a potential invisible wall issue in custom levels with non-portal doors and certain geometry (#1958, regression from 4.3)
- fixed transparent eyes on the wolf and bat models in Peru (#1945)
- fixed incorrect transparent pixels on some Egypt textures (#1975)
- fixed arrows overlapping with passport text if strings longer than the defaults are used (#1971)
- fixed objects close to the camera being clipped (#819, regression from TombATI)
- fixed the drawbridge in Obelisk of Khamoon not being angled correctly when open, which was resulting in embedded artefacts (#2006)
- fixed incorrect positions on static meshes in Obelisk of Khamoon, Return to Egypt and Temple of the Cat (#2006)
- fixed incorrect picture strides on certain hardware (#1979)
- fixed doors at times disappearing if Lara is close to portals and the door's room is no longer visible (#2005)
- fixed camera positions in Return to Egypt and Temple of the Cat (#1317, regression from 4.1)
- fixed being able to see the flipmap in Natla's Mines when moving the boat (#2019)
- fixed an invisible wall in Temple of the Cat, due to a wrongly positioned door (#2021)
- fixed the `enable_console` config option not being adhered to (#2063, regression from 4.5)
- fixed random pixels in the injected explosion sprites (#1985, regression from 4.6)

## [4.6.1](https://github.com/LostArtefacts/TRX/compare/tr1-4.6...tr1-4.6.1) - 2024-11-25
- added ability to disable saves completely by setting the save slot to 0 (#1954)
- fixed invisible walls being present in front of some doors (#1948, regression from 4.6)
- fixed big font scale causing text overlaps in the graphics options (#1946)
- fixed missing FMVs causing the game to go silent (#1931, regression from 4.6)
- fixed game crashing when toggling the bilinear filter in passport (#1942, regression from 4.5)
- fixed game crashing when changing the save slot with `/set` when in passport (#1954, regression from 4.2)

## [4.6](https://github.com/LostArtefacts/TRX/compare/tr1-4.5.1...tr1-4.6) - 2024-11-18
Showcase: https://www.youtube.com/watch?v=raSzSAu7kLI
- added support for wading, similar to TR2+ (#1537)
- added the ability to pause during cutscenes (#1673)
- added an option to enable responsive swim cancellation, similar to TR2+ (#1004)
- added a special target, "pickup", to item-based console commands
- added support for custom levels to enforce values for any config setting (#1846)
- added support for key/puzzle/pickup descriptions, allowing players to examine said items in the inventory (#1821)
- added an option to fix inventory item usage duplication (#1586)
- added optional automatic key/puzzle inventory item pre-selection (#1884)
- added a search feature to the config tool (#1889)
- improved enemy item drops by supporting the TR2+ approach of having drops defined in level data (#1713)
- improved Italian localization for the Config Tool
- improved the injection approach for Lara's responsive jumping (#1823)
- improved the exploding Lara input cheat to always use explosion sprites
- changed OpenGL backend to use version 3.3, with fallback to 2.1 if initialization fails (#1738)
- changed text backend to accept named sequences. Currently supported sequences (limited by the sprites available in OG):
    - `\{umlaut}`
    - `\{hat}`
    - `\{acute accent}`
    - `\{grave accent}`
    - `\{arrow up}`
    - `\{arrow down}`
    - `\{small digit 0}`
    - `\{small digit 1}`
    - `\{small digit 2}`
    - `\{small digit 3}`
    - `\{small digit 4}`
    - `\{small digit 5}`
    - `\{small digit 6}`
    - `\{small digit 7}`
    - `\{small digit 8}`
    - `\{small digit 9}`
    - `\{button empty}`
    - `\{button triangle}`
    - `\{button circle}`
    - `\{button x}`
    - `\{button square}`
    - `\{button l1}`
    - `\{button r1}`
    - `\{button l2}`
    - `\{button r2}`
    - `\{button down}`
    - `\{button up}`
    - `\{button left}`
    - `\{button right}`
    - `\{icon sound}`
    - `\{icon music}`
    - `\{ammo shotgun}`
    - `\{ammo magnums}`
    - `\{ammo uzis}`
- changed the `/pos` command to include the level number and title
- changed the `/tp` command to teleport to items in a round-robin fashion
  The first call will teleport Lara to the object that's the closest to her; repeated calls will cycle through all matching objects in the object placement order.
- changed the music timestamp loading behaviour and config option to support ambient tracks (#1769)
- removed health cheat (we now have the `/hp` command)
- removed background for the "Reset" and "Unbind" labels in the controls dialog
- removed `force_game_modes` and `force_save_crystals` from the gameflow - see GAMEFLOW.md for details on how to enforce these settings (#1857)
- fixed a crash relating to audio decoding (#1895)
- fixed missing pushblock SFX in Natla's Mines (#1714)
- fixed crash reports not working in certain circumstances (#1738)
- fixed missing trapdoor triggers in City of Khamoon (#1744)
- fixed being unable to rename the lead bar (#1774, regression from 4.5)
- fixed the controls menu extending to the bottom of the screen with certain text scaling values (#1783, regression from 2.12)
- fixed game stuck at remapping controller key if no controllers connected (#1788)
- fixed being able to shoot the scion multiple times if save/load is used while it blows up (#1819)
- fixed certain erroneous `/play` invocations resulting in duplicated error messages
- fixed the `/play` console command resulting in Lara starting the target level without pistols (#1861, regression from 4.5)
- fixed the demo mode text overlapping with the enemy health bar if the health bar is located in the bottom centered (#1446)
- fixed mutant explosions sometimes heavily damaging Lara even if they missed (#1758, regression since 4.5)
- fixed wrongly calculated trapdoor size that could affect custom levels (#1904)

## [4.5.1](https://github.com/LostArtefacts/TRX/compare/tr1-4.5...tr1-4.5.1) - 2024-10-14
- fixed mac builds missing embedded resources (#1710, regression from 4.5)

## [4.5](https://github.com/LostArtefacts/TRX/compare/tr1-4.4...tr1-4.5) - 2024-10-08
Showcase: https://www.youtube.com/watch?v=eMnVYbB4QBc
- added a photo mode feature (#1669)
- added `/sfx` command
- added `/nextlevel` alias to `/endlevel` console command
- added `/quit` alias to `/exit` console command
- added an option to toggle the in-game UI, such as healthbars and ammo text (#1656)
- added the ability to cycle through console prompt history (#1571)
- added Lara's exit-water-to-medium-height animation from TR2+ (#1538)
- improved object name matching in console commands to work like TR2X
- improved vertex movement when looking through water portals even more (#1493)
- improved console commands targeting creatures and pickups (#1667)
- changed the easter egg console command to pack more punch
- changed `/set` console command to do fuzzy matching (LostArtefacts/libtrx#38)
- removed dedicated camera reset button in favor of pressing the look button (#1658)
- fixed console caret position off by a couple of pixels (regression from 3.0)
- fixed holding a key when closing the console registering as a game input (regression from 3.0)
- fixed ability to crash the game with extreme FOV values (regression from 0.9)
- fixed double "Fly mode enabled" message when using `/fly` console command (regression from 4.0)
- fixed crash in the `/set` console command (regression from 4.4)
- fixed toggling fullscreen not always saving (regression from 4.4)
- fixed altering fov with `/set` not being immediately respected (#1547)
- fixed main menu music volume when exiting while underwater with certain music settings (#1540, regression from 4.4)
- fixed `/kill` command unable to target a special object
- fixed really fast typing in console sometimes losing the first input (regression from 4.4)
- fixed Lara's head not matching the braid if in use when she is killed by the T-rex (#1549)
- fixed `/endlevel` displaying a success message in the title screen
- fixed Story So Far feature looping cutscenes forever (#1551, regression from 4.4)
- fixed a rare crash related to the camera that could affect custom levels (#1671)
- fixed a bug when saving and loading when picking up an item or using a switch with animated interactions enabled (#1546)
- fixed a bug where Lara was stuck for a long time in an animated interactions if pushed (#1687)

## [4.4](https://github.com/LostArtefacts/TRX/compare/tr1-4.3...tr1-4.4) - 2024-09-20
Showcase: https://www.youtube.com/watch?v=3XOSl9WqH3A
- added `/exit` command (#1462)
- added reflections to Midas Hand death animation and savegame crystals (#154)
- added an option to use PS1 tinted savegame crystals (#1506)
- improved appearance of textures around edges when bilinear filter is off (#1483)
  Since this removes the seams on pushblocks, this was made optional.
- improved level load times (#1456, #1457)
- improved logs module names readability
- improved crash debug information on Windows
- improved vertex movement when looking through water portals (#1493)
- improved anisotropic filter rendering (#902, #1507)
- improved skybox appearance (#1520)
- fixed `/play`, `/load`, `/demo` and similar commands not working in stats, credits, cinematics and fmvs (#1477)
- fixed console commands being able to interfere with demos, cutscenes and the title screen (#1489, regression from 3.0)
- fixed reopening the compass not resetting its needle (#1472, regression from 4.0)
- fixed holstering pistols hiding the gun meshes 1 frame too early (#1449, regression from 0.6)
- fixed Lara's sliding animation sometimes being interrupted by a stumble (#1452, regression from 4.3)
- fixed cameras with glide values sometimes moving in the wrong direction (#1451, regression from 4.3)
- fixed `/give` console command giving duplicate items under some circumstances (#1463, regression from 3.0)
- fixed `/give` console command confusing logging around mismatched items (#1463, regression from 3.0)
- fixed `/give` console command unable to give Scion in Tomb of Qualopec and Sanctuary (regression from 3.0)
- fixed `/flip` console command misreporting an already enabled flipmap as off (regression from 4.0)
- fixed `/kill` console command not fully killing enemies (#1482, regression from 3.0)
- fixed `/tp` console command not always picking the closest item (#1486, regression from 4.1)
- fixed `/tp` console command reporting teleport fails as success (#1484, regression from 4.1)
- fixed `/tp` console command allowing teleporting to consumed savegame crystals (#1518)
- fixed `/hp` console command taking arbitrary integers
- fixed `/set` console command crashing with unknown targets (regression from 4.2)
- fixed `/set` console command not sanitizing numeric values (#1515)
- fixed console commands causing improper ring shutdown with selected inventory item (#1460, regression from 3.0)
- fixed console input immediately ending demo (#1480, regression from 4.1)
- fixed a potential softlock when killing the Torso boss in Great Pyramid (#1236)
- fixed Bacon Lara re-spawning after saving and loading (#1500, regression from 0.7)
- fixed config JSON not sanitizing some numeric values (#1515)
- fixed potential crashes in custom levels if hybrid creature objects are not present in the level (#1444)
- changed the target change functionality from the look key to a new, standalone key (default `z` on keyboard, `left analog click` on controller) (#1503)
- changed `/heal` console command to also extinguish Lara
- changed `/tp` console command to look for the closest place to teleport to when targeting items (#1484)
- changed `/set` console command output to always use fully-qualified option names
- changed `/fps`, `/vsync`, `/wireframe`, `/braid` and `/cheats` console commands output to be in line with `/set` console command output
- changed the door cheat to also target drawbridges

## [4.3](https://github.com/LostArtefacts/TRX/compare/tr1-4.2...tr1-4.3) - 2024-08-15
Showcase: https://www.youtube.com/watch?v=kc2oo-pSMh0
- added deadly water feature from TR2+ for custom levels (#1404)
- added skybox support, with a default option provided for Lost Valley, Colosseum and Obelisk of Khamoon (#94)
- added an option for Lara to use her underwater swimming physics from TR2+ (#1003)
- added weapons to Lara's empty holsters on pickup (#1291)
- added options to quiet or mute music while underwater (#528)
- improved initial level load time by lazy-loading audio samples (LostArtefacts/TR2X#114)
- changed the turbo cheat to no longer affect the gameplay time (#1420)
- changed weapon pickup behavior when unarmed to set any weapon as the default weapon, not just pistols (#1443)
- fixed adjacent Midas Touch objects potentially allowing gold bar duplication in custom levels (#1415)
- fixed the excessive pitch and playback speed correction for music files with sampling rate other than 44100 Hz (#1417, regression from 2.0)
- fixed the ingame timer being skewed upon inventory open (#1420, regression from 4.1)
- fixed Lara able to reach triggers through closed doors (#1419, regression from 1.1.4)
- fixed Lara voiding when loading the game on a closed door (#1419)
- fixed underwater caustics not resumed smoothly when unpausing (#1423, regression from 3.2)
- fixed collision issues with drawbridges, trapdoors, and bridges when stacked over each other, over slopes, and near the ground (#606)
- fixed an issue with a missing Spanish config tool translation for the target mode (#1439)
- fixed carrying over unexpected guns in holsters to the next level under rare scenarios (#1437, regression from 2.4)
- fixed item cheats not updating Lara holster and backpack meshes (#1437)

## [4.2](https://github.com/LostArtefacts/TRX/compare/tr1-4.1.2...tr1-4.2) - 2024-07-14
Showcase: https://www.youtube.com/watch?v=gV7oz0wEzWk
- added creating minidump files on crashes
- added new console commands:
    - `/hp`
    - `/hp [num]`
    - `/heal`
    - `/wireframe`
    - `/set`
- added unobtainable secrets stat support in the gameflow (#1379)
- added a wireframe mode
- changed console caret blinking rate (#1377)
- changed the TR1X install source in the installer to suggest using the existing installation directory (#1350)
- fixed config tool and installer missing icons (#1358, regression from 4.0)
- fixed looking forward too far causing an upside down camera frame (#1338)
- fixed the enemy bear behavior in demo mode (#1370, regression from 2.16)
- fixed the FPS counter overlapping the healthbar in demo mode (#1369)
- fixed the Scion being extremely difficult to shoot with the shotgun (#1381)
- fixed lightning rendering z-buffer issues (#1385, regression from 1.4)
- fixed possible game crashes if more than 16 savegame slots are set (#1374)
- fixed savegame slots higher than 64 not working (#1395)
- fixed a crash in custom levels if a room had more than 1500 vertices (#1398)
- fixed a potential crash or silence with certain music files (#794, regression from 2.0)
- fixed the console command to change FPS also starting demo mode (#1368)
- fixed text blinking rate being different in 30 and 60 FPS (#1377)
- fixed slow sound volume adjustment at 60 FPS when holding arrow keys (#1407)

## [4.1.2](https://github.com/LostArtefacts/TRX/compare/tr1-4.1.1...tr1-4.1.2) - 2024-04-28
- fixed pictures display time (#1349, regression from 4.1)

## [4.1.1](https://github.com/LostArtefacts/TRX/compare/tr1-4.1...tr1-4.1.1) - 2024-04-27
- fixed reading animated texture data in levels (#1346, regression from 4.1)

## [4.1](https://github.com/LostArtefacts/TRX/compare/tr1-4.0.3...tr1-4.1) - 2024-04-26
Showcase: https://www.youtube.com/watch?v=ioo2P0FuFWU
- added ability to show enemy healthbars only for bosses (#1300)
- added ability to kill specific enemy types (#1313)
- added ability to teleport to nearest specific object (#1312)
- added `/load` and `/save` commands for even quicker savegame operations
- added `/demo` command to quickly play the demo
- added `/title` command to quickly exit to title
- added `/vsync on` and `/vsync off` commands to toggle the VSync setting
- added `/give all` variant of the item cheat
- changed injection files to be placed in its own directory (#1306)
- changed item cheat sound effects
- changed the `/play` command to work immediately in the title screen
- fixed turbo cheat speed setting not saved across game relaunches (#1320)
- fixed turbo cheat behavior with the following game elements (#1341):
    - animated textures animation rate (regression from 4.0.3)
    - 3D pickups animation rate (regression from 4.0.3)
    - healthbar flashing rate
    - UI text flashing rate
    - inventory stats timer
    - underwater wibble effect rate
    - loading screen and credit images display time
    - title screen demo delay
    - fade times
- fixed camera vibrations when using the teleport command in 60 FPS (#1274)
- fixed the camera being thrown through doors for one frame when looked at from fixed camera positions (#954)
- fixed console not retaining changed user settings across game relaunches (#1318)
- fixed passport inventory item not being animated in 60 FPS (#1314)
- fixed object explosions not being animated in 60 FPS (#1314)
- fixed lava emitters not being animated in 60 FPS (#1314)
- fixed underwater bubbles not being animated in 60 FPS (#1314)
- fixed compass needle being too fast in 60 FPS (#1316, regression from 4.0)
- fixed black screen flickers that can occur in 60 FPS (#1295)
- fixed a slight delay with the passport menu selector (#1334)
- decreased initial flicker upon game launch (#1322)

## [4.0.3](https://github.com/LostArtefacts/TRX/compare/tr1-4.0.2...tr1-4.0.3) - 2024-04-14
- fixed flickering sprite pickups (#1298)

## [4.0.2](https://github.com/LostArtefacts/TRX/compare/tr1-4.0.1...tr1-4.0.2) - 2024-04-11
- fixed Mac binaries not working on x86-64 (eg not Apple Silicon)
- fixed building on Linux outside of the Docker toolchain (#1296, regression from 4.0)

## [4.0.1](https://github.com/LostArtefacts/TRX/compare/tr1-4.0...tr1-4.0.1) - 2024-04-10
- fixed trying to pick up a lead bar crashing the game (#1293, regression from 4.0)

## [4.0](https://github.com/LostArtefacts/TRX/compare/tr1-3.1.1...tr1-4.0) - 2024-04-09
Showcase: https://www.youtube.com/watch?v=-ED8HSHdHHQ&t=63s
- added experimental support for 60 FPS, available from the in-game graphics menu
- added ability to slow the game down using the turbo cheat (#1215)
- added /speed command to control the turbo cheat (#1215)
- added the option to change weapon targets by tapping the look key like in TR4+ (#1145)
- added three targeting options: full lock always keeps target lock (OG), semi lock loses target lock if the enemy dies, and no lock loses target lock if the enemy goes out of sight or dies (TR4+) (#1146)
- added an option to the installer to install from a CD drive (#1144)
- added stack traces to logs for better crash debugging (#1165)
- added an option to use PS1 loading screens (#358)
- added high quality images for the Eidos, Unfinished Business title, Unfinished Business credit, and final statistics screens
- added support for macOS builds (for both Apple Silicon and Intel)
- added optional support for OpenGL 3.3 Core Profile
- added Italian localization to the config tool
- added the ability to move the look camera while targeting an enemy in combat (#1187)
- added the ability to skip fade-out in stats screens
- added support for animated room sprites in custom levels and an option to animate plant sprites in The Cistern and Tomb of Tihocan (#449)
- added on-screen messages for certain actions (#1220)
- changed stats no longer disappear during fade-out (#1211)
- changed the way music timestamps are internally handled – resets music position in existing saves
- changed vertex and fragment shaders into unified files that are runtime pre-processed for OpenGL versions 2.1 or 3.3
- changed the `/kill` command to use Lara as a reference point, and kill all creatures that are within a single tile first (#1256)
- changed the config not to save key mappings if they do not deviate from the current version's defaults (#1218)
- changed the item cheat keybind to also work in Gym
- changed the item cheat command to display a relevant message if Lara object is not loaded
- fixed a missing translation for the Spanish config tool for the Eidos logo skip option (#1151)
- fixed a flipmap issue in Natla's Mines that could make the cabin appear stacked and prevent normal gameplay (#1052)
- fixed several texture issues across the majority of levels (#1231)
- fixed broken gorilla animations (#1244, regression from 2.15.3)
- fixed saving and loading the music timestamp when the load current music option is enabled and game sounds in inventory are disabled (#1237)
- fixed the remember played music option always being enabled (#1249, regression from 2.16)
- fixed the underwater SFX playing for one frame at the start of Palace Midas (#1251)
- fixed an incorrect frame in Lara's underwater twist animation (OG bug in TR2 onwards) (#1242)
- fixed Lara saying "no" when taking valid actions in front of a key item receptacle (#1268)
- fixed Lara not saying "no" when using the Scion incorrectly (#1278)
- fixed flickering in bats' death animations and rapid shooting if Lara continues to fire when they are killed (#992)
- fixed an incorrect animation in the door used at the beginning of Colosseum (#1287)

## [3.1.1](https://github.com/LostArtefacts/TRX/compare/tr1-3.1...tr1-3.1.1) - 2024-01-19
- changed quick load to show empty passport instead of opening the save game menu when there are no saves (#1141)
- fixed a game crash when the quick load passport is deselected (#1136, regression from 3.1)
- fixed not being able to save in an empty slot using quick save if the load game menu was opened before (#1140, regression from 3.1)
- fixed the passport briefly flashing inaccessible page text (#1137, regression from 3.1)

## [3.1](https://github.com/LostArtefacts/TRX/compare/tr1-3.0.5...tr1-3.1) - 2024-01-14
- added the option to use "shell(s)" to give shotgun ammo in the developer console (#1096)
- added the restart level option to the passport in save crystal mode (#1099)
- added the ability to back out of menus with the circle and triangle buttons when using a gamepad (cross acts as confirm) (#1104)
- changed `force_enable_save_crystals` to `force_save_crystals` for custom level authors to force enable or disable the save crystals setting (#1102)
- changed `force_disable_game_modes` to `force_game_modes` for custom level authors to force enable or disable the game modes setting (#1102)
- changed the Scion in The Great Pyramid from spawning blood when hit to a ricochet effect if texture fixes enabled (#1121)
- changed the gamepad control menu's 'reset all buttons' bind to held R1 (was held triangle) (#1104)
- changed the number of visible enemies from 8 to 32 (#1122)
- fixed FMVs always playing at 100% volume – now they'll play at the game sound volume (#1110)
- fixed bugs when trying to stack multiple movable blocks (#1079)
- fixed Lara's meshes being swapped in the gym level when using the console to give guns (#1092)
- fixed Midas's touch having unrestricted vertical range (#1094)
- fixed flames not being drawn when Lara is on fire but leaves the room where she caught fire (#1106)
- fixed being able to deselect the passport in quick save, quick load, save crystal, and death modes (#1108)
- fixed inability to save in Unfinished Business in crystals mode as UB doesn't have crystals (#1102)
- fixed items not being added to inventory if the sprite is missing from the level file (#1130)
- fixed differences when looking at items from triggers that do not use fixed cameras when the enhanced look option is enabled (#1026)

## [3.0.5](https://github.com/LostArtefacts/TRX/compare/tr1-3.0.4...tr1-3.0.5) - 2023-12-13
- fixed crash when pressing certain keys and the console is disabled (#1116, regression from 3.0)
- fixed lightning bolts wrongly drawn (#1113, regression from 0.9)

## [3.0.4](https://github.com/LostArtefacts/TRX/compare/tr1-3.0.3...tr1-3.0.4) - 2023-12-08
- fixed missiles damaging Lara when she is far beyond their damage range (#1090)
- fixed pushblocks moving freely if Lara releases but tries to regrab during the release animation (#1101, regression from 3.0)

## [3.0.3](https://github.com/LostArtefacts/TRX/compare/tr1-3.0.2...tr1-3.0.3) - 2023-11-27
- fixed underwater shadow effects rendering always in the same way rather than at random (#1081)

## [3.0.2](https://github.com/LostArtefacts/TRX/compare/tr1-3.0.1...tr1-3.0.2) - 2023-11-11
- fixed incorrect usage reference URLs in the gameflow files (#1073)
- fixed random number generation becoming stuck after entering and leaving the inventory, which affected effects and SFX (#1070, #1074)

## [3.0.1](https://github.com/LostArtefacts/TRX/compare/tr1-3.0...tr1-3.0.1) - 2023-11-10
- fixed installer not detecting old Tomb1Main installations (#1071)

## [3.0](https://github.com/LostArtefacts/TRX/compare/tr1-2.16...tr1-3.0) - 2023-11-09
Showcase: https://www.youtube.com/watch?v=vqvOkZzHx6M
- renamed the project from Tomb1Main to TR1X in an effort to establish our own unique identity, while respectfully disassociating from TR2Main.
- added developer console (accessible with `/`, see [2-COMMANDS.md] for details)
- added Linux builds and toolchain
- added an option to allow Lara to roll while underwater, similar to TR2+ (#993)
- added an option to turn off Eidos logo entirely through config (#1044)
- added the bonus level type for custom levels that unlocks if all main game secrets are found (#645)
- added detection for animation commands to play SFX on land, water or both (#999)
- added support for customizable enemy item drops via the gameflow (#967)
- added an option to enable F-key and inventory frame buffering (#591)
- added a pickup overlay for the Midas gold bar when it changes from lead (#1010)
- added an option to allow Lara to creep forwards or backwards further when performing neutral jumps, in line with TR2+ (#998)
- added an option to the installer to choose between the original and fan-made Unfinished Business level sets (#1019)
- fixed baddies dropping duplicate guns (only affects mods) (#1000)
- fixed Lara never using the step back down right animation (#1014)
- fixed dead crocodiles floating in drained rooms (#1031)
- fixed 3d pickups sometimes triggering z-buffer issues (#1015)
- fixed oversized passport in cinematic camera mode (eg when Lara steps on the Midas Hand) (#1009)
- fixed braid being disabled by default unless the player runs the config tool first (#1043)
- fixed various bugs with falling movable blocks (#723)
- fixed the incorrect positioning of door 12 in Tomb of Tihocan (#1063)
- fixed a potential softlock in The Cistern by restoring a missing trigger in room 56 (#1066)
- improved frame scheduling to use less CPU (#985)
- improved and expanded gameflow documentation (#1018)
- rotated the Scion in Tomb of Qualopec to face the the main gate and Qualopec (#1007)

## [2.16](https://github.com/LostArtefacts/TRX/compare/tr1-2.15.3...tr1-2.16) - 2023-09-20
- added a new rendering mode called "framebuffer" that lets the game to run at lower resolutions (#114)  
  (forces players to reset their bilinear filter setting)
- added the current music track and timestamp to the savegame so they now persist on load (#419)
- added the triggered music tracks to the savegame so one shot tracks don't replay on load (#371)
- added forward/backward input detection in line with TR2+ for jump-twists (#931)
- added an option to restore the mummy in City of Khamoon room 25, similar to the PS1 version (#886)
- added a flag indicating if new game plus is unlocked to the player config which allows the player to select new game plus or not when making a new game (#966)
- improved Spanish localization for the config tool
- improved support for windowed mode (#896)
- changed sprite-based pickups to 3D pickups when the 3D pickups option is enabled (#257)
- changed the installer to always overwrite all essential files such as the gameflow and injections (#904)
- changed the data injection system to warn when it detects invalid or missing files, rather than preventing levels from loading (#918)
- changed the gameflow to detect and skip over legacy sequence types, rather than preventing the game from starting (#882)
- moved the enable_game_modes option from the gameflow to the config tool and added a gameflow option to override (#962)
- moved the enable_save_crystals option from the gameflow to the config tool (#962)
- fixed Natla's gun moving while she is in her semi death state (#878)
- fixed an error message from showing on exiting the game when the gym level is not present in the gameflow (#899)
- fixed the bear pat attack so it does not miss Lara (#450)
- fixed some incorrectly rotated pickups when using the 3D pickups option (#253)
- fixed dead centaurs exploding again after saving and reloading (#924)
- fixed the incorrect starting animation on centaurs that spawn from statues (#926, regression from 2.15)
- fixed jump-twist animations at times being interrupted (#932, regression from 2.15.1)
- fixed walk-run-jump at times breaking when TR2 jumping is enabled (OG bug in TR2+) (#934)
- fixed Lara jumping late with TR2 jumping enabled, as compared to normal TR1 jumping when entering the run animation initially (#975)
- fixed the reset and unbind progress bars in the controls menu for non-default bar scaling (#930)
- fixed original data issues where music triggers are not set as one shot (#939)
- fixed a missing enemy trigger in Tomb of Tihocan (#751)
- fixed incorrect trapdoor triggers in City of Khamoon and a switch trigger in Obelisk of Khamoon (#942)
- fixed the setup of two music triggers in St. Francis' Folly (#865)
- fixed data portal issues in Atlantean Stronghold that could result in a crash (#227)
- fixed the camera in Natla's Mines when pulling the lever in room 67 (#352)
- fixed flame emitter saving and loading which caused rare crashing (#947)
- fixed new game plus not working if enable_game_modes was set to false (#960, regression from 2.8)
- fixed Alt-Enter triggering game actions (#979, regression from 2.15)
- fixed Natla spinning in her semi-death and second phases when more than one is active in the level (#906)
- fixed FPS counter, perspective filter and texture filter not always saved when changed from keyboard (#988)

## [2.15.3](https://github.com/LostArtefacts/TRX/compare/tr1-2.15.2...tr1-2.15.3) - 2023-08-15
- fixed Lara stuttering when performing certain animations (#901, regression from 2.14)
- fixed Lara not grabbing certain edges when the swing-cancel option is enabled (#911)

## [2.15.2](https://github.com/LostArtefacts/TRX/compare/tr1-2.15.1...tr1-2.15.2) - 2023-07-17
- fixed Natla not leaving her semi-death state after Lara takes her down for the first time (#892, regression from 2.15.1)

## [2.15.1](https://github.com/LostArtefacts/TRX/compare/tr1-2.15...tr1-2.15.1) - 2023-07-14
- fixed the ape not performing the vault animation when climbing (#880)
- fixed holding down up or down to scroll the passport faster (#883, regression from 2.14)
- fixed Lara becoming stuck in a T-pose on rare occasions after performing a jump tiwst (#889)

## [2.15](https://github.com/LostArtefacts/TRX/compare/tr1-2.14...tr1-2.15) - 2023-06-08
- added an option to enable TR2+ jump-twist and somersault animations (#88)
- added the ability to unbind the sidestep left and sidestep right keys (#766)
- added a cheat to explode Lara like in TR2 and TR3 (#793)
- added an inverted look camera option (#700)
- added a camera speed option for the manual camera (#815)
- added an option to fix original texture issues (#826)
- added menu specific controls meaning arrow keys, return, and escape now always function in menus (#814, regression from 2.12)
- added forward/backward jumps while looking and looking up/down while hanging if enhanced look is enabled (#848)
- added case insensitive directory and file detection (#845)
- added controller detection during runtime (#850)
- added an option to allow cancelling Lara's ledge-swinging animation (#856)
- added an option to allow Lara to jump at any point while running, similar to TR2+ (#157)
- added the ability to define the anchor room for Bacon Lara in the gameflow (#868)
- changed screen resolution option to apply immediately (#114)
- changed shaders to use GLSL 1.20 which should fix most issues with OpenGL 2.1 (#327, #685)
- changed Bacon Lara to prevent movement after her death (#875)
- fixed sounds stopping instead of pausing if game sounds in inventory are disabled (#717)
- fixed skipping Eidos logo and end credits (#541)
- fixed ceiling heights at times being miscalculated, resulting in camera issues and Lara being able to jump into the ceiling (#323)
- fixed Lara not being able to jump off trapdoors or crumbling floors if the sidestep descent fix is enabled (#830)
- fixed walk to pickups feature (#834, regression from 2.8)
- fixed .mpeg FMVs not working (#844)
- fixed the restart level passport text incorrectly showing new game in Lara's Home (#851)
- fixed quick load creating an invalid save if used when no saves are present (#853)
- fixed Lara entering body hit animations when not appropriate to do so (#857)
- fixed SkateKid causing a game crash when too many enemies are active (#866)
- fixed missiles damaging Lara when she is far beyond their damage range (#871)

## [2.14](https://github.com/LostArtefacts/TRX/compare/tr1-2.13.2...tr1-2.14) - 2023-04-05
- added Spanish localization to the config tool
- added an option to launch Unfinished Business from the config tool (#739)
- added dart emitters to the savegame (#774)
- added the ability for level builders to stop all music via triggers (#785)
- added an option to prevent enemy speeches stopping the current music track (#762)
- improved the control of Lara's braid to result in smoother animation and to detect floor collision (#761)
- increased the number of effects from 100 to 1000 (#623)
- changed the health, air, and enemy bars to better match the PS1 version (#698)
- removed the fix_pyramid_secret gameflow sequence (now handled by data injection) (#788)
- fixed Larson's gun textures in Tomb of Qualopec to match the cutscene and Sanctuary of the Scion (#737)
- fixed texture issues in the Cowboy, Kold and Skateboard Kid models (#744)
- fixed the savegame requestor arrow's position with a large number of savegames and long level titles (#756)
- fixed empty holsters when starting a level with the shotgun equipped (#749)
- fixed a crash when taking a screenshot of an opening FMV (#445)
- fixed the animation of Lara's left arm when the shotgun is equipped (#771)
- fixed Lara's braid not turning to gold during the Midas touch animation (#769)
- fixed the equipped weapon's ammo showing on the inventory screen (#777)
- fixed the health, air, and enemy bars from being affected by the text scaling option (#698)
- fixed music triggers with partial masks killing the ambient track (#763)
- fixed the text and bar scaling from being able to be set below the max and min  (#698)
- fixed a data issue in Colosseum, which prevented a bat from triggering (#750)
- fixed lightning and gun flash continuing to animate in the inventory, pause and statistics screens (#767)
- fixed the FPS, healthbar, and arrows from overlapping on the inventory screen (#787)

## [2.13.2](https://github.com/LostArtefacts/TRX/compare/tr1-2.13.1...tr1-2.13.2) - 2023-03-10
- fixed depth buffer size causing rendering issues on some hardware (#748, regression from 2.13)
- fixed a game crash when loading a save in which Lara had been struck by an exploding missile (#746)

## [2.13.1](https://github.com/LostArtefacts/TRX/compare/tr1-2.13...tr1-2.13.1) - 2023-03-03
- added an option to use the PlayStation Uzi sound effects (#152)
- fixed a few flip effect sounds not playing (#743, regression from 2.12.1)
- fixed a game crash when exiting the game with a controller connected (#663)

## [2.13](https://github.com/LostArtefacts/TRX/compare/tr1-2.12.1...tr1-2.13) - 2023-02-19
- added the ability to inject data into levels, with Lara's braid being the initial focus (#27)
- added support for .ogg, .mp3 and .wav formats for audio tracks (#688)
- added the mummy to the level kill stats if Lara touches it and it falls (#701)
- fixed save crystal collision pushing Lara through walls (#682)
- fixed passport animation when deselecting the passport (#703)
- fixed inconsistent wording in config tool health and air color options (#705)
- fixed Scion 1 respawning on load (#707)
- fixed dead water rats looking alive when a room's water is drained (#687, regression from 0.12.0)
- fixed triggered flip effects not working if there are no sound devices (#583)
- fixed the incorrect ceiling textures in Colosseum (#131)

## [2.12.1](https://github.com/LostArtefacts/TRX/compare/tr1-2.12...tr1-2.12.1) - 2023-01-16
- fixed crash when using enhanced saves in levels with flame emitters (#693)
- fixed the death counter from breaking old saves if enhanced saves are turned on (#699)

## [2.12](https://github.com/LostArtefacts/TRX/compare/tr1-2.11...tr1-2.12) - 2022-12-23
- added collision to save crystals (#654)
- added additional custom control schemes (#636)
- added the ability to unbind unessential keys (#657)
- added the ability to reset control schemes to default (#657)
- added customizable controller support (#659)
- added French localization to the config tool (#664)
- fixed small cracks in the UI borders for PS1-style menus (#643)
- fixed Lara loading inside a movable block if she's on a stack near a room portal (#619)
- fixed a game crash on shutdown if the action button is held down (#646)
- fixed the compass and new game menus at high text scaling (#648)
- fixed save crystals so they are single use (#654)
- fixed demo mode if the do not heal on level finish option is used (#660)
- removed the puzzle key sound effect when using save crystals (#654)
- stopped the default controls from functioning when the user unbound them (#564)

## [2.11](https://github.com/LostArtefacts/TRX/compare/tr1-2.10.3...tr1-2.11) - 2022-10-19
- added a .NET-based configuration tool (#633)
- added graphics effects, lava emitters, flame emitters, and waterfalls to the savegame so they now persist on load (#418)
- added an option to turn off sound effect pitching (#625)
- changed passport to highlight latest save at game start (#618)
- fixed some sound effects playing in the inventory when disable_music_in_inventory is true (#486)
- fixed underwater currents breaking in rare cases (#127)
- fixed gameflow option remove_guns preventing weapon pickups in rare situations (#611)
- fixed gameflow option remove_scions causing Lara to equip weapons even if she has none (#605)
- added gameflow option remove_ammo to remove all shotgun, magnum and uzi ammo from the inventory on level start (#599)
- added gameflow option remove_medipacks to remove all medi packs from the inventory on level start (#599)
- improved the UI frame drawing, it will now look consistent across all resolutions and no longer have gaps between the lines
- fixed bridge item in City of Khamoon being incorrectly raised (#627)
- fixed Lara firing blanks indefinitely when she doesn't have pistols and is out of ammo on non-pistol weapons (#629) 

## [2.10.3](https://github.com/LostArtefacts/TRX/compare/tr1-2.10.2...tr1-2.10.3) - 2022-09-15
- fixed save crystal mode always saving in the first slot (#607, regression from 2.8)

## [2.10.2](https://github.com/LostArtefacts/TRX/compare/tr1-2.10.1...tr1-2.10.2) - 2022-08-03
- fixed revert_to_pistols ignoring gameflow's remove_guns (#603)

## [2.10.1](https://github.com/LostArtefacts/TRX/compare/tr1-2.10...tr1-2.10.1) - 2022-07-27
- fixed Lara being able to equip pistols in the gym level (#594)

## [2.10](https://github.com/LostArtefacts/TRX/compare/tr1-2.9.1...tr1-2.10) - 2022-07-26
- added a .NET-based installer
- added the option to make Lara revert to pistols on new level start (#557)
- added the PS1 style UI (#517)
- added the "Story So far..." option in the select level menu to view cutscenes and FMVs (#201)

## [2.9.1](https://github.com/LostArtefacts/TRX/compare/tr1-2.9...tr1-2.9.1) - 2022-06-03
- fixed crash on centaur hatch (#579, regression from 2.9)

## [2.9](https://github.com/LostArtefacts/TRX/compare/tr1-2.8.2...tr1-2.9) - 2022-06-01
- added generic SDL-based controller support (#278)
- added the ability to make freshly triggered (runaway) Pierre replace an already existing (runaway) Pierre (#532)
- added a fade out when completing Lara's Home (#383)
- added the config option to change the number of save slots (#170)
- changed default save slot count to 25 (#170)
- removed DInput-based XBox controller support
- fixed Tihocan chain block sound (#433)
- fixed passport menu with high UI scaling (#546, regression from 2.7)
- fixed passport menu border being off by one pixel (#547)
- fixed the new game and save game passport options using the wrong closing animation (#542, regression from 2.7)
- fixed bridges at floor level appearing under the floor (#523)
- fixed Lara's outfit in Lara's Home when replaying the level (#571, regression from 2.7)
- fixed crash when dying in the gym level with no saves (#576, regression from 2.8)
- fixed exiting select level menu causing deaths in a new game incremented in that slot (#575, regression from 2.8)

## [2.8.2](https://github.com/LostArtefacts/TRX/compare/tr1-2.8.1...tr1-2.8.2) - 2022-05-20
- fixed Lara not picking up items near the edges of room portals (#563, regression from 2.8)

## [2.8.1](https://github.com/LostArtefacts/TRX/compare/tr1-2.8...tr1-2.8.1) - 2022-05-05
- fixed Pierre not resetting across levels (#538, regression from 2.7)
- fixed pushables breaking with flipped rooms when loading a save (#536, regression from 2.8)

## [2.8](https://github.com/LostArtefacts/TRX/compare/tr1-2.7...tr1-2.8) - 2022-05-04
- added the option to pause sound in the inventory screen (#309)
- added level selection to the load game menu (#197)
- added the ability to pick up multiple items at once with walk to items enabled (#505)
- added the ability to skip pictures during fade animation (#510)
- added a cheat to increase the game speed (#135)
- added a matrix stack overflow error check and message if GetRoomBounds runs infinitely (#506)
- added ability to turn off trex collision (#437)
- changed the savegame dialog to remember the user's requested slot number (#514)
- changed the new game dialog to always fall back to new game
- fixed ghost margins during fade animation on HiDPI screens (#438)
- fixed music rolling over to the main menu if main menu music disabled (#490)
- fixed Unfinished Business gameflow not using basic / detailed stats strings (#497, regression from 2.7)
- fixed picking up multiple underwater pickups with walk to items enabled (#500)
- fixed incorrect Lara health when restarting a level
- fixed pushables breaking with flipped rooms when loading a save (#496, regression from 2.6)
- fixed pictures displayed before starting a level causing a black screen (custom levels only)
- fixed underwater caustics animating at 2x speed (#109)
- fixed new game plus infinite ammo carrying over to a loaded game (#535, regression from 2.6)

## [2.7](https://github.com/LostArtefacts/TRX/compare/tr1-2.6.4...tr1-2.7) - 2022-03-16
- added ability to automatically walk to pickups when nearby (#18)
- added ability to automatically walk to switches when nearby (#222)
- added ability to turn off detailed end of the level stats (#447)
- added contextual arrows to passport navigation (#420)
- added contextual arrows to sound option navigation (#459)
- added contextual arrows to controls option navigation (#461)
- added contextual arrows to graphics option navigation (#462)
- added a final statistics screen (#385)
- added music during the credits (#356)
- added fade effects to displayed images (#476)
- added unobtainable pickups and kills stats support in the gameflow (#470)
- fixed exploded mutant pods sometimes appearing unhatched on reload (#423)
- fixed sound effects playing rapidly in sound menu if input held down (#467)

## [2.6.4](https://github.com/LostArtefacts/TRX/compare/tr1-2.6.3...tr1-2.6.4) - 2022-02-20
- fixed crash when loading a legacy save and saving on a new slot (#442, regression from 2.6)

## [2.6.3](https://github.com/LostArtefacts/TRX/compare/tr1-2.6.2...tr1-2.6.3) - 2022-02-18
- fixed croc and rats breaking saves after a flipmap (#441, regression from 2.6)

## [2.6.2](https://github.com/LostArtefacts/TRX/compare/tr1-2.6.1...tr1-2.6.2) - 2022-02-17
- fixed equipping gun after starting a demo (#440, regression from 2.6)

## [2.6.1](https://github.com/LostArtefacts/TRX/compare/tr1-2.6...tr1-2.6.1) - 2022-02-16
- fixed equipping gun after starting the game (#439, regression from 2.6)

## [2.6](https://github.com/LostArtefacts/TRX/compare/tr1-2.5...tr1-2.6) - 2022-02-16
- added deaths counter (#388, requires new saves)
- added total pickups and kills per level to the compass and end level stats screens (#362)
- added new, more resilient savegame format (#277)
- added ability to give Lara various items in the gameflow file
- added restart level to passport menu on death (#48)
- changed Lara's starting health to be configurable; useful for no damage runs (#365)
- changed saves to be put in the saves/ directory (#87)
- changed fade animations to block the main menu inventory ring like in PS1 (#379)
- changed fade animations to be FPS-independent
- changed fade animations to run faster in the main menu
- changed compass text order to be consistent with level stats (#415)
- fixed detail levels text flashing with any option change (#380)
- fixed main menu demo playing even when the passport is open (#410, regression from 2.1)
- fixed broken poses at the end of cinematics (#390)
- fixed libavcodec-related memory leaks (#389)
- fixed crash in custom levels that call `level_stats` after playing an FMV (#393, regression from 2.5)
- fixed calling `level_stats` for different levels (#336, requires new saves)
- fixed sounds playing after demo mode ends when game is minimized (#399)
- fixed glitched floor in the Natla cutscene (#405)
- fixed gun pickups disappearing in rare circumstances on save load (#406)
- fixed equipping gun after loading a legacy save (#427, regression from 2.4)
- fixed empty mutant shells in Unfinished Business spawning Lara's hips (#250)
- fixed rare audio distance glitch (#421)
- fixed Lara not getting her pistols in Atlantis if the player finishes Natla's Mines without picking up any gun (#424)
- fixed broken dart ricochet effect (#429)

## [2.5](https://github.com/LostArtefacts/TRX/compare/tr1-2.4...tr1-2.5) - 2022-01-31
- added CHANGELOG.md
- added ability to skip cinematics with the Action key
- added fade animations (#363)
- added a vsync option (#364)
- fixed certain inputs skipping too many things (#359)
- fixed a memory leak in the audio sampler (#369)


## [2.4](https://github.com/LostArtefacts/TRX/compare/tr1-2.3...tr1-2.4) - 2022-01-19
- added ability to skip FMVs with the action key (#334)
- changed shaders to use GLSL version 1.30 (#327)
- changed savegames to consume less space
- fixed ingame overlay (bars and ammo) being sometimes shown in the menus
- fixed menu backgrounds not being shown on certain platforms (#324)
- fixed Lara reverting back to pistols when finishing a level with another gun (#338)
- fixed lava wedge not setting Lara on fire (#353, regression from 2.2)
- fixed fallback game strings not working (#335, regression from 2.3)
- fixed high DPI window scaling on Windows (#280)
- fixed not all sounds being muted when minimizing the game (#349)
- fixed ability to push movable blocks through doors (#46)
- fixed showing inventory ring up/down arrows when uncalled for (#337)
- fixed Tomb1Main.log to be placed in the game directory rather than the current working directory
- fixed a crash when exiting the game (regression from 2.3)
- fixed a crash when shader compilation fails


## [2.3](https://github.com/LostArtefacts/TRX/compare/tr1-2.2.1...tr1-2.3) - 2022-01-12
- added ability to hold down forward/back to move through saves faster (#171)
- changed screenshots to be saved in its own folder and with more meaningful names (#255)
- fixed audible clicks near the end of samples (#281)
- fixed secret chime not playing if the secret sound fix is disabled, and nothing plays between consecutive secret pickups (#310)
- fixed ambient noises not pausing on pause screen (#316)
- fixed underwater sound effect playing only once (#305)
- fixed UZI sound stopping near big mutant explosions
- fixed switching inventory rings briefly displaying black frames (#75)
- fixed top offscreen load game selection (#273, #304)
- fixed Lara voiding through static objects (#299)
- fixed step left controller input not working (#302, regression from 2.0)
- fixed memory leaks


## [2.2.1](https://github.com/LostArtefacts/TRX/compare/tr1-2.2...tr1-2.2.1) - 2022-01-05
- fixed listing available resolutions (a regression from 2.2)
- fixed Lara's airbar showing up when Lara's dead (a regression from 2.1)


## [2.2](https://github.com/LostArtefacts/TRX/compare/tr1-2.1...tr1-2.2) - 2022-01-05
- added ability to control anisotropy filter strength
- changed the engine look for HD FMVs by default for Unfinished Business
- removed tiny screen resolutions (might require setting the resolution again)
- fixed Lara getting set on fire on trapdoors over lava
- fixed letterbox in main menu showing garbage data on certain machines
- fixed save crystals saving before gym level
- fixed black lines appearing on walls and floors
- fixed hang bug for stacked rooms


## [2.1](https://github.com/LostArtefacts/TRX/compare/tr1-...tr1-2.1) - 2021-12-21
- added ability to disable healthbar and airbar flashing
- changed the engine look for HD FMVs by default
- increased max active samples to 20 (should fix rare mute sounds issues)
- fixed loading TombATI Atlantis saves
- fixed shotgun shooting when target out of sight
- fixed save selection being offscreen if the first savegame starts with high enough number
- fixed alligators dealing no damage under certain circumstances
- fixed grabbing bridges under certain circumstances
- fixed crash if user presses a key during ring close animation

## [2.0.1](https://github.com/LostArtefacts/TRX/compare/tr1-2.0...tr1-2.0.1) - 2021-12-13
Added an icon to the .exe (thanks TRFan94!)


## [2.0](https://github.com/LostArtefacts/TRX/compare/tr1-1.4.0...tr1-2.0) - 2021-12-07
Shipped our own .exe! Tomb1Main is now fully open source and no longer needs injecting itself to the game. It also no longer depends on any of the TombATI .dll files. You can have both versions installed in the same folder.
- added support for HD FMVs
- added support for .png and .jpg pictures
- added support for .png and .jpg screenshots
- added fanmade 16:9 menu backgrounds
- added wine support
- added ability to run the game from any directory (its CWD no longer needs to point to the game's directory)
- changed music player to SDL
- changed sample player to SDL
- changed FMV player to libavcodec and SDL
- changed Eidos logo and initial FMVs to be stored in the gameflow file
- changed Unfinished Business to no longer play cafe.rpl
- changed the game no longer switches resolution back and forth in windowed mode
- changed T1M no longer reads atiset.dat
- improved shaders readability (chroma key is now stored in the texture alpha channel)
- improved shader performance a bit when the bilinear filter is off
- improved 3D rendering performance a bit (no more C++ exception handling)
- fixed brightness not being saved
- fixed game exiting with "Fatal DirectInput error" when losing focus early


## [1.4.0](https://github.com/LostArtefacts/TRX/compare/tr1-1.3.0...tr1-1.4.0) - 2021-11-16
- added adjustable ingame brightness
- added per-level fog settings
- added control over fog density (in terms of tiles)
- improved TR3 sidesteps
- improved wording in readme
- fixed lighting for 3D pickups
- fixed a crash when drawing lightnings
- fixed a crash when compiling the game on MSVC


## [1.3.0](https://github.com/LostArtefacts/TRX/compare/tr1-1.2.2...tr1-1.3.0) - 2021-11-06
- added version in the bottom right corner
- added movable camera on W,A,S,D
- added Xbox One Controller support
- added rounded shadows (instead of the default octagon)
- added per-level customizable water color (with customizable blue component)
- added rendering of pickups on the ground as 3D meshes
- added the ability to change resolution in-game
- added optional fixes for the following original game glitches:
  - slope/wall bug ("bonk to ascend" bug)
  - breakable tiles bug ("sidestep to descend" bug)
  - qwop
- changed maximum textures from 2048 to 8192
- changed maximum texture pages from 32 to 128
- changed default level skip cheat key from X to L
- removed hard limit of 1024 rooms
- fixed level skip working in inventory (it would apply only after closing the inventory)
- fixed bats being positioned too high
- fixed flashing conflicts when cheat buttons are disabled
- fixed ability to rebind the pause button


## [1.2.2](https://github.com/LostArtefacts/TRX/compare/tr1-1.2.1...tr1-1.2.2) - 2021-10-17
- added ability to mute music in main menu
- added pausing the music while in pause
- added more screen resolutions
- fixed demos playing oddly when the enhanced look option is enabled
- fixed shadows rendering
- fixed too big healthbar margins on low resolutions
- fixed bilinear filter not working
- fixed resolution width/height being ignored


## [1.2.1](https://github.com/LostArtefacts/TRX/compare/tr1-1.2.0...tr1-1.2.1) - 2021-10-17
- added resolution_width and resolution_height to the default settings
- fixed screen resolution regression from 1.2.0


## [1.2.0](https://github.com/LostArtefacts/TRX/compare/tr1-1.1.5...tr1-1.2.0) - 2021-10-15
- fixed a common crash on many machines


## [1.1.5](https://github.com/LostArtefacts/TRX/compare/tr1-1.1.4...tr1-1.1.5) - 2021-10-13
- fixed a regression resulting in crashes from 1.1.4


## [1.1.4](https://github.com/LostArtefacts/TRX/compare/tr1-1.1.3...tr1-1.1.4) - 2021-10-13
- fixed problem with the alt key on certain machines
- fixed a rare crash on certain machines


## [1.1.3](https://github.com/LostArtefacts/TRX/compare/tr1-1.1.2...tr1-1.1.3) - 2021-03-30
- changed smooth bars to be enabled by default
- changed end of level freeze fix can no longer be disabled
- changed creature distance fix can no longer be disabled
- changed pistols + key triggers fix can no longer be disabled
- changed illegal gun equip fix can no longer be disabled
- changed FMV escape key fix can no longer be disabled
- changed input to DirectInput
- fixed switchin Control keys when shimmying causing Lara to drop
- fixed some anomalies around FPS counter within ingame menus
- fixed controls UI missing its borders


## [1.1.2](https://github.com/LostArtefacts/TRX/compare/tr1-1.1.1...tr1-1.1.2) - 2021-03-30
- fixed main menu demo mode not playing correctly (regression from 1.1.1)
- fixed game speeding up on certain machines (regression from 1.1.1)


## [1.1.1](https://github.com/LostArtefacts/TRX/compare/tr1-1.1...tr1-1.1.1) - 2021-03-29
- added deactivating game when Alt-Tabbing
- improved pink bar color
- fixed sounds volume slider not working for ingame sounds


## [1.1](https://github.com/LostArtefacts/TRX/compare/tr1-1.0...tr1-1.1) - 2021-03-28
- added an alert messagebox whenever something bad (within the code's expectations) happens
- added smooth bars (needs to be explicitly enabled in the settings)
- finished porting the input and sound routines
- fixed custom bar colors not working in certain levels
- fixed RNG not being seeded (no practical consequences on the gameplay)


## [1.0](https://github.com/LostArtefacts/TRX/compare/tr1-0.13.3...tr1-1.0) - 2021-03-21
- added pause screen
- added -gold command line switch to run Unfinished Business


## [0.13.3](https://github.com/LostArtefacts/TRX/compare/tr1-0.13.2...tr1-0.13.3) - 2021-03-21
- added crystals mode (can be enabled in the gameflow)
- improved navigation through keyboard controls UI
- fixed Unfinished Business gameflow not loading
- fixed OG conflicting controls not flashing after relaunching the game
- fixed drawing Lara's hair when she carries shotgun on her back
- fixed loading custom layouts that conflict with default controls


## [0.13.2](https://github.com/LostArtefacts/TRX/compare/tr1-0.13.1...tr1-0.13.2) - 2021-03-19
- fixed lighting issues (Lara being sometimes very brightly lighted)


## [0.13.1](https://github.com/LostArtefacts/TRX/compare/tr1-0.13.0...tr1-0.13.1) - 2021-03-19
- changed demo_delay constant to be stored in the gameflow file
- fixed regression in LoadSamples


## [0.13.0](https://github.com/LostArtefacts/TRX/compare/tr1-0.12.7...tr1-0.13.0) - 2021-03-19
- added display_time parameter to display_picture (requires overwriting your gameflow file)
- added user controllable UI and bar scaling
- changed limit of max items (moveables in TRLE lingo) from 256 to 10240
- fixed whacky navigation in controls dialog if cheats are enabled
- fixed regression in LoadItems that crashes Atlantis
- fixed skipping pictures displayed before starting the level with the escape key causing inventory to open


## [0.12.7](https://github.com/LostArtefacts/TRX/compare/tr1-0.12.6...tr1-0.12.7) - 2021-03-19
- added ability to remap cheat keys (except obscure f11 debug key)
- changed f10 level skip cheat key to 'x' (can be now changed); had to be done because the game does not let mapping to function keys
- changed lots of variables to stay in T1M memory (may cause regressions)
- changed runtime game config to be read and written to a new JSON configuration rather than atiset.cfg
- changed files directory placement to a new directory, cfg/


## [0.12.6](https://github.com/LostArtefacts/TRX/compare/tr1-0.12.5...tr1-0.12.6) - 2021-03-18
- fixed loading game in Natla's Mines causing Lara to lose her guns


## [0.12.5](https://github.com/LostArtefacts/TRX/compare/tr1-0.12.4...tr1-0.12.5) - 2021-03-17
- fixed collected secrets resetting after using compass


## [0.12.4](https://github.com/LostArtefacts/TRX/compare/tr1-0.12.3...tr1-0.12.4) - 2021-03-17
- added showing level stats in compass (can be disabled)
- added ability to disable game mode selection in gameflow
- added fallback gameflow strings (in case someone installs new T1M but forgets to't override the gameflow file)
- added ability to exit level stats with escape
- changed ingame timer to tick also in the inventory (can be disabled)
- changed bar sizes and location to match TR2Main
- fixed reading key configuration for keys that override defaults
- fixed calculating creature distances (fixes Tihocan croc bug)


## [0.12.3](https://github.com/LostArtefacts/TRX/compare/tr1-0.12.2...tr1-0.12.3) - 2021-03-17
- add Japanese mode (enemies are 2 times weaker)
- improve skipping cutscenes
- fix crash when FMVs are missing (this doesn't add support for HQ FMVs though)


## [0.12.2](https://github.com/LostArtefacts/TRX/compare/tr1-0.12.1...tr1-0.12.2) - 2021-03-14
- changed settings to save after each change
- fixed OG music stopping when playing the secrets chime (can be disabled)
- fixed OG game not saving key layout choice (default vs. user keys)
- fixed OG volume slider not working when starting muted
- fixed OG holding action to skip credit pictures skipping them all at once
- fixed OG holding escape to skip FMVs opening inventory


## [0.12.1](https://github.com/LostArtefacts/TRX/compare/tr1-0.12.0...tr1-0.12.1) - 2021-03-14
- huge internal refactors
- improved door open cheat
- changed 4k scaling path to be always enabled (previously known as enable_enhanced_ui)
- fixed killing music underwater
- fixed main menu background for UB


## [0.12.0](https://github.com/LostArtefacts/TRX/compare/tr1-0.11.1...tr1-0.12.0) - 2021-03-12
- introduced gameflow sequencer (moves FMVs, cutscenes, level stats etc. logic to the gameflow JSON file); add ability to control number of levels
- refactored gameflow
- added ability to disable cinematic scenes
- changed automatic calculation of secret count to be always enabled
- fixed starting NG+ from gym not working
- fixed cinematics resetting FOV


## [0.11.1](https://github.com/LostArtefacts/TRX/compare/tr1-0.11...tr1-0.11.1) - 2021-03-11
- added ability to turn off main menu demos
- added ability to turn off FMVs
- added reporting JSON parsing errors in the logs
- fixed reading config sometimes not working
- fixed killing music in the inventory
- fixed missing Demo Mode text
- fixed showing Eidos logo for too short
- fixed Lara wearing normal clothes in Gym


## [0.11](https://github.com/LostArtefacts/TRX/compare/tr1-0.10.5...tr1-0.11) - 2021-03-11
- introduced gameflow file (moves all game strings to a gameflow JSON file, including level paths and names); level number, FMVs etc. are still hardcoded


## [0.10.5](https://github.com/LostArtefacts/TRX/compare/tr1-0.10.4...tr1-0.10.5) - 2021-03-10
- added arrows to save/load dialogs
- improved user keys settings dialog - you don't have to hold the key for exactly 1 frame anymore
- made new game dialog smaller
- fixed passport closing when exiting new game mode selection dialog


## [0.10.4](https://github.com/LostArtefacts/TRX/compare/tr1-0.10.3...tr1-0.10.4) - 2021-03-08
- fixed load game screen


## [0.10.3](https://github.com/LostArtefacts/TRX/compare/tr1-0.10.2...tr1-0.10.3) - 2021-03-08
- added NG/NG+ mode selection


## [0.10.2](https://github.com/LostArtefacts/TRX/compare/tr1-0.10.1...tr1-0.10.2) - 2021-03-07
- fixed fly cheat resurrection with lava wedges


## [0.10.1](https://github.com/LostArtefacts/TRX/compare/tr1-0.10...tr1-0.10.1) - 2021-03-07
- improved dealing with missing config
- renamed config to .json5
- fixed sound going off after playing a cinematic


## [0.10](https://github.com/LostArtefacts/TRX/compare/tr1-0.9.2...tr1-0.10) - 2021-03-06
- added support for opening closest doors


## [0.9.2](https://github.com/LostArtefacts/TRX/compare/tr1-0.9.1...tr1-0.9.2) - 2021-03-05
- fixed messged up FMV sequence IDs
- fixed crash when drawing lightnings near Scion


## [0.9.1](https://github.com/LostArtefacts/TRX/compare/tr1-0.9...tr1-0.9.1) - 2021-03-04
- fixed bats flying near floor
- fixed typo in Tomb1Main.json causing everything to be disabled


## [0.9](https://github.com/LostArtefacts/TRX/compare/tr1-0.8.3...tr1-0.9) - 2021-03-03
- added FOV support (overrides GLrage completely, but should be compatible with it)
- added support for more than 3 pickups at once (for TRLE builders)
- fixed smaller pickup sprites
- fixed showing FPS in the main menu doing weird stuff to the inventory text after starting the game


## [0.8.3](https://github.com/LostArtefacts/TRX/compare/tr1-0.8.2...tr1-0.8.3) - 2021-02-28
- improved TR3-like sidesteps
- improved bar flashing modes
- fixed Lara targeting enemies even after death
- fixed version information missing from releases


## [0.8.2](https://github.com/LostArtefacts/TRX/compare/tr1-0.8.1...tr1-0.8.2) - 2021-02-28
- fixed Lara drawing guns when loading OG saves


## [0.8.1](https://github.com/LostArtefacts/TRX/compare/tr1-0.8...tr1-0.8.1) - 2021-02-27
- fixed AI sometimes having problems to find Lara
- fixed shotgun firing sound after running out of ammo
- fixed OG being able to get pistols by running out of ammo in other weapons, even without having them in the inventory


## [0.8](https://github.com/LostArtefacts/TRX/compare/tr1-0.7.6...tr1-0.8) - 2021-02-27
- added optional TR3-like sidesteps
- added "never" to healthbar display modes (so that you can run without ever knowing your health!)
- added airbar display modes (so that you can swim without ever knowing your remaining oxygen)
- added experimental braid, off by default (works only in Lost Valley due to other levels having no braid meshes)
- added version information (it's in file properties)
- changed turning fly cheat on above water no longer causes Lara to create bubbles for 1 frame
- changed turning fly cheat on after stepping on Midas hand and getting eaten by T-Rex now resets Lara's appearance back to normal
- change turning fly cheat on while burning now extinguishes Lara
- increase the chance for the player to resurrect Lara with fly cheat after dying (up to 10 s, but it has to be the first keystroke they press)
- fixed T1M bug - holding fly cheat and WALK resulting in hoisting Lara up
- fixed T1M bug - added ability to draw last selected weapon with numkeys
- fixed OG bug - keys and puzzles not triggering after drawing guns
- fixed OG bug - having to draw guns via inventory after picking them up in Natla's Mines
- fixed OG crash when Lara is on fire and walks too far away from where she caught fire


## [0.7.6](https://github.com/LostArtefacts/TRX/compare/tr1-0.7.5...tr1-0.7.6) - 2021-02-23
- fixed Atlanteans behavior


## [0.7.5](https://github.com/LostArtefacts/TRX/compare/tr1-0.7.4...tr1-0.7.5) - 2021-02-22
- fixed ammo text placement
- fixed healthbar placement in the inventory


## [0.7.4](https://github.com/LostArtefacts/TRX/compare/tr1-0.7.3...tr1-0.7.4) - 2021-02-22
- added support for user-configured bar colors
- switched configuration format to use JSON5
- moved comments to Tomb5Main.json
- fixed bar placement


## [0.7.3](https://github.com/LostArtefacts/TRX/compare/tr1-0.7.2...tr1-0.7.3) - 2021-02-22
- added support for user-configured bar locations
- fixed pickups scaling


## [0.7.2](https://github.com/LostArtefacts/TRX/compare/tr1-0.7.1...tr1-0.7.2) - 2021-02-22
- fixed ability to look around while Lara's dead
- fixed UI scaling in controls dialog
- fixed crash for some creatures


## [0.7.1](https://github.com/LostArtefacts/TRX/compare/tr1-0.7...tr1-0.7.1) - 2021-02-22
- added inventory cheat
- made fly cheat faster


## [0.7](https://github.com/LostArtefacts/TRX/compare/tr1-0.6...tr1-0.7) - 2021-02-21
- added fly cheat
- fixed a crash when hit by a lightning (T1M regression)
- fixed missing "Demo Mode" text (T1M regression)


## [0.6](https://github.com/LostArtefacts/TRX/compare/tr1-0.5.1...tr1-0.6) - 2021-02-20
- changed the code to count secrets automatically (useful for custom level builders)
- fixed secret trigger in The Great Pyramid
- fixed a crash when loading levels with more than 1024 textures
- fixed drawing Lara (T1M regression)


## [0.5.1](https://github.com/LostArtefacts/TRX/compare/tr1-0.5...tr1-0.5.1) - 2021-02-20
- added fire sprite to shotgun


## [0.5](https://github.com/LostArtefacts/TRX/compare/tr1-0.4.1...tr1-0.5) - 2021-02-18
- renamed the project from TR1Main to Tomb1Main on the request of Arsunt
- improved documentation


## [0.4.1](https://github.com/LostArtefacts/TRX/compare/tr1-...tr1-0.4.1) - 2021-02-15
- added an option to always show the healthbar
- fixed enemy healthbars in NG+
- fixed no heal mode

## [0.4](https://github.com/LostArtefacts/TRX/compare/tr1-0.3.1...tr1-0.4) - 2021-02-14
- added UI scaling
- added ability to look around underwater


## [0.3.1](https://github.com/LostArtefacts/TRX/compare/tr1-...tr1-0.3.1) - 2021-02-13
- improved the ability to look around while running

## [0.3](https://github.com/LostArtefacts/TRX/compare/tr1-0.2.1...tr1-0.3) - 2021-02-13
- added an option disable magnums
- added an option disable uzis
- added an option disable shotgun
- added ability to look around while running
- added support for using items with numeric keys
- fixed an OG bug with the secret sound in Tomb of Tihocan


## [0.2.1](https://github.com/LostArtefacts/TRX/compare/tr1-0.2...tr1-0.2.1) - 2021-02-11
- changed the default configuration to enable enemy healthbars, red healthbar and end of the level freeze fix


## [0.2](https://github.com/LostArtefacts/TRX/compare/tr1-0.1...tr1-0.2) - 2021-02-11
- added enemy healthbars
- added a red healthbar


## [0.1](https://github.com/LostArtefacts/TRX/compare/...tr1-0.1) - 2021-02-10

Initial version.
