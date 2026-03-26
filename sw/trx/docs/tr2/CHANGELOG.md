## [Unreleased](https://github.com/LostArtefacts/TRX/compare/tr2-1.5.1...develop) - ××××-××-××
See [/docs/CHANGELOG.md].

## [1.5.1](https://github.com/LostArtefacts/TRX/compare/tr2-1.5...tr2-1.5.1) - 2025-10-10
- changed the examine dialog to be usable with non-puzzle items (#4009)
- fixed discs that spawn from emitters facing East starting too far from the emitters themselves (#4007)
- fixed a crash on game exit if specifying "ambient_tracks" in the game flow root (regression from 1.1)
- fixed alternate ambient tracks being lost on reload in custom levels (#3997, regression from 1.4)
- fixed a crash if the game had certain objects (id=17, 18, 41, 43, 50) but was missing their reference objects (id=16, 16, 42, 44, 49, respectively)
- fixed Lara at times not being able to grab pushblocks despite being in the correct position to do so (#4005, regression from 1.5)
- fixed enemies being unable to smash windows (#4011, regression from 1.4)
- fixed Lara appearing flat for a frame during the neutral twist, controlled drop and ledge jump back animations (#4012, regression from 1.4)

## [1.5](https://github.com/LostArtefacts/TRX/compare/tr2-1.4.2...tr2-1.5) - 2025-10-04
Showcase: https://youtu.be/ClkbvsENSvc
- added an option to use Lara's barefoot sound effects in appropriate levels (Sound options → Barefoot SFX) (#2643)
- added dev console gradient backdrop, similar to TR1X (#2150)
- added an option to use smooth bars (Graphics → UI → Smooth bars, default off)
- added an option to use TR1-style UI bars (Graphics → UI → Bars look)
- added an option to use PS1-style UI bars (Graphics → UI → Bars look) (#1637)
- added an option to use PS1-style UI backgrounds and frames (Graphics → UI → Menu style) (#1635)
- added an option to use PS1-style carpet texture animation (Graphics → UI → Background style) (#1630)
- added an option to change target lock modes (Gameplay → Controls → Weapon lock mode) (#3950)
- added an option to cycle targets (Gameplay → Controls → Target change; Controls → Misc → Change Target) (#3951)
- added a new `/cls` / `/clear` console command to quickly clear console logs
- added an option to turn off ingame timer in the inventory ring (Gameplay → General → Timer counts in inventory) (#3931)
- added an option to disable demos (Gameplay → General → Demo mode)
- added an option to disable music in the title screen (Sound → Misc → Main menu music)
- improved sound settings:
    - added tabs (Volume and Misc)
    - added a dedicated option to control master volume (Sound options → Volume → Master volume)
    - added a dedicated option to control cutscenes volume (Sound options → Volume → Cutscenes volume) (#3490)
    - added a dedicated option to control FMV volume (Sound options → Volume → FMV volume) (#3490)
    - added a dedicated option to control general ambient volume (Sound options → Volume → Ambient volume) (#3707)
    - added an option to turn off sound effect pitching (#625)
    - improved volume settings to accept slow input for finer adjustments
    - fixed changing sound volume not updating certain ambient sound sources while in the inventory ring (#3970)
- changed OG glitch-related config options to be on/fixed by default (#3929)
- changed the Use PSX FOV option name to Use PS1 FOV (Graphics → Visuals → Use PS1 FOV)
- changed the UI style to use the PS1 look by default (Graphics → UI → Menu style)
- changed the bar appearance to use the PS1 look by default (Graphics → UI → Bars look)
- changed the inventory and stats screen to use the PS1 wave animation by default (Graphics → UI → Background style)
- changed idle pose timeout from 15 to 60 seconds by default (Gameplay → Controls → Idle pose timeout)
- changed idle pose camera to be disabled by default (Gameplay → Controls → Idle pose camera)
- changed the game to launch in fullscreen mode by default (Alt-Enter to toggle)
- changed max pickup scale to 200% (#3952)
- fixed pickup scale being greyed out if the 3D pickups option is enabled (#3952)
- fixed trapdoor type 3 (object #116) not functioning (#3895)
- fixed camera stutter when shimmying on ladders to the left (#3904, regression from 1.3)
- fixed gameplay settings UI displaying eagerly after the first use (#3583, regression from 1.3)
- fixed changing FPS after advancing frames in photo mode causing the game to speed up (#3605, regression from 1.3)
- fixed CPU spike during playing FMVs (regression from 0.6)
- fixed `/play` command likely to skip opening FMVs (#3910, regression from 0.8)
- fixed resumed music tracks playing briefly track start upon savegame load (#3916)
- fixed highlight size in health and air bars
- fixed a potential crash when loading a save where Lara is holding a flare (#3924, regression from 1.0)
- fixed unrestricted look mode allowing cinematic cameras to be broken out of (#3926, regression from 1.4)
- added the ability for falling movable blocks to kill Lara outright if one lands directly on her (#3784)
- fixed numerous interactions with movable blocks, trapdoors, drawbridges, bridges, lifts, and falling blocks for custom levels (#2758):
    - added the ability for movable blocks to move on trapdoors, drawbridges, bridges, lifts, and falling blocks
    - added the ability for stacks of movable blocks to fall and land on trapdoors, drawbridges, bridges, lifts, and falling blocks
    - added the ability for stacks of movable blocks to fall when on opened trapdoors and drawbridges
    - added the ability for movable blocks to travel up and down lifts
    - fixed various bugs with falling movable blocks
- fixed Lara hang climbing up a movable block used as a ladder piece (#3828)
- fixed pushblocks becoming unusable when on the same sector as a door that does not sit on a room portal (#3814)
- fixed pushblocks that fall from a great height potentially causing a crash (#3969)
- fixed a rare crash if the t-rex is killed with a grenade and many other enemies are active (#3938)
- fixed dead skidoo drivers not registering with combat end after loading a save (#3966)
- fixed recordings replaying commands twice (regression from 1.4)
- fixed the fix for the sticky corner glitch not being optional - now linked to Gameplay → Fixes → Wall glitch mode (#3957, regression from 1.4)
- fixed Lara shooting rifle-type weapons drawn during wade to float transition (#3986)
- fixed Lara retaining guns if drawn during wade to float transition (#3979, regression from 1.3)
- fixed Lara instantly holstering harpoon when drawing in 2+ click water (#3980, regression from 1.3)
- fixed -s/--save argument no longer working with -l/--level (#3990, regression from 1.4)

## [1.4.2](https://github.com/LostArtefacts/TRX/compare/tr2-1.4.1...tr2-1.4.2) - 2025-09-07
- fixed broken rendering in MacOS releases (#3880, regression from 1.4)
- fixed images from MacOS releases (#3892, regression from 1.4)
- fixed the combat end logic not completing properly if Lara is on a vehicle (#3885)
- fixed dead water creatures not registering with combat end (#3887, regression from 1.3)

## [1.4.1](https://github.com/LostArtefacts/TRX/compare/tr2-1.4...tr2-1.4.1) - 2025-08-30
- fixed missing shader and configuration files from MacOS releases (#3870, regression from 1.4)
- fixed zero byte at the end of config files (#3875, regression from 1.4)
- fixed stacked sprites flickering (#3872, regression from 1.4)

## [1.4](https://github.com/LostArtefacts/TRX/compare/tr2-1.3.2...tr2-1.4) - 2025-08-23
Showcase: https://www.youtube.com/watch?v=AAOP1VFX9Lw

>[!WARNING]
>Attention level builders: this version introduces backwards incompatible changes to the file structure.
>Please refer to the [migration guide](../3-MIGRATING.md) to see how to update your levels.

- reworked TR2 rendering
    - added round shadows option (Graphic options → Visuals → Round shadows)
    - added option to disable skyboxes (Graphic options → Visuals → Skyboxes)
    - added brightness option (Graphic settings → Rendering → Brightness)
    - added anisotropy option (Graphic settings → Rendering → Anisotropy filter)
    - added vertical sync option (Graphic settings → Rendering → VSync)
    - added an option to keep sprites upright (Graphic options → Rendering → Sprites lock mode)
    - added debug portals feature (`/debug 1`)
    - added debug room clip feature (`/debug 1`)
    - added debug spheres feature (`/debug 1`)
    - added debug triggers feature (`/debug 1`)
    - added support for 60 fps in 3D UI pickups
    - improved bilinear filter appearance - no more dark edges around objects
    - improved bilinear filter texture adjustment - no more texture "expansion" (#2258)
    - changed the F7 hotkey to be used as a wireframe toggle (previously available as Shift+F7)
    - removed software rendering mode
    - removed the z-buffer option, which is now always enabled
    - removed undocumented linear and nearest texel adjustment options
    - fixed trapezoid textures warping at the edge of the screen (#2629)
    - fixed certain polygons disappearing in some objects (#3699)
    - fixed z-fighting of doors near walls
- added new command switches:
    - `--test-record` and `--test-replay` for automated playthroughs with (internal tool – the recording file format may be subject to changes)
    - `--headless`: runs the game offscreen with no audio and at unlocked simulation speed
    - -q`, `--quiet`: outputs only error messages to the terminal, with log files being written to normally
- added ability to move Lara around in photo mode (use sidestep keys to switch modes)
- added additional poses for photo mode
- added an option to allow Lara to sprint (Gameplay → Controls → Sprinting) (#3711)
- added an option to use Lara's slide-to-run animation from TR3+ (Gameplay → Controls → Slide-to-run) (#1089)
- added an option to use Lara's neutral jump-twist from early TR1 betas (Gameplay → Controls → Neutral twists) (#1392)
- added an option to allow Lara to turn around and grab a ledge she has just stepped off (Gameplay → Controls → Controlled drops) (#3621)
- added an option to allow Lara to jump up or back when hanging from a ledge (Gameplay → Controls → Ledge jumps) (#3683)
- added an option to have Lara pose after standing idle for a certain time (Gameplay → Controls → Idle pose timeout) (#3727)
- added an option to animate the algae in 40 Fathoms, Wreck of the Maria Doria and The Deck (Gameplay settings → Fixes → Fix sprite animations) (#3141)
- added an option to scale the 3D pickups in the UI (Graphic options → UI → Pickup scale)
- added an option to control fog color (Graphic options → Visuals → Fog transparency and Fog color) (#712, #3618)
- added German translation
- added a PS1 fade-out to final cutscene (#3521)
- added a new `/vsync` console command to toggle the vsync option, like in TR1
- added a new `/lua` console command (for now, [it cannot do much](../8-LUA.md))
- added a new `/restless` console command, which enables or disables infinite sprint
- improved frames in Lara's jump-twist animations
- improved object loading error messages when an invalid object ID is detected
- improved window resize performance in the title inventory ring
- improved projectiles
    - changed conventional weapons to smash all shatterable objects simultaneously instead of 1 for rifles and 2 for pistols (#3378, #3551)
    - fixed collision detection on windows
    - fixed harpoons/grenades having no effect on bells (#3379)
    - fixed conventional weapons not spawning ricochets on bells (#3379)
- changed the game flow and game strings file placement
- changed the texture page limit from 128 to unlimited (#3517)
- changed the `/set` console command to report boolean values as `0` or `1`, language-agnostic
- changed waterfall objects to always be drawn when active rather than only when Lara is within a 10 sector range (#3598)
- changed `-l`/`--level` switch to accept the level number on top of the level path
- changed settings dialogs to show a suitable message if a level builder has hidden all options within that dialog (#3637)
- changed the text and bar scale option to work in smaller increments (10% reduced to 5%); added support for slow increments by 1% (hold Walk key)
- changed the fly cheat to allow Lara to interact with switches and pickups (#3665)
- fixed audio in the shower cutscene in Home Sweet Home not being sync with the turbo cheat (#3541)
- fixed projectiles sometimes not shattering breakable windows (#3378, #3551)
- fixed flat/opaque window shards in Lara's Home and Home Sweet Home (#3512)
- fixed several OG texture issues (#1834, #2082, #3140, #3187, #3372, #3516, #3629, #3634, #3657, #3659, #3791, #3829, #3860)
- fixed the passport having an invisible back page, noticeable when opening/closing it (#2051)
- fixed z-fighting on the front of the passport (#3584)
- fixed window 23 in Venice potentially appearing broken after loading a savegame, despite being intact before saving (#3559)
- fixed French translations containing Italian text in some cases (#3567)
- fixed several missing, delayed and duplicated door sound effects (#3363, #3614. #3615, #3616, #3663)
- fixed being unable to antitrigger waterfall objects (#3589)
- fixed incorrect frames in Lara's underwater roll animation (#1589)
- fixed mismatched animation frames between the airlock wheel and its corresponding door in offshore levels (#3644)
- fixed incorrect airlock and sliding door object positions in offshore levels (#3644)
- fixed incorrect door positions in Nightmare in Vegas, causing some to be visible through walls (#3836)
- fixed incorrect push button object positions in all levels where it appears (#3596)
- fixed incorrect portals in Catacombs of the Talion room 41 (#3664)
- fixed being unable to hang off bridges in Barkhang Monastery and Temple of Xian (#3691)
- fixed missing zipline reset triggers in Lara's Home (#3698)
- fixed shadows Y component not interpolated in 60 FPS (#1314)
- fixed a crash when the level file was missing
- fixed Lara walking backwards off ledges into lava (#3745)
- fixed backslash/grave key/less-than character on some keyboards shown as ???? – now it's shown as backslash (#3713)
- fixed Lara being able to get on a skidoo while underwater and consequently dying (#3810)
- fixed a missing transition animation between Lara jumping forward and entering freefall (#3815)
- fixed potentially being able to reactivate an already used puzzle slot's trigger (#3849, regression from 1.3)
- fixed persistent damage resetting Lara's HP after cutscenes (#3595, regression from 1.2)
- fixed Lara not being able to look when look mode is set to unrestricted and she is using an airlock door (#3645, regression from 1.3)
- fixed wireframe mode rendering as mostly white (#3649, regression from 1.3.2)
- fixed being unable to cycle poses in photo mode if cheats were disabled (#3726, regression from 1.3)
- fixed Lara exiting the fly cheat if the walk key is used during photo mode (#3753, regression from 1.3)
- fixed triggered pickup items flickering in custom levels (#3623, regression from 0.10)
- fixed Lara not throwing away a spent flare when swimming of flying (#3816, regression from 0.8)
- fixed flame SFX being audible underwater (#3830, regression from 0.3)
- fixed harpoon gun not working correctly in NG+ (#3837, regression from 1.3)
- fixed exiting photo mode on a controller conflicting with the roll input (#3842, regression from 0.9)
- fixed Lara being able to move away from a keyhole/puzzle slot after selecting the key item from the inventory (#3866, regression from 1.3)

## [1.3.2](https://github.com/LostArtefacts/TRX/compare/tr2-1.3.1...tr2-1.3.2) - 2025-07-20
- fixed audio playback with CDAudio backend in cutscenes (#2593)
- fixed sprites having thick borders depending on viewing angle (#3549, regression from 1.3)
- fixed savegame scanner only seeing all-lowercase file names (#3518, regression from 1.0)
- fixed dynamic fire light being generated despite the flame object not being present in the level (#3539, regression from 1.3)
- fixed harpoons disappearing if used near inactive/invisible enemies (#3546, regression from 1.3)
- fixed the first camera frame when starting or loading a level being inaccurate (#3537, regression from 1.2.2)

## [1.3.1](https://github.com/LostArtefacts/TRX/compare/tr2-1.3...tr2-1.3.1) - 2025-07-18
- fixed Lara's first pose in photo mode at times being skipped (#3522, regression from 1.3)
- fixed Lara's arms being drawn inaccurately when posing in photo mode with dual weapons equipped (#3520, regression from 1.3)
- fixed Lara's hair at times becoming detached when posing in photo mode with the M16 equipped (#3525, regression from 1.3)

## [1.3](https://github.com/LostArtefacts/TRX/compare/tr2-1.2.2...tr2-1.3) - 2025-07-14
Showcase: https://youtu.be/C9Nf4j05u_w
- reworked scaler/sizer options
    - added an option to set the upscaling filter (Graphic settings → Rendering → Upscaling filter)
    - changed the "Sizer" option name to "Upscaling factor" (Graphic settings → Rendering → Upscaling factor)
    - changed the maximum upscaling factor from 4 to 8
    - changed the "Scaler" option name to "Borders" (Graphic settings → Rendering → Borders)
    - changed the border option to use nice square borders if the aspect mode is set to Any
    - greatly improved text and other UI rendering with upscaling turned on (#1944)
    - removed default bindings for the "Sizer" and the "Scaler" options (#2853)
    - changed screenshots to always produce images at desktop resolution
- added French translation
- added Gaelic translation
- added Italian translation to the installer
- added dedicated British English translation (#3212)
- added the ability to advance individual frames to the photo mode
- added the ability to skip end game credits (#3266)
- added the ability to hide specific game settings (#3242)
- added the ability to cycle UI tabs with sidestep keys (#3272)
- added the ability to change the health bar color for allies, defaulting to green (#3005)
- added the ability to skip consecutive credit images by holding the action / escape keys
- added the ability to cycle between a list of predefined Lara poses in the photo mode
- added the ability to use the dev console during FMVs
- added a new easter egg command
- added a `/lighting` console command to let the player turn lighting system on/off
- added an `/immune` console command to make Lara impervious to damage
- added an option to have dynamic lights generated by flames (Graphic options → Visuals → Fire lighting) (#3336)
- added missing weapons to Lara's Home, Home Sweet Home and Nightmare in Vegas (for the weapons cheat) (#3360)
- added the ability in custom levels to use the bear, wolf and ice warrior monk from The Golden Mask in the same level as spiders and other monks
- added an option to use TR1 snappy swim turn behaviour (Gameplay settings → Controls → Smooth swimming) (#3387)
- added an option to disable underwater twist (Gameplay settings → Controls → Underwater roll) (#3388)
- added an option to disable jump twist and swan-dive roll (Gameplay settings → Controls → Jump twists) (#3388)
- added an option to control responsive jumping lock behaviour (Gameplay settings → Controls → Jump lock mode) (#3389)
- added an option to display level counter in the statistics dialog (Graphic options → UI → Level counter) (#1087)
- added an option to control playing of certain animation sound effects such as doors when underwater (Sound options → Underwater animation SFX) (#3385)
- added an option to choose between original TR1, original TR2 or unrestricted look modes (Gameplay settings → Controls → Look mode) (#3403)
- added an option to make the quick gun equip keys also holster the active gun (Gameplay settings → UI → Quick gun keys) (#828)
- added an option to allow the audio to mute when the game is out of focus (Sound options → Mute audio when focus lost, #3333)
- added an option to control texture filter for UI alone (Graphic options → Rendering → UI filter)
- added a 16:10 aspect ratio to the Aspect mode option (Graphic options → Rendering → Aspect mode)
- added an inverted look camera option (Gameplay settings → Controls → Inverted look) (#3403)
- added missing end of level statistic screens to Home Sweet Home and Kingdom (#2682)
- added an option to control whether or not Lara reverts to pistols when going from one level to another (Gameplay settings → General → Remember guns between levels) (#3455)
- improved performance when resizing the window
- improved support for >3 secret dragons in custom levels up to 16 dragons
- improved the `/tp` command to orient Lara towards keyholes and doors
- improved handling of animation sound effects when in shallow water (#3385)
- improved error messages for game flow and string edit mistakes to include path of the problematic file
- ⚠️ changed game flow logic for a level that follows one that removed Lara's guns e.g. Diving Area: re-adding pistols now needs to be done in the game flow file, similar to Atlantis in TR1
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
- changed the item duplication glitch fix to be on by default
- changed the Bartoli's Hideout sunset effect to also apply to skybox lighting (#1617)
- changed `/secret give` and `/secret take` to give or take all valid secrets when no index is specified
- removed config tool (we have ingame setting dialogs now)
- removed the limit of 10 dynamic lights per frame (#3384)
- removed the `gym_enabled` game flow property
- fixed inventory screen carpet background texture stretched on non-4:3 aspect ratios (#2022)
- fixed picked up guns not appearing in holsters / on Lara's back (#1588)
- fixed room 134 in Opera House having wrong textures (#3142)
- fixed room 136 in Opera House not having water (#3214)
- fixed Lara not saying 'aha' when picking up the secret in Lara's Home (#3103)
- fixed Lara not drawing weapons with quick draw hotkeys if that was her last equipped weapon (#828)
- fixed Lara not drawing weapons other than pistols and Shotgun with draw key if she didn't have any weapons (#828)
- fixed Lara using flares only once when holding the flare key (#2062, regression from 0.3)
- fixed Lara defaulting to pistols when starting Diving Area, if the player has not collected them in Offshore Rig (#828)
- fixed missing zipline sound in Home Sweet Home (#3102)
- fixed flare count getting corrupt on save/load if Lara had more than 255 flares (#1592)
- fixed title screen background not updating aspect ratio when moving fullscreen window between monitors (#2842)
- fixed title screen background and credit images stretching when using very wide resolutions (#2001)
- fixed certain commands (such as `/load` or `/play`) not working as expected while in the key use inventory screen (#3338)
- fixed Lara able to schedule an interaction with a detonator when it's in use (#3349)
- fixed Lara not saying 'no' near gong or detonator when applicable (#3337)
- fixed Lara saying 'no' near receptacles after loading a game (#1603)
- fixed Lara saying 'no' near receptacles when using guns, medikits or flares (#1601)
- fixed Lara being able to permanently discard a key item if she gets pushed on the exact frame she interact with a receptacle (#3398)
- fixed key items getting consumed at the start of the interaction with receptacles (#3399)
- fixed the Bartoli's Hideout sunset effect being reset after reloading a save (#1617)
- fixed the shotgun sound at the end of the shower cutscene in Home Sweet Home being cut off when the credits start (#1579)
- fixed the camera being partially inside the wall at the end of the Home Sweet Home shower cutscene (#3370)
- fixed the boat veering if Lara looks left or right when driving (#3409)
- fixed Lara not equipping a weapon chosen from inventory if it is the last weapon used (#3457)
- fixed Stopwatch label in Gym not appearing when holding arrows during inventory spin-out (#3460)
- fixed incorrectly shaded sprites (#3476, regression from 1.0)
- fixed being able to deselect the passport in the game over screen (#3381, regression from 1.0)
- fixed Lara getting stuck in the fly cheat in rare circumstances (#3392, regression from 0.3)
- fixed hostile snowmobiles only shooting one gun (#3478, regression from 0.8)
- fixed support for >3 secret dragons in custom levels (#3415, regression from 1.2)
- fixed level select picking one level ahead of the one chosen if the gym is disabled (#3446, regression from 1.0)
- fixed Lara's holsters resetting at times to incorrect meshes when using the fly cheat (#3451, regression from 0.3)
- fixed a possible soft lock when saving the game after killing the last boss in Home Sweet Home (#3470, regression from 1.2)
- fixed the `/play` command starting the level with wrong items sometimes (#3147, regression from 1.1)
- fixed the `/play` command starting Gym in The Golden Mask (this level is not working correctly with TR2G's main.sfx)
- fixed the `/tp` command breaking the photo mode
- fixed the `/tp` command misbehaving when giving fractional coordinates
- fixed the `/play` command not stopping active music when used to play Venice (#3469, regression from 0.8)
- fixed Lara being affected by the `/kill` command if monks have been angered (#3492, regression from 1.0)

## [1.2.2](https://github.com/LostArtefacts/TRX/compare/tr2-1.2.1...tr2-1.2.2) - 2025-06-24
- fixed underwater hum not playing properly (#3305, regression from 0.10)
- fixed game crashing when the expected resources are missing (#3310, regression from 1.2.1)
- fixed restore default pop-up requiring all 3 water color options to be adjusted instead of just one (#3314, regression from 1.2)
- fixed pause screen rendered without background overlay if fade effects are disabled (#3316, regression from 1.1)
- fixed `/pos` command crashing when the level title is not set (regression from 1.2)

## [1.2.1](https://github.com/LostArtefacts/TRX/compare/tr2-1.2...tr2-1.2.1) - 2025-06-22
- fixed some secrets in some levels incorrectly registering by standing on specific tiles (#3280, regression from 1.2)
- fixed movable blocks getting stuck in midair if the game is saved and loaded while they are falling (#3274)
- fixed PS touchpad input missing an icon (#3288, regression from 4.12)
- fixed inability to use unbind key / reset layout buttons with controllers (#3290, regression from 1.2)
- fixed inventory ring consuming too many items under severe frame drop conditions (#3295, regression from 1.0)
- fixed screenshots stripping accented characters (#3238)
- fixed accented lowercase `i` characters retaining the superscript dot (#3298)
- reverted the partial fix for wrong audio device reinitialization (#3251, regression from 1.2)

## [1.2](https://github.com/LostArtefacts/TRX/compare/tr2-1.1...tr2-1.2) - 2025-06-17
Showcase: https://www.youtube.com/watch?v=yG82_Lt6v9M
- added builtin support for ingame string translations
    - changed duplicate game strings between TR1 and TR2 to be placed in a single file TRX_common_strings.json5
    - added a new setting, `enable_review_markers`, which display which text requires review (only available via `/set`)
    - added Italian translation
    - added Polish translation
    - added support for non-breaking spaces
    - fixed game crashing when trying to word-wrap unknown characters
- added UI for all config tool settings
- added ingame help for all settings
- added the ability to use `.avi`, `.mkv`, `.mp4`, `.mpeg`, and `.webm` files for FMVs, as well as the default `.rpl` (#3190)
- added support for showing key/puzzle/pickup item descriptions (examining) in the inventory (#1875)
- added support for object name aliases; added aliases for dev commands
- added a pickup overlay display when Lara pulls the dagger from the dragon (#1830)
- added an option to disable Lara's braid (#3089)
- added an option to disable the breeze effect on Lara's braid (#3090)
- added keyboard and controller input icons to the controls settings dialog
- added an option to continue playing music while in the inventory (#1702)
- added an option to adjust music and ambient volume while in the inventory (#2870)
- added a `/debug` console command
- added a `/secret` console command for easier debugging of secrets
- added `enable_debug_pos` setting that shows Lara's position in realtime (reachable via `/debug`)
- added graphics effects to the savegame so they now persist on load (#2736)
- added an option to control whether or not Lara responds to hitting a wall while wading (#3138)
- added an option to fix the breakable floor descending glitch (#3152)
- added an option to fix wall glitches, or to use TR1 wall glitch behaviour (#3153)
- added an option to disable swing cancelling (#3150)
- added an option to disable lean jumping (#3151)
- added an option to disable smooth wall deflection when Lara comes to a stop at a wall, similar to TR1 (#3148)
- added an option to have Lara boost forward when rolling off one-click steps, similar to TR1 (#3149)
- added an option to toggle allowing Lara to exit from water horizontally, below, or climbing out onto non-standable slopes (#3154)
- added an option to toggle random enemy initial angle adjustment (#3129)
- added an option to prevent Lara targeting allies, either with weapons or the skidoo (#3012)
- added an option to alter Lara's HP for the beginning of each level (#3179)
- added an option to not restore Lara's HP at the beginning of each level (#3179)
- added an option to configure how many shots Lara can take with the harpoon gun before reloading, including disabling reloading altogether (#3057)
- improved word wrapping algorithm in the dev console
- improved the dev console commands documentation
- changed logs format to include timestamps
- changed the music track slot limit from 64 to 1024 (#3101)
- ⚠️ changed the music track behaviour to no longer shift track numbers (#3100)
  - if playing original levels, make sure to update the game flow and injection files from this release
  - if building levels, use track numbers that correspond to the file names; previously built levels will need to be manually adjusted
- changed the maximum number of 2D static mesh slots (room sprites) from 50 to 256 (#3200)
- changed sound and music volumes to be displayed as percentage instead of 0-10
- changed the `/tp` command to align Lara to switches and pickups
- changed the `/set` command to accept `-`, which will restore the given setting to its default state
- changed the graphic settings dialog to use tabs
- changed the setting dialogs to respect the UI wraparound setting
- changed the combat end logic (used in Home Sweet Home) to allow using any regular enemy type aside from the boss
- changed the rotation of some pickups in The Golden Mask to better suit the 3D pickups option (#1973)
- changed text kerning to a smaller value
- fixed a missing collapsible tile trigger in The Cold War room 82 (#3058)
- fixed missing sound effects for collapsible tiles in Opera House, The Deck and Catacombs of the Talion (#2262, #2872, #3087)
- fixed texture and visibility issues with the skyboxes in The Cold War and Kingdom (#3056)
- fixed the same boss item always being selected in Home Sweet Home, regardless of Lara's proximity (#3062)
- fixed transparent eyes on Lara's model in the gym and Home Sweet Home levels (#3072)
- fixed transparent eyes on the wolf model in Furnace of the Gods (#3073)
- fixed Lara getting stuck in her hit animation if she is hit while using an airlock door, the detonator or the gong (#3092)
- fixed Lara behaving erratically if she is killed while hanging from a ledge (#3134)
- fixed Lara's health bar showing in the Home Sweet Home shower cutscene (#1564)
- fixed Lara dropping flares after certain special animations, such as pulling the dagger from the dragon (#3084, regression from 1.1)
- fixed unbind key option being available when it shouldn't (#3111, regression from 1.1)
- fixed the sizer option accepting values above 1 which made no sense (#3123, regression from 1.0)
- fixed a rare crash when editing certain dev console history entries (#2913, regression from 1.0)
- fixed Lara's health bar showing at the start of cutscenes (#3182, regression from 1.1)
- fixed scaler/sizer options not working under some circumstances (#3240, regression from 0.7)
- fixed broken playback of mono music tracks (regression from 0.2)
- fixed hot-plugging certain audio devices causing glitchy playback (partial fix; regression from 0.2)
- fixed stats dialog reserving too much space for extra secrets (#3237, regression from 1.0)
- fixed logging not outputting anything on Windows terminals
- fixed `/kill all` command softlocking the game in Home Sweet Home

## [1.1](https://github.com/LostArtefacts/TRX/compare/tr2-1.0.2...tr2-1.1) - 2025-05-23
Showcase: https://www.youtube.com/watch?v=g5lrrDXDYKo
- added a /help command (#2917)
- added a flashing Demo Mode caption to demos (#1556)
- added arrows to the passport text like in TR1X (#2926)
- added aliases to CLI options (`-gold` becomes `-g/--gold`)
- added a `--help` CLI option (may not output anything on Windows machines – OS bug)
- added explosion sprites to Home Sweet Home (#1569)
- added ability to reposition the health bar and the air bar (#1611)
- added enemy health bars (#2909)
- added an FPS counter (#2910)
- added the ability to move the camera around with W,A,S,D (rebindable) (#2978)
- added an option to toggle between TR1 and TR2 camera modes (#2990)
- added the ability to reset active inputs layout
- added the ability to unbind non-essential keys
- added the ability to rebind more keys
- added the ability to trigger different ambient tracks in custom levels, which will loop and be remembered between saves
- improved word wrapping algorithm in the dev console
- improved the `/set` console command to display available options if given an unknown argument
- improved handling of items that are dropped by enemies (#2952)
    - added the ability for any enemy type to drop items, excluding eels
    - fixed items dropped by flying creatures not falling to the ground
- changed the design of the controls dialog to use pages, making it better suited for small screens, larger text sizes, and more key bindings
- changed on-screen messages (such as `Z-Buffer on` to use the dev console, like in TR1X)
- changed the sound dialog appearance (repositioned, added text labels and arrows)
- changed the installer to always allow downloading music files (#2891)
- changed the dev console to no longer add duplicate entries to the history
- changed the health bar and the air bar sizes to be slightly bigger
- changed the pause screen to have a darker black overlay transparency (#2252)
- removed the hard-coded inventory allocation on the first level by default, moving it instead to the game flow (#1867)
- removed the hard-coded repositioning of Bartoli (pre-dragon) on initialise (#2950)
- fixed Lara's braid pointing straight down when swimming below sloped ceilings (#1600)
- fixed glide cameras using a default speed rather than maintaining the values set in the level file (#2962)
- fixed Lara being killed if she enters the void in a level that uses the `disable_floor` sequence in the game flow (#2874, regression from 0.10)
- fixed Lara unable to equip pistols after getting a rifle-type weapon wet while wading (#2994)
- fixed flame emitter 23 in room 6 not being deactivated when the lever in room 1 is used (#2851)
- fixed Lara snapping to face forwards if she has a slight angle and action is pressed after using an airlock door (#2215)
- fixed Lara being able to equip guns and flares during in-game cutscenes (#2895)
- fixed an illegal reachable slope in Barkhang Monastery room 96, which could lead to Lara becoming softlocked (#2900)
- fixed the camera behaving erratically in rooms/sectors that have no pathfinding data (#2946)
- fixed wall light mesh positions in Venice, Bartoli's Hideout and Barkhang Monastery (#2944)
- fixed faulty zoning data in Ice Palace rooms 48/110 that could result in the yetis becoming stuck (#3000)
- fixed a misplaced springboard trigger in Ice Palace room 104 (#3003)
- fixed the game crashing on unknown sequencer events
- fixed the game crashing when editing long dev console history entries (#2913, regression from 1.0)
- fixed harpoon's ammo counter overlapping with the air bar (#2871)
- fixed flames showing briefly when Lara enters water and a death tile is present
- fixed being unable to load a save made in the first level if that level removes Lara's weapons but also has a shotgun pickup (#2934, regression from 0.9)
- fixed misplaced effects such as bubbles and dragon fire in 60 FPS (#2873, #2881, regression from 0.10)
- fixed incorrect camera shifts when some fixed cameras return to normal view (#2971, regression from 0.10)
- fixed blood not spawning when Lara is run down by boulders/barrels (#2982, regression from 0.7)
- fixed floors being lowered too much under pushable blocks that are killed in the same trigger that flips the map (#3007, regression from 0.9)
- fixed inventory ring items not being animated when the ring is rotating (#2964, regression from 0.9)
- fixed the camera jumping if going from a look at trigger to a fixed camera, such as in The Cold War room 36 (#3033, regression from 0.9)
- fixed a crash in The Golden Mask if the bear is killed with the grenade launcher (#3037, regression from 1.0)
- fixed passport faces partially invisible

## [1.0.2](https://github.com/LostArtefacts/TRX/compare/tr2-1.0.1...tr2-1.0.2) - 2025-04-26
- changed The Golden Mask strings to default to the OG strings file for the main tables (#2847)
- fixed Lara voiding if she stops on a tile with a closing door, and the door isn't on a portal (#2848)
- fixed guns carried by enemies not being converted to ammo if Lara has picked up the same gun elsewhere in the same level (#2856)
- fixed button mashing causing quick save/load to misbehave on a specific passport animation frame  (#2863, regression from 1.0)
- fixed guns carried by enemies not being converted to ammo if Lara starts the level with the gun and the game has later been reloaded (#2850, regression from 1.0)
- fixed 1920x1080 screenshots in 16:9 aspect mode being saved as 1919x1080 (#2845, regression from 0.8)
- fixed clicks in audio sounds (#2846, regression from 0.2)

## [1.0.1](https://github.com/LostArtefacts/TRX/compare/tr2-1.0...tr2-1.0.1) - 2025-04-24
- added an option to wraparound when scrolling UI dialogs, such as save/load (#2834)
- improved graphic settings dialog sizing (#2841)
- changed save to take priority over load when both inputs are held on the same frame, in line with OG (#2833)
- fixed the selected keyboard/controller layout not being saved (#2830, regression from 1.0)
- fixed toggling the PSX FOV option not having an immediate effect (#2831, regression from 1.0)
- fixed changing the aspect ratio not updating the current background image (#2832, regression from 1.0)

## [1.0](https://github.com/LostArtefacts/TRX/compare/tr2-0.10...tr2-1.0) - 2025-04-23
Showcase: https://www.youtube.com/watch?v=iUNUJda6QCU
- added support for The Golden Mask (#1621)
- added ability to turn off legal screen and FMVs (#2740)
- added ability to turn off ingame cutscenes (#2127)
- added HD images from TR2Main (with Arsunt's consent)
- added sunglasses for graphic options (#1615)
- added control over the fog distances for players and level builders (#1622)
- added control over the water color for players and level builders [see the reference](/docs/WATER_COLORS.md) (#1619)
- added an installer for Windows (#2681)
- added the bonus level game flow type, which allows for levels to be unlocked if all main game secrets are found (#2668)
- added the ability for custom levels to have up to two of each secret type per level (#2674)
- added BSON savegame support, removing the limits imposed by the OG 8KB file size, so allowing for storing more data and offering improved feature support (legacy save files can still be read, similar to TR1) (#2662)
- added NG+, Japanese, and Japanese NG+ game mode options to the New Game page in the passport (#2731)
- added the ability for spike walls to be reset (antitriggered)
- added the current music track and timestamp to the savegame so they now persist on load (#2579)
- added waterfalls to the savegame so that they now persist on load (#2686)
- added support for aspect ratio-specific images (#1840)
- added a guard to ensure the game always starts on a visible screen even after unplugging displays (#2819)
- improved performance when moving the window around
- improved pause exit dialog - it can now be canceled with escape
- changed savegame files to be stored in the `saves` directory (#2087)
- changed the default fog distance to 22 tiles cutting off at 30 tiles to match TR1X (#1622)
- changed the number of static mesh slots from 50 to 256 (#2734)
- changed the maximum number of items (moveables) per level from 256 to 10240 (1024 remains the limit for triggered items) (#1794)
- changed the maximum number of visible enemies from 5 to 32 (#1624)
- changed the maximum number of effects (flames, embers, exploding parts etc) from 100 to 1000 (#1581)
- changed default pitch of the save/load dialog ingame - it's now higher.
- removed the need to specify in the game flow levels that have no secrets (secrets will be automatically counted) (#1582)
- removed the hard-coded end-level behaviour of the bird guardian for custom levels (#1583)
- removed the FPS and aspect mode options from the config tool (now available in-game in the graphics options)
- fixed the inability to completely mute the sounds, even at sound volume 0 (#2722)
- fixed the final two levels not allowing for secrets to be counted in the statistics (#1582)
- fixed assault course best times not being retained between game relaunches (#1578)
- fixed flares disappearing on the ground when the z buffer is enabled (#1595)
- fixed Lara's holsters being empty if a game flow level removes all weapons but also re-adds the pistols (#2677)
- fixed the console opening when remapping its key (#2641)
- fixed the boat when it explodes after crossing mines, where Lara's hips would appear rather than exploded boat parts (#1605)
- fixed Lara's hips appearing on Bartoli in the Temple of Xian cutscene (#2558)
- fixed collision issues with drawbridges, trapdoors, and bridges when stacked over each other, over slopes, and near the ground (#2752)
- fixed the lift to work in any cardinal direction in custom levels, not just South (#2100)
- fixed the springboard not responding correctly when Lara drives across one on a skidoo (#1903)
- fixed the drawbridge producing dynamic light when open (#2294)
- fixed the scale of several pickup models in The Golden Mask (#2652)
- fixed the shark in The Cold War not making any sounds when biting Lara (#2678)
- fixed the bird monster not having a shadow (#2060)
- fixed the in-game cinematic camera at times yielding invalid positions (and hence views) in custom levels (#2754)
- fixed a softlock in Temple of Xian if the main chamber key is missed (#2042)
- fixed a potential softlock in Floating Islands if returning towards the level start from the gold secret (#2590)
- fixed a potential softlock in Nightmare in Vegas where the bird monster could remain inactive, or the flip map not set (#1851)
- fixed invalid portals in The Deck between rooms 17 and 104, which could result in Lara seeing enemies in disconnected rooms (#2393)
- fixed pushblocks being rotated when Lara grabs them, most noticeable if asymmetric textures have been used (#2776)
- fixed the boat briefly having an underwater hue when Lara first climbs on (#2787)
- fixed destroyed gondolas appearing embedded in the ground after loading a save (#1612)
- fixed a crash in custom levels with large rooms (#2749)
- fixed the viewport not always in sync with the window (#2820)
- fixed inability to move the window to another screen (#2820)
- fixed flares flipped to the right when thrown (regression from 0.10)
- fixed the camera going out of bounds in 60fps near specific invalid floor data (known as no-space) (#2764, regression from 0.10)
- fixed sprites rendering black if no shade value is assigned in the level (#2701, regression from 0.8)
- fixed some 3D pickup items rendering black in software mode (#2792, regression from 0.10)
- fixed Lara at times ending up in incorrect rooms when using the teleport cheat (#2486, regression from 0.3)
- fixed the `/pos` console command reporting the base room number when Lara is actually in a flipped room (#2487, regression from 0.3)
- fixed a crash if an image was missing
- fixed a crash on level load if an animation has no frames (#2746, regression from 0.8)
- fixed flares missing the flicker effect in 60 FPS (#2806, regression from 0.10)

## [0.10](https://github.com/LostArtefacts/TRX/compare/tr2-0.9.2...tr2-0.10) - 2025-03-18
Showcase: https://www.youtube.com/watch?v=s41hznpTJkY
- added support for 60 FPS rendering
- added support for more accented characters (#2356)
- added quadrilateral interpolation (#354)
- added a `/cheats` console command
- added a `/wireframe` console command (#2500)
- added a `/fps` console command
- added `/flood` and `/drain` console commands
- added support for `-l`/`--level` argument to play a single level
- added support for `-s`/`--save` argument to immediately start a saved game
- added the ability to specify per-level SFX files rather than enforcing the default (main.sfx) on all levels (#2615)
- added the camera shutter sound to cutscenes for photo mode (#2280)
- added Italian localization to the config tool
- improved camera mode navigation:
    - improved support for pivoting
    - improved roll support
    - expanded world bounding box by 5 tiles in each direction
    - added support for 60 FPS
- changed injections to a new file format with a smaller footprint, improved applicability tests and similar feature support as TR1 (#1967)
- changed the `/pos` command to show `Demo` and `Cutscene` instead of `Level` when relevant
- changed the `/pos` command to show demo and cutscene numbers starting at 1, in line with `/play`
- changed the `/play` and `/pos` commands to always treat the gym level as the level 0 – even if it's not included
- removed the hardcoded title screen image path, replacing it with a game flow file property instead
- fixed smashed windows blocking enemy pathing after loading a save (#2535)
- fixed several instances of the camera going out of bounds (#1034)
- fixed Lara getting stuck in a T-pose after jumping/falling and then dying before reaching fast fall speed (#2575)
- fixed missing enemy sound effects in the underwater levels (#2293)
- fixed seaweed collision in Living Quarters preventing Lara from climbing out of the water in room 15 (#2197)
- fixed the scale and rotation of several pickup models, such as the offshore key cards and Barkhang prayer wheels (#1832, #1894)
- fixed a rare issue whereby Lara would be unable to move after disposing a flare (#2545, regression from 0.9)
- fixed flare pickups only adding one flare to Lara's inventory rather than six (#2551, regression from 0.9)
- fixed several issues with pushblocks (#2036/#2193)
    - fixed an invisible wall above stacked pushblocks if near a ceiling portal
    - fixed floor height issues with pushblocks poised to fall in various scenarios
    - fixed being unable to stack multiple pushblocks over multiple rooms
    - fixed falling pushblocks using the enemy grunt sound effect
- fixed play any level causing the game to hang when no gym level is present (#2560, regression from 0.9)
- fixed extremely large item quantities crashing the game (#2497, regression from 0.3)
- fixed missing new game text in the passport when play any level is enabled (#2563, regression from 0.9)
- fixed the play any level dialog not showing in the gym passport (#2564, regression from 0.9)
- fixed losing the NG+ flag when loading a save that has it set (#2566, regression from 0.9.2)
- fixed the ammo counter not showing in demos if NG+ is set (#2574, regression from 0.9)
- fixed being able to play with Lara invisible after using the explosion cheat then the fly cheat (#2584, regression from 0.9)
- fixed the `/pos` command not showing demo and cutscene titles
- fixed the distance travelled stat displaying the wrong value when over 1000m (#2659)

## [0.9.2](https://github.com/LostArtefacts/TRX/compare/tr2-0.9.1...tr2-0.9.2) - 2025-02-19
- fixed secret rewards not handed out after loading a save (#2528, regression from 0.8)
- fixed music not working on certain Linux setups (#2504, regression from 0.2)

## [0.9.1](https://github.com/LostArtefacts/TRX/compare/tr2-0.9...tr2-0.9.1) - 2025-02-15
- improved memory usage by shedding ca. 100-110 MB on average
- changed passport to be more responsive to player inputs (#1328)
- fixed resolving paths (especially to music files) on case-sensitive filesystems (#1934, #2504)
- fixed loading a game crashing on Linux (#2508, regression from 0.9)

## [0.9](https://github.com/LostArtefacts/TRX/compare/tr2-0.8...tr2-0.9) - 2025-02-14
Showcase: https://www.youtube.com/watch?v=FrBSW35ZPKY
- added Linux builds and toolchain (#1598)
- added macOS builds (for both Apple Silicon and Intel) (#2226)
- added pause dialog (#1638)
- added a photo mode feature (#2277)
- added fade-out effect to the demos
- added the ability to hold left/right to move through menus more quickly (#2298)
- added the ability to disable exit fade effects alone (#2348)
- added a fade-out effect when completing Lara's Home
- added support for animated sprites (#2401)
- added a `/cut` (alias: `/cutscene`) console command for playing cutscenes
- added a `/gym` (alias: `/home`) console command for playing Lara's Home
- added a `/music` console command that plays a specific music track
- added a console log when using the `/demo` command
- improved rendering to achieve a slight performance boost in big rooms (#2325)
- improved wireframe mode appearance around screen edges
- changed the object texture limit from 2048 to unlimited (within game's overall memory cap) (#1795)
- changed the sprite texture limit from 512 to unlimited (within game's overall memory cap) (#1795)
- changed the texture page limit from 32 to 128 (#1796)
- changed default input bindings to let the photo mode binding be compatible with TR1X:
    | Key                           | Old binding | New binding  |
    | ----------------------------- | ----------- | ------------ |
    | Decrease resolution           | Shift+F1    | Shift+F11    |
    | Increase resolution           | F1          | F11          |
    | Decrease internal screen size | Shift+F2    | Shift+F10    |
    | Increase internal screen size | F2          | F10          |
    | Toggle photo mode             | ---         | F1           |
    | Toggle photo mode UI          | ---         | H            |
- changed the `/kill` command with no arguments to look for enemies within 5 tiles (#2297)
- changed the game data to use a separate strings file for text information, removing it from the game flow file
- changed dynamic lighting for gun flashes and explosions to be optional (#2357)
- fixed scale of secret icons on level complete summary (#1631)
- fixed showing inventory ring up/down arrows when uncalled for (#2225)
- fixed Lara never stepping backwards off a step using her right foot (#1602)
- fixed flawed frame number checks which prevented Lara's wall hit animation while wading
- fixed blood spawning on Lara from gunshots using incorrect positioning data (#2253)
- fixed ghost meshes appearing near statics in custom levels (#2310)
- fixed potential memory corruption when reading a custom level with more than 512 sprite textures (#2338)
- fixed the teleporting command sometimes putting Lara in invalid flipmap rooms (#2370)
- fixed teleporting to an item on a ledge sometimes pushing Lara to the room below (#2372)
- fixed the game crashing if a cinematic is triggered but the level contains no cinematic frames (#2413)
- fixed being unable to load a level that contains no sound effect data (#2460)
- fixed issues with sound effects not playing or looping forever in some cases when many other effects are playing (#2494)
- fixed Lara activating triggers one frame too early (#2205, regression from 0.7)
- fixed savegame incompatibility with OG (#2271, regression from 0.8)
- fixed stopwatch showing wrong UI in some circumstances (#2221, regression from 0.8)
- fixed excessive braid movement when dead in windy rooms (#2265, regression from 0.8)
- fixed item counter shown even for a single medipack (#2222, regression from 0.3)
- fixed item counter always hidden in NG+, even for keys (#2223, regression from 0.3)
- fixed the passport object not being selected when exiting to title (#2192, regression from 0.8)
- fixed the upside-down camera fix to no longer limit Lara's vision (#2276, regression from 0.8)
- fixed /kill command freezing the game under rare circumstances (#2297, regression from 0.3)
- fixed wireframe mode discarding transparent pixels (#2315, regression from 0.7)
- fixed sprite pickups not being paused in the pause/inventory screen (#2319, regression from 0.6)
- fixed Skidoo snow wake effects at slow speeds (#2324, regression from 0.6)
- fixed software renderer skybox occlusion issues (#2343, regression from 0.7)
- fixed gunflare from bandits in Tibetan levels spawning too far from their guns (#2365, regression from 0.8)
- fixed guns sometimes appearing in Lara's hands when entering the fly cheat while undrawing weapons (#2376, regression from 0.3)
- fixed the `/play` console command not resetting Lara's inventory (#2267, regression from 0.3)
- fixed flashing text when trying to exit passport while Lara is dead and an action is required (#2263)

## [0.8](https://github.com/LostArtefacts/TRX/compare/tr2-0.8...tr2-0.8) - 2025-01-01
- completed decompilation efforts – TR2X.dll is gone, Tomb2.exe no longer needed (#1694)
- added the ability to set user-defined FOV (no UI for it yet) (#2177)
- added the ability to turn FMVs off (#2110)
- added an option to use PS1 contrast levels, available under F8 (#1646)
- added an option to use TR3+ side steps (#2111)
- added an option to allow disabling the developer console (#2063)
- added an optional fix for the QWOP glitch (#2122)
- added an optional fix for the step glitch, where Lara can be pushed into walls (#2124)
- added an optional fix for drawing a free flare during the underwater pickup animation (#2123)
- added an optional fix for Lara drifting into walls when collecting underwater items (#2096)
- added an option to control how music is played while underwater (#1937)
- added an optional demo number argument to the `/demo` command
- added an option to set the bar scaling (no UI for it yet) (#1636)
- added an option to set the text scaling (no UI for it yet) (#1636)
- improved the animation of Lara's braid (#2094)
- changed demo to be interrupted only by esc or action keys
- changed the turbo cheat to also affect ingame timer (#2167)
- fixed health bar and air bar scaling (#2149)
- fixed text being stretched on non-4:3 aspect ratios (#2012)
- fixed Lara prioritising throwing a spent flare while mid-air, so to avoid missing ledge grabs (#1989)
- fixed Lara at times not being able to jump immediately after going from her walking to running animation (#1587)
- fixed bubbles spawning from flares if Lara is in shallow water (#1590)
- fixed flare sound effects not always playing when Lara is in shallow water (#1590)
- fixed looking forward too far causing an upside down camera frame (#1594)
- fixed music not playing if triggered while the game is muted, but the volume is then increased (#2170)
- fixed game FOV being interpreted as horizontal (#2002)
- fixed the inventory up arrow at times overlapping the health bar (#2180)
- fixed software renderer not applying underwater tint (#2066, regression from 0.7)
- fixed some enemies not looking at Lara (#2080, regression from 0.6)
- fixed the camera getting stuck at the start of Home Sweet Home (#2129, regression from 0.7)
- fixed assault course timer not paused in the inventory (#2153, regression from 0.6)
- fixed Lara spawning air bubbles above water surfaces during the fly cheat (#2115, regression from 0.3)
- fixed demos playing too eagerly (#2068, regression from 0.3)
- fixed Lara sometimes being unable to use switches (#2184, regression from 0.6)
- fixed Lara interacting with airlock switches in unexpected ways (#2186, regression from 0.6)
- fixed input controller remaps not being saved across game relaunches (#2422, regression from 0.6)

## [0.7.1](https://github.com/LostArtefacts/TRX/compare/tr2-0.7...tr2-0.7.1) - 2024-12-17
- fixed a crash when selecting the sound option (#2057, regression from 0.6)

## [0.7](https://github.com/LostArtefacts/TRX/compare/tr2-0.6...tr2-0.7) - 2024-12-16
- switched to OpenGL rendering (#1844)
    - improved support for non-4:3 aspect ratios (#1647)
    - changed fullscreen behavior to use windowed desktop mode (#1643)
    - added an option for 1-2-3-4× pixel upscaling (available under the F1/Shift-F1 key)
    - added the ability to use the window border option at all times (available under the F2/Shift-F2 key)
    - added the ability to toggle between the software/hardware renderer at runtime (available under the F12 key)
    - added fade effects to the hardware renderer (#1623)
    - added an informative text when toggling various rendering options at runtime (#1873)
    - added a wireframe mode (available with `/set` console command and with Shift+F7)
    - changed the software renderer to use the picture's palette for the background pictures
    - changed the hardware renderer to always use 16-bit textures (#1558)
    - fixed texture corruption after FMVs play (#1562)
    - fixed black borders in windowed mode (#1645)
    - fixed "Failed to create device" when toggling fullscreen (#1842)
    - fixed distant rooms sometimes not appearing, causing the skybox to be visible when it shouldn't (#2000)
    - fixed rendering problems on certain Intel GPUs (#1574)
- replaced the Windows Registry configuration with .json files
    - removed setup dialog support (using `Tomb2.exe -setup` will have no effect on TR2X)
    - removed unused detail level option
    - removed triple buffering option
    - removed dither option
- added support for custom levels to enforce values for any config setting (#1846)
- added an option to fix inventory item usage duplication (#1586)
- added optional automatic key/puzzle inventory item pre-selection (#1884)
- added a search feature to the config tool (#1889)
- added an option to fix rotation on some pickup items to better suit 3D pickup mode (#1613)
- added background for the final game stats (#1584)
- added the ability to turn fade effects on/off (#1623)
- removed unused detail level option
- fixed a crash when trying to draw too many rooms at once (#1998)
- fixed Lara getting stuck in her hit animation if she is hit while mounting the boat or skidoo (#1606)
- fixed pistols appearing in Lara's hands when entering the fly cheat during certain animations (#1874)
- fixed wrongly calculated trapdoor size that could affect custom levels (#1904)
- fixed one of the collapsible tiles in Opera House room 184 not triggering (#1902)
- fixed being unable to use the drawbridge key in Tibetan Foothills after the flipmap (#1744)
- fixed missing triggers and ladder in Catacombs of the Talion after the flipmap (#1960)
- fixed incorrect music trigger types at the beginning of Catacombs of the Talion (#1962)
- fixed missing death tiles in Temple of Xian room 91 (#1920)
- fixed the detonator key and gong hammer not activating their target items when manually selected from the inventory (#1887)
- fixed wrongly positioned doors in Ice Palace and Floating Islands, which caused invisible walls (#1963)
- fixed picking up the Gong Hammer in Ice Palace sometimes not opening the nearby door (#1716)
- fixed room 98 in Wreck of the Maria Doria not having water (#1939)
- fixed a potential crash if Lara is on the skidoo in a room with many other adjoining rooms (#1987)
- fixed a softlock in Home Sweet Home if the final cutscene is triggered while Lara is on water surface (#1701)
- fixed Lara's left arm becoming stuck if a flare is drawn just before the final cutscene in Home Sweet Home (#1992)
- fixed resizing game window on the stats dialog cloning the UI elements, eventually crashing the game (#1999)
- fixed exiting the game with Alt+F4 not immediately working in cutscenes
- fixed game freezing when starting demo/credits/inventory offscreen
- fixed problems when trying to launch the game with High DPI mode enabled (#1845)
- fixed clock drift accumulating with time, causing audio desync in cutscenes (#1935, regression from 0.6)
- fixed controllers dialog missing background in the software renderer mode (#1978, regression from 0.6)
- fixed a crash relating to audio decoding (#1895, regression from 0.2)
- fixed depth problems when drawing certain rooms (#1853, regression from 0.6)
- fixed being unable to go from surface swimming to underwater swimming without first stopping (#1863, regression from 0.6)
- fixed Lara continuing to walk after being killed if in that animation (#1880, regression from 0.1)
- fixed some music tracks looping while Lara remained on the same trigger tile (#1899, regression from 0.2)
- fixed some music tracks not playing if they were the last played track and the level had no ambience (#1899, regression from 0.2)
- fixed broken final stats screen in software rendering mode (#1915, regression from 0.6)
- fixed screenshots not capturing level stats (#1925, regression from 0.6)
- fixed screenshots sometimes crashing in the windowed mode (regression from 0.6)
- fixed creatures being able to swim/fly above the ceiling up to one tile (#1936, regression from 0.1)
- fixed the `/kill all` command reporting an incorrect count in some levels (#1995, regression from 0.3)

## [0.6](https://github.com/LostArtefacts/TRX/compare/tr2-0.5...tr2-0.6) - 2024-11-06
- added a fly cheat key (#1642)
- added an items cheat key (#1641)
- added a level skip cheat key (#1640)
- added a turbo cheat (#1639)
- added the ability to skip end credits with the action and escape keys (#1800)
- added the ability to skip FMVs with the action key (#1650)
- added the ability to hold forward/back to move through menus more quickly (#1644)
- added optional rendering of pickups in the UI as 3D meshes (#1633)
- added optional rendering of pickups on the ground as 3D meshes (#1634)
- added a special target, "pickup", to item-based console commands
- changed the inputs backend from DirectX to SDL (#1695)
    - improved controller support to match TR1X
    - changed the number of custom layouts to 3
    - changed default key bindings according to the following table:
        | Key                           | Old binding | New binding  | Reason
        | ----------------------------- | ----------- | ------------ | -----
        | Flare                         | Comma (,)   | Period (.)   | To maintain forward compatibility with TR3
        | Screenshot                    | S           | Print Screen | To maintain compatibility with TR1X
        | Toggle bilinear filter        | F8          | F3           | To maintain compatibility with TR1X
        | Toggle perspective filter     | Shift+F8    | F4           | To maintain compatibility with TR1X
        | Toggle z-buffer               | F7          | F7           | Likely to be permanently enabled in the future
        | Toggle triple buffering       | Shift+F7    | **Removed**  | Obscure setting, will be either removed or available via the ingame UI at some point
        | Toggle dither                 | F11         | **Removed**  | Obscure setting, will be either removed or available via the ingame UI at some point
        | Toggle fullscreen             | F12         | Alt-Enter    | To maintain compatibility with TR1X
        | Toggle rendering mode         | Shift+F12   | F12          | No more conflict to require Shift
        | Decrease resolution           | F1          | Shift+F1     | F3 and F4 are already taken
        | Increase resolution           | F2          | F1           | F3 and F4 are already taken
        | Decrease internal screen size | F3          | Shift+F2     | F3 and F4 are already taken
        | Increase internal screen size | F4          | F2           | F3 and F4 are already taken
    - removed "falling through" to the default layout, with the exception of keyboard arrows (matching TR1X behavior)
    - removed hardcoded Shift+F7 key binding for toggling triple buffering
    - removed hardcoded `0` key binding for flares
    - removed hardcoded cooldown of 15 frames for medipacks
- changed text backend to accept named sequences (eg. "\{arrow up}" and similar)
- changed inventory to pause the music rather than muting it (#1707)
- changed the `/pos` command to include the level number and title
- changed the `/tp` command to teleport to items in a round-robin fashion
  The first call will teleport Lara to the object that's the closest to her; repeated calls will cycle through all matching objects in the object placement order.
- improved FMV mode appearance - removed black scanlines (#1729)
- improved FMV mode behavior - stopped switching screen resolutions (#1729)
- improved screenshots: now saved in the screenshots/ directory with level titles and timestamps as JPG or PNG, similar to TR1X (#1773)
- improved switch object names
    - Switch Type 1 renamed to "Airlock Switch"
    - Switch Type 2 renamed to "Small Switch"
    - Switch Type 3 renamed to "Switch Button"
    - Switch Type 4 renamed to "Lever/Switch"
    - Switch Type 5 renamed to "Underwater Lever/Switch"
- fixed screenshots not working in windowed mode (#1766)
- fixed screenshots key not getting debounced (#1773)
- fixed `/give` not working with weapons (regression from 0.5)
- fixed the camera being cut off after using the gong hammer in Ice Palace (#1580)
- fixed the audio not being in sync when Lara strikes the gong in Ice Palace (#1725)
- fixed door cheat not working with drawbridges (#1748)
- fixed certain audio samples continuing to play after finishing the level (#1770, regression from 0.2)
- fixed Lara's underwater hue being retained when re-entering a boat (#1596)
- fixed Lara reloading the harpoon gun after every shot in NG+ (#1575)
- fixed the dragon reviving itself after Lara removes the dagger in rare circumstances (#1572)
- fixed grenades counting as double kills in the game statistics (#1560)
- fixed the ammo counter being hidden while a demo plays in NG+ (#1559)
- fixed the game crashing in large rooms with z-buffer disabled (#1761, regression from 0.2)
- fixed the game hanging if exited during the level stats, credits, or final stats (#1585)
- fixed the console not being drawn during credits (#1802)
- fixed grenades launched at too slow speeds (#1760, regression from 0.3)
- fixed the dragon counting as more than one kill if allowed to revive (#1771)
- fixed a crash when firing grenades at Xian guards in statue form (#1561)
- fixed harpoon bolts damaging inactive enemies (#1804)
- fixed enemies that are run over by the skidoo not being counted in the statistics (#1772)
- fixed sound settings resuming the music (#1707)
- fixed being able to use hotkeys in the end-level statistics screen
- fixed the inventory ring spinout animation sometimes running too fast (#1704, regression from 0.3)
- fixed new saves not displaying the save count in the passport (#1591)
- fixed certain erroneous `/play` invocations resulting in duplicated error messages

## [0.5](https://github.com/LostArtefacts/TRX/compare/afaf12a...tr2-0.5) - 2024-10-08
- added `/sfx` command
- added `/nextlevel` alias to `/endlevel` console command
- added `/quit` alias to `/exit` console command
- added the ability to cycle through console prompt history (#1571)
- improved vertex movement when looking through water portals (#1493)
- improved console commands targeting creatures and pickups (#1667)
- changed `/set` console command to do fuzzy matching (LostArtefacts/libtrx#38)
- fixed crash in the `/set` console command (regression from 0.3)
- fixed using console in cutscenes immediately exiting the game (regression from 0.3)
- fixed Lara remaining tilted when teleporting off a vehicle while on a slope (LostArtefacts/TR2X#275, regression from 0.3)
- fixed `/endlevel` displaying a success message in the title screen
- fixed very loud music volume set by default (#1614)

## [0.4]
Version 0.4 was skipped because of a major repository merge with TR1X into TRX.

## [0.3](https://github.com/LostArtefacts/TR2X/compare/0.2...0.3) - 2024-09-20
- added new console commands:
    - `/endlevel`
    - `/demo`
    - `/title`
    - `/play [level]`
    - `/load [slot]`
    - `/save [slot]`
    - `/exit`
    - `/fly`
    - `/give`
    - `/kill`
    - `/flip`
    - `/set`
- added the ability to remap the console key (LostArtefacts/TR2X#163)
- added `/tp` console command's ability to teleport to specific items
- added `/fly` console command's ability to open nearest doors
- added an option to fix M16 accuracy while running (LostArtefacts/TR2X#45)
- added a .NET-based configuration tool (LostArtefacts/TR2X#197)
- improved initial level load time by lazy-loading audio samples (LostArtefacts/TR2X#114)
- improved crash debug information (LostArtefacts/TR2X#137)
- improved the console caret sprite (LostArtefacts/TR2X#91)
- changed the default flare key from `/` to `.` to avoid conflicts with the console (LostArtefacts/TR2X#163)
- fixed numeric keys interfering with the demos (LostArtefacts/TR2X#172)
- fixed explosions sometimes being drawn too dark (LostArtefacts/TR2X#187)
- fixed killing the T-Rex with a grenade launcher crashing the game (LostArtefacts/TR2X#168)
- fixed secret rewards not displaying shotgun ammo (LostArtefacts/TR2X#159)
- fixed controls dialog remapping being too sensitive (LostArtefacts/TR2X#5)
- fixed `/tp` console command during special animations in HSH and Offshore Rig (LostArtefacts/TR2X#178, regression from 0.2)
- fixed `/hp` console command taking arbitrary integers
- fixed console commands being able to interfere with demos, cutscenes and the title screen (LostArtefacts/TR2X#182, #179, regression from 0.2)
- fixed console registering key inputs too eagerly (regression from 0.2)
- fixed console not being drawn in cutscenes (LostArtefacts/TR2X#180, regression from 0.2)
- fixed sounds not playing under certain circumstances (LostArtefacts/TR2X#113, regression from 0.2)
- fixed the excessive pitch and playback speed correction for music files with sampling rate other than 44100 Hz (LostArtefacts/TR1X#1417, regression from 0.2)
- fixed a crash potential with certain music files (regression from 0.2)
- fixed enemy movement patterns in demo 1 and demo 3 (LostArtefacts/TR2X#98, regression from 0.1)
- fixed underwater creatures dying (LostArtefacts/TR2X#98, regression from 0.1)
- fixed a crash when spawning enemy drops (LostArtefacts/TR2X#125, regression from 0.1)
- fixed how sprites are shaded (LostArtefacts/TR2X#134, regression from 0.1.1)
- fixed enemies unable to climb (LostArtefacts/TR2X#138, regression from 0.1)
- fixed items not being reset between level loads (LostArtefacts/TR2X#142, regression from 0.1)
- fixed pulling the dagger from the dragon not activating triggers (LostArtefacts/TR2X#148, regression from 0.1)
- fixed the music at the beginning of Offshore Rig not playing (LostArtefacts/TR2X#150, regression from 0.1)
- fixed wade animation when moving from deep to shallow water (LostArtefacts/TR2X#231, regression from 0.1)
- fixed the distorted skybox in room 5 of Barkhang Monastery (LostArtefacts/TR2X#196)

## [0.2](https://github.com/LostArtefacts/TR2X/compare/0.1.1...0.2) - 2024-05-07
- added dev console with the following commands:
    - `/pos`
    - `/tp [room_num]`
    - `/tp [x] [y] [z]`
    - `/hp`
    - `/hp [num]`
    - `/heal`
- changed the music backend from WinMM to libtrx (SDL + libav)
- changed the sound backend from DirectX to libtrx (SDL + libav)
- fixed seams around underwater portals (LostArtefacts/TR2X#76, regression from 0.1)
- fixed Lara's climb down camera angle (LostArtefacts/TR2X#78, regression from 0.1)
- fixed healthbar and airbar flashing the wrong way when at low values (LostArtefacts/TR2X#82, regression from 0.1)

## [0.1.1](https://github.com/LostArtefacts/TR2X/compare/0.1...0.1.1) - 2024-04-27
- fixed Lara's shadow with z-buffer option on (LostArtefacts/TR2X#64, regression from 0.1)
- fixed rare camera issues (LostArtefacts/TR2X#65, regression from 0.1)
- fixed flat rectangle colors (LostArtefacts/TR2X#70, regression from 0.1)
- fixed medipacks staying open after use in Lara's inventory (LostArtefacts/TR2X#69, regression from 0.1)
- fixed pickup sprites UI drawn forever in Lara's Home (LostArtefacts/TR2X#68, regression from 0.1)

## [0.1](https://github.com/rr-/TR2X/compare/...0.1) - 2024-04-26
- added version string to the inventory
- fixed CDAudio not playing on certain versions (uses PaulD patch)
- fixed TGA screenshots crashing the game
