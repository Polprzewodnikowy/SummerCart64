# Windows (installer)

## Installing (simplified)

1. Download the latest TRX installer for TR1 (e.g. `TRX-1.0-Windows_Installer-tr1.exe`).
2. Mark the installer EXE as safe to run:
    - Right-click on the `.exe`.
    - Go to properties.
    - Click "Unblock".
3. Run the installer and proceed with the steps.

> [!NOTE]
> When downloading TRX, you might see a warning from Windows Defender, your browser, or another security tool. Modern antivirus systems use AI‑based heuristics – they flag anything uncommon or unsigned as suspicious, even if it's perfectly safe. TRX can trigger these alerts because:
>
> - It isn't signed with a costly commercial certificate.
> - It's a niche, community‑built project, so not widely recognized.
> - It's a custom build, not from the Microsoft Store.
>
> Don't worry: TRX is open‑source, and you can inspect the code yourself on [GitHub](https://github.com/LostArtefacts/TRX/).

# Windows / Linux

## Installing (manual)

1. Download the TRX zip file.
2. Extract the zip file into a directory of your choice.  
     Make sure you choose to overwrite existing directories and files.
3. If installing for the first time – put your original game files into the target directory.

    **Steam / GOG users**

    1. Extract the original `GAME.BIN` / `GAME.GOG` file to your target directory.  
        On Windows, this can be done with tools like UltraISO or UniExtract.  
        On Linux, this can be done with `innoextract`, `bin2iso` and `7z`.
    2. Get the music files – unfortunately, neither GOG nor Steam ship these assets.
        - You can download the music files from the link below.  
            https://lostartefacts.dev/aux/tr1x/music.zip  
            (The legality of this approach is disputable.)
        - Rip the assets yourself from a physical PlayStation/SegaSaturn disk.
    3. Optionally, install the Unfinished Business expansion pack.
        - Pre-packaged level files containing fan-made patch to include music triggers:  
            https://lostartefacts.dev/aux/tr1x/trub-music.zip
        - Pre-packaged original level files, which do not include music triggers:  
            https://lostartefacts.dev/aux/tr1x/trub-vanilla.zip
        - Original ISO - requires more involved manual extraction:  
            https://archive.org/details/tomb-raider-i-unfinished-business-pc-eng-full-version_20201225

    **TombATI users**

    1. Copy the `data`, `fmv` and `music` directories.

## Verifying the installation

If you install everything correctly, your game directory should look more or less like this (click to expand):

<details data-id="file-tree-win">
<pre><code>.
├── cfg
│   ├── base_strings-de.json5
│   ├── base_strings-en-gb.json5
│   ├── base_strings-fr.json5
│   ├── base_strings-gd.json5
│   ├── base_strings-it.json5
│   ├── base_strings-pl.json5
│   ├── base_strings-ru.json5
│   ├── base_strings.json5
│   ├── catalog_item_actions.csv
│   ├── catalog_lara_anims.csv
│   ├── catalog_lara_states.csv
│   ├── catalog_music.csv
│   ├── catalog_objects.csv
│   ├── catalog_samples.csv
│   ├── inv_ring.json5
│   ├── outfits.json5
│   ├── poses.json5
│   ├── presets
│   │   ├── tr1-pc.json5
│   │   ├── tr1-ps1.json5
│   │   ├── tr2-pc.json5
│   │   ├── tr2-ps1.json5
│   │   ├── tr3-pc.json5
│   │   └── tr3-ps1.json5
│   ├── tr1
│   │   ├── gameflow.json5
│   │   ├── strings-de.json5
│   │   ├── strings-en-gb.json5
│   │   ├── strings-fr.json5
│   │   ├── strings-gd.json5
│   │   ├── strings-it.json5
│   │   ├── strings-pl.json5
│   │   ├── strings-ru.json5
│   │   └── strings.json5
│   ├── tr1-demo-pc
│   │   ├── gameflow.json5
│   │   ├── strings-de.json5
│   │   ├── strings-fr.json5
│   │   ├── strings-gd.json5
│   │   ├── strings-it.json5
│   │   ├── strings-pl.json5
│   │   ├── strings-ru.json5
│   │   └── strings.json5
│   ├── tr1-level
│   │   ├── gameflow.json5
│   │   ├── strings-de.json5
│   │   ├── strings-fr.json5
│   │   ├── strings-gd.json5
│   │   ├── strings-it.json5
│   │   ├── strings-pl.json5
│   │   ├── strings-ru.json5
│   │   └── strings.json5
│   ├── tr1-ub
│   │   ├── gameflow.json5
│   │   ├── strings-de.json5
│   │   ├── strings-fr.json5
│   │   ├── strings-gd.json5
│   │   ├── strings-it.json5
│   │   ├── strings-pl.json5
│   │   ├── strings-ru.json5
│   │   └── strings.json5
│   ├── TR1X.json5*
│   ├── ui.json5
│   └── weapons.json5
├── data
│   ├── cat.phd
│   ├── cut1.phd
│   ├── cut2.phd
│   ├── cut3.phd
│   ├── cut4.phd
│   ├── egypt.phd
│   ├── end2.phd
│   ├── end.phd
│   ├── gym.phd
│   ├── images
│   │   ├── atlantis.webp
│   │   ├── credits_1.webp
│   │   ├── credits_2.webp
│   │   ├── credits_3.webp
│   │   ├── credits_3_alt.webp
│   │   ├── credits_ps1.webp
│   │   ├── credits_ub.webp
│   │   ├── egypt.webp
│   │   ├── eidos.webp
│   │   ├── end.webp
│   │   ├── greece.webp
│   │   ├── greece_saturn.webp
│   │   ├── gym.webp
│   │   ├── install.webp
│   │   ├── peru.webp
│   │   ├── title.webp
│   │   ├── title_og_alt.webp
│   │   ├── title_ub.webp
│   │   ├── ub_loading1.webp
│   │   └── ub_loading2.webp
│   ├── injections
│   │   ├── atlantis_door_sfx.bin
│   │   ├── atlantis_fd.bin
│   │   ├── atlantis_itemrots.bin
│   │   ├── atlantis_textures.bin
│   │   ├── braid.bin
│   │   ├── bubbles.bin
│   │   ├── cat_cameras.bin
│   │   ├── cat_fd.bin
│   │   ├── cat_itemrots.bin
│   │   ├── cat_meshfixes.bin
│   │   ├── cat_textures.bin
│   │   ├── caves_fd.bin
│   │   ├── caves_itemrots.bin
│   │   ├── caves_textures.bin
│   │   ├── cistern_fd.bin
│   │   ├── cistern_itemrots.bin
│   │   ├── cistern_plants.bin
│   │   ├── cistern_skybox.bin
│   │   ├── cistern_textures.bin
│   │   ├── colosseum_fd.bin
│   │   ├── colosseum_itemrots.bin
│   │   ├── colosseum_skybox.bin
│   │   ├── colosseum_textures.bin
│   │   ├── cut1_setup.bin
│   │   ├── cut2_setup.bin
│   │   ├── cut3_setup.bin
│   │   ├── cut3_textures.bin
│   │   ├── cut4_setup.bin
│   │   ├── cut4_textures.bin
│   │   ├── door58_frames.bin
│   │   ├── door59_frames.bin
│   │   ├── door59_sfx.bin
│   │   ├── door60_frames.bin
│   │   ├── door61_sfx.bin
│   │   ├── egypt_cameras.bin
│   │   ├── egypt_fd.bin
│   │   ├── egypt_itemrots.bin
│   │   ├── egypt_meshfixes.bin
│   │   ├── egypt_textures.bin
│   │   ├── explosion.bin
│   │   ├── folly_fd.bin
│   │   ├── folly_itemrots.bin
│   │   ├── folly_pickup_meshes.bin
│   │   ├── folly_textures.bin
│   │   ├── font.bin
│   │   ├── gun_glow.bin
│   │   ├── gym_textures.bin
│   │   ├── hive_fd.bin
│   │   ├── hive_itemrots.bin
│   │   ├── hive_textures.bin
│   │   ├── khamoon_fd.bin
│   │   ├── khamoon_itemrots.bin
│   │   ├── khamoon_meshfixes.bin
│   │   ├── khamoon_mummy.bin
│   │   ├── khamoon_textures.bin
│   │   ├── lara_animations.bin
│   │   ├── lara_extra.bin
│   │   ├── lara_feet_sfx.bin
│   │   ├── lara_flares.bin
│   │   ├── lara_guns.bin
│   │   ├── lara_gym_flares.bin
│   │   ├── lara_gym_guns.bin
│   │   ├── lara_outfits.bin
│   │   ├── midas_itemrots.bin
│   │   ├── midas_textures.bin
│   │   ├── mines_cameras.bin
│   │   ├── mines_door_sfx.bin
│   │   ├── mines_fd.bin
│   │   ├── mines_itemrots.bin
│   │   ├── mines_meshfixes.bin
│   │   ├── mines_pushblocks.bin
│   │   ├── mines_textures.bin
│   │   ├── misc_sprites.bin
│   │   ├── obelisk_fd.bin
│   │   ├── obelisk_itemrots.bin
│   │   ├── obelisk_meshfixes.bin
│   │   ├── obelisk_skybox.bin
│   │   ├── obelisk_textures.bin
│   │   ├── panther_sfx.bin
│   │   ├── pda_model.bin
│   │   ├── photo.bin
│   │   ├── pickup_aid.bin
│   │   ├── purple_crystal.bin
│   │   ├── pyramid_fd.bin
│   │   ├── pyramid_itemrots.bin
│   │   ├── pyramid_textures.bin
│   │   ├── qualopec_door_sfx.bin
│   │   ├── qualopec_fd.bin
│   │   ├── qualopec_itemrots.bin
│   │   ├── qualopec_textures.bin
│   │   ├── sanctuary_fd.bin
│   │   ├── sanctuary_itemrots.bin
│   │   ├── sanctuary_scion.bin
│   │   ├── sanctuary_textures.bin
│   │   ├── scion_collision.bin
│   │   ├── skate_kid_sfx.bin
│   │   ├── sprite_alignment.bin
│   │   ├── stronghold_fd.bin
│   │   ├── stronghold_itemrots.bin
│   │   ├── stronghold_textures.bin
│   │   ├── tihocan_fd.bin
│   │   ├── tihocan_itemrots.bin
│   │   ├── tihocan_skybox.bin
│   │   ├── tihocan_textures.bin
│   │   ├── title_textures.bin
│   │   ├── uzi_sfx.bin
│   │   ├── valley_fd.bin
│   │   ├── valley_itemrots.bin
│   │   ├── valley_skybox.bin
│   │   ├── valley_textures.bin
│   │   ├── vilcabamba_door_sfx.bin
│   │   ├── vilcabamba_itemrots.bin
│   │   ├── vilcabamba_textures.bin
│   │   └── winston_model.bin
│   ├── level1.phd
│   ├── level2.phd
│   ├── level3a.phd
│   ├── level3b.phd
│   ├── level4.phd
│   ├── level5.phd
│   ├── level6.phd
│   ├── level7a.phd
│   ├── level7b.phd
│   ├── level8a.phd
│   ├── level8b.phd
│   ├── level8c.phd
│   ├── level10a.phd
│   ├── level10b.phd
│   ├── level10c.phd
│   ├── scripts
│   │   └── gym.lua
│   └── title.phd
├── fmv
│   ├── cafe.rpl
│   ├── canyon.rpl
│   ├── core.avi
│   ├── end.rpl
│   ├── escape.rpl
│   ├── lift.rpl
│   ├── mansion.rpl
│   ├── prison.rpl
│   ├── pyramid.rpl
│   ├── snow.rpl
│   └── vision.rpl
├── music
│   ├── track02.flac
│   ├── track03.flac
│   ├── track04.flac
│   ├── track05.flac
│   ├── track06.flac
│   ├── track07.flac
│   ├── track08.flac
│   ├── track09.flac
│   ├── track10.flac
│   ├── track11.flac
│   ├── track12.flac
│   ├── track13.flac
│   ├── track14.flac
│   ├── track15.flac
│   ├── track16.flac
│   ├── track17.flac
│   ├── track18.flac
│   ├── track19.flac
│   ├── track20.flac
│   ├── track21.flac
│   ├── track22.flac
│   ├── track23.flac
│   ├── track24.flac
│   ├── track25.flac
│   ├── track26.flac
│   ├── track27.flac
│   ├── track28.flac
│   ├── track29.flac
│   ├── track30.flac
│   ├── track31.flac
│   ├── track32.flac
│   ├── track33.flac
│   ├── track34.flac
│   ├── track35.flac
│   ├── track36.flac
│   ├── track37.flac
│   ├── track38.flac
│   ├── track39.flac
│   ├── track40.flac
│   ├── track41.flac
│   ├── track42.flac
│   ├── track43.flac
│   ├── track44.flac
│   ├── track45.flac
│   ├── track46.flac
│   ├── track47.flac
│   ├── track48.flac
│   ├── track49.flac
│   ├── track50.flac
│   ├── track51.flac
│   ├── track52.flac
│   ├── track53.flac
│   ├── track54.flac
│   ├── track55.flac
│   ├── track56.flac
│   ├── track57.flac
│   ├── track58.flac
│   ├── track59.flac
│   └── track60.flac
├── shaders
│   ├── 2d.glsl
│   ├── billboard.glsl
│   ├── common.glsl
│   ├── fbo.glsl
│   ├── lights.glsl
│   ├── meshes.glsl
│   ├── meshes_tr3.glsl
│   ├── meshes_tr12.glsl
│   └── ui.glsl
└── TRX.exe</code></pre>
</details>

*\* Will not be present until the game has been launched.*

## Playing the game

- To play the game, run `TRX.exe`.
- To play the Unfinished Business expansion pack, run `TRX.exe --gold`.

# macOS

## Installing

1. Download the latest TRX for TR1 installer image (e.g `TRX-0.1-Mac-tr1.dmg`). Mount the image and drag TR1X to the Applications folder.
2. Run TR1X from the Applications folder. This will show you an error dialog about missing game data files. This is expected at this point, as you have not copied them in yet. However, it's important to run the app first to allow macOS to verify the app bundle's signature.
3. Find TR1X in your Applications folder. Right-click it and click "Show Package Contents".
4. Copy your Tomb Raider 1 game data files into `Contents/Resources`. (See the Windows / Linux instructions for retrieving game data from e.g. GOG.)

In case you see a popup "TR1X is damaged" when you run the game, run `xattr -cr /Applications/TR1X.app`.

## Verifying the installation

If you install everything correctly, your game directory should look more or less like this (click to expand):

<details data-id="file-tree-mac">
<pre><code>.
└── Contents
    ├── _CodeSignature
    ├── Frameworks
    ├── info.plist
    ├── MacOS
    └── Resources
        ├── cfg
        │   ├── base_strings-de.json5
        │   ├── base_strings-en-gb.json5
        │   ├── base_strings-fr.json5
        │   ├── base_strings-gd.json5
        │   ├── base_strings-it.json5
        │   ├── base_strings-pl.json5
        │   ├── base_strings-ru.json5
        │   ├── base_strings.json5
        │   ├── catalog_item_actions.csv
        │   ├── catalog_lara_anims.csv
        │   ├── catalog_lara_states.csv
        │   ├── catalog_music.csv
        │   ├── catalog_objects.csv
        │   ├── catalog_samples.csv
        │   ├── inv_ring.json5
        │   ├── outfits.json5
        │   ├── poses.json5
        │   ├── presets
        │   │   ├── tr1-pc.json5
        │   │   ├── tr1-ps1.json5
        │   │   ├── tr2-pc.json5
        │   │   ├── tr2-ps1.json5
        │   │   ├── tr3-pc.json5
        │   │   └── tr3-ps1.json5
        │   ├── tr1
        │   │   ├── gameflow.json5
        │   │   ├── strings-de.json5
        │   │   ├── strings-en-gb.json5
        │   │   ├── strings-fr.json5
        │   │   ├── strings-gd.json5
        │   │   ├── strings-it.json5
        │   │   ├── strings-pl.json5
        │   │   ├── strings-ru.json5
        │   │   └── strings.json5
        │   ├── tr1-demo-pc
        │   │   ├── gameflow.json5
        │   │   ├── strings-de.json5
        │   │   ├── strings-fr.json5
        │   │   ├── strings-gd.json5
        │   │   ├── strings-it.json5
        │   │   ├── strings-pl.json5
        │   │   ├── strings-ru.json5
        │   │   └── strings.json5
        │   ├── tr1-level
        │   │   ├── gameflow.json5
        │   │   ├── strings-de.json5
        │   │   ├── strings-fr.json5
        │   │   ├── strings-gd.json5
        │   │   ├── strings-it.json5
        │   │   ├── strings-pl.json5
        │   │   ├── strings-ru.json5
        │   │   └── strings.json5
        │   ├── tr1-ub
        │   │   ├── gameflow.json5
        │   │   ├── strings-de.json5
        │   │   ├── strings-fr.json5
        │   │   ├── strings-gd.json5
        │   │   ├── strings-it.json5
        │   │   ├── strings-pl.json5
        │   │   ├── strings-ru.json5
        │   │   └── strings.json5
        │   ├── ui.json5
        │   └── weapons.json5
        ├── data
        │   ├── cat.phd
        │   ├── cut1.phd
        │   ├── cut2.phd
        │   ├── cut3.phd
        │   ├── cut4.phd
        │   ├── egypt.phd
        │   ├── end2.phd
        │   ├── end.phd
        │   ├── gym.phd
        │   ├── images
        │   │   ├── atlantis.webp
        │   │   ├── credits_1.webp
        │   │   ├── credits_2.webp
        │   │   ├── credits_3.webp
        │   │   ├── credits_3_alt.webp
        │   │   ├── credits_ps1.webp
        │   │   ├── credits_ub.webp
        │   │   ├── egypt.webp
        │   │   ├── eidos.webp
        │   │   ├── end.webp
        │   │   ├── greece.webp
        │   │   ├── greece_saturn.webp
        │   │   ├── gym.webp
        │   │   ├── install.webp
        │   │   ├── peru.webp
        │   │   ├── title.webp
        │   │   ├── title_og_alt.webp
        │   │   ├── title_ub.webp
        │   │   ├── ub_loading1.webp
        │   │   └── ub_loading2.webp
        │   ├── injections
        │   │   ├── atlantis_door_sfx.bin
        │   │   ├── atlantis_fd.bin
        │   │   ├── atlantis_itemrots.bin
        │   │   ├── atlantis_textures.bin
        │   │   ├── braid.bin
        │   │   ├── bubbles.bin
        │   │   ├── cat_cameras.bin
        │   │   ├── cat_fd.bin
        │   │   ├── cat_itemrots.bin
        │   │   ├── cat_meshfixes.bin
        │   │   ├── cat_textures.bin
        │   │   ├── caves_fd.bin
        │   │   ├── caves_itemrots.bin
        │   │   ├── caves_textures.bin
        │   │   ├── cistern_fd.bin
        │   │   ├── cistern_itemrots.bin
        │   │   ├── cistern_plants.bin
        │   │   ├── cistern_skybox.bin
        │   │   ├── cistern_textures.bin
        │   │   ├── colosseum_fd.bin
        │   │   ├── colosseum_itemrots.bin
        │   │   ├── colosseum_skybox.bin
        │   │   ├── colosseum_textures.bin
        │   │   ├── cut1_setup.bin
        │   │   ├── cut2_setup.bin
        │   │   ├── cut3_setup.bin
        │   │   ├── cut3_textures.bin
        │   │   ├── cut4_setup.bin
        │   │   ├── cut4_textures.bin
        │   │   ├── door58_frames.bin
        │   │   ├── door59_frames.bin
        │   │   ├── door59_sfx.bin
        │   │   ├── door60_frames.bin
        │   │   ├── door61_sfx.bin
        │   │   ├── egypt_cameras.bin
        │   │   ├── egypt_fd.bin
        │   │   ├── egypt_itemrots.bin
        │   │   ├── egypt_meshfixes.bin
        │   │   ├── egypt_textures.bin
        │   │   ├── explosion.bin
        │   │   ├── folly_fd.bin
        │   │   ├── folly_itemrots.bin
        │   │   ├── folly_pickup_meshes.bin
        │   │   ├── folly_textures.bin
        │   │   ├── font.bin
        │   │   ├── gun_glow.bin
        │   │   ├── gym_textures.bin
        │   │   ├── hive_fd.bin
        │   │   ├── hive_itemrots.bin
        │   │   ├── hive_textures.bin
        │   │   ├── khamoon_fd.bin
        │   │   ├── khamoon_itemrots.bin
        │   │   ├── khamoon_meshfixes.bin
        │   │   ├── khamoon_mummy.bin
        │   │   ├── khamoon_textures.bin
        │   │   ├── lara_animations.bin
        │   │   ├── lara_extra.bin
        │   │   ├── lara_feet_sfx.bin
        │   │   ├── lara_flares.bin
        │   │   ├── lara_guns.bin
        │   │   ├── lara_gym_flares.bin
        │   │   ├── lara_gym_guns.bin
        │   │   ├── lara_outfits.bin
        │   │   ├── midas_itemrots.bin
        │   │   ├── midas_textures.bin
        │   │   ├── mines_cameras.bin
        │   │   ├── mines_door_sfx.bin
        │   │   ├── mines_fd.bin
        │   │   ├── mines_itemrots.bin
        │   │   ├── mines_meshfixes.bin
        │   │   ├── mines_pushblocks.bin
        │   │   ├── mines_textures.bin
        │   │   ├── misc_sprites.bin
        │   │   ├── obelisk_fd.bin
        │   │   ├── obelisk_itemrots.bin
        │   │   ├── obelisk_meshfixes.bin
        │   │   ├── obelisk_skybox.bin
        │   │   ├── obelisk_textures.bin
        │   │   ├── panther_sfx.bin
        │   │   ├── pda_model.bin
        │   │   ├── photo.bin
        │   │   ├── pickup_aid.bin
        │   │   ├── purple_crystal.bin
        │   │   ├── pyramid_fd.bin
        │   │   ├── pyramid_itemrots.bin
        │   │   ├── pyramid_textures.bin
        │   │   ├── qualopec_door_sfx.bin
        │   │   ├── qualopec_fd.bin
        │   │   ├── qualopec_itemrots.bin
        │   │   ├── qualopec_textures.bin
        │   │   ├── sanctuary_fd.bin
        │   │   ├── sanctuary_itemrots.bin
        │   │   ├── sanctuary_scion.bin
        │   │   ├── sanctuary_textures.bin
        │   │   ├── scion_collision.bin
        │   │   ├── skate_kid_sfx.bin
        │   │   ├── sprite_alignment.bin
        │   │   ├── stronghold_fd.bin
        │   │   ├── stronghold_itemrots.bin
        │   │   ├── stronghold_textures.bin
        │   │   ├── tihocan_fd.bin
        │   │   ├── tihocan_itemrots.bin
        │   │   ├── tihocan_skybox.bin
        │   │   ├── tihocan_textures.bin
        │   │   ├── title_textures.bin
        │   │   ├── uzi_sfx.bin
        │   │   ├── valley_fd.bin
        │   │   ├── valley_itemrots.bin
        │   │   ├── valley_skybox.bin
        │   │   ├── valley_textures.bin
        │   │   ├── vilcabamba_door_sfx.bin
        │   │   ├── vilcabamba_itemrots.bin
        │   │   ├── vilcabamba_textures.bin
        │   │   └── winston_model.bin
        │   ├── level1.phd
        │   ├── level2.phd
        │   ├── level3a.phd
        │   ├── level3b.phd
        │   ├── level4.phd
        │   ├── level5.phd
        │   ├── level6.phd
        │   ├── level7a.phd
        │   ├── level7b.phd
        │   ├── level8a.phd
        │   ├── level8b.phd
        │   ├── level8c.phd
        │   ├── level10a.phd
        │   ├── level10b.phd
        │   ├── level10c.phd
        │   ├── scripts
        │   │   └── gym.lua
        │   └── title.phd
        ├── fmv
        │   ├── cafe.rpl
        │   ├── canyon.rpl
        │   ├── core.avi
        │   ├── end.rpl
        │   ├── escape.rpl
        │   ├── lift.rpl
        │   ├── mansion.rpl
        │   ├── prison.rpl
        │   ├── pyramid.rpl
        │   ├── snow.rpl
        │   └── vision.rpl
        ├── icon.icns
        ├── music
        │   ├── track02.flac
        │   ├── track03.flac
        │   ├── track04.flac
        │   ├── track05.flac
        │   ├── track06.flac
        │   ├── track07.flac
        │   ├── track08.flac
        │   ├── track09.flac
        │   ├── track10.flac
        │   ├── track11.flac
        │   ├── track12.flac
        │   ├── track13.flac
        │   ├── track14.flac
        │   ├── track15.flac
        │   ├── track16.flac
        │   ├── track17.flac
        │   ├── track18.flac
        │   ├── track19.flac
        │   ├── track20.flac
        │   ├── track21.flac
        │   ├── track22.flac
        │   ├── track23.flac
        │   ├── track24.flac
        │   ├── track25.flac
        │   ├── track26.flac
        │   ├── track27.flac
        │   ├── track28.flac
        │   ├── track29.flac
        │   ├── track30.flac
        │   ├── track31.flac
        │   ├── track32.flac
        │   ├── track33.flac
        │   ├── track34.flac
        │   ├── track35.flac
        │   ├── track36.flac
        │   ├── track37.flac
        │   ├── track38.flac
        │   ├── track39.flac
        │   ├── track40.flac
        │   ├── track41.flac
        │   ├── track42.flac
        │   ├── track43.flac
        │   ├── track44.flac
        │   ├── track45.flac
        │   ├── track46.flac
        │   ├── track47.flac
        │   ├── track48.flac
        │   ├── track49.flac
        │   ├── track50.flac
        │   ├── track51.flac
        │   ├── track52.flac
        │   ├── track53.flac
        │   ├── track54.flac
        │   ├── track55.flac
        │   ├── track56.flac
        │   ├── track57.flac
        │   ├── track58.flac
        │   ├── track59.flac
        │   └── track60.flac
        └── shaders
            ├── 2d.glsl
            ├── billboard.glsl
            ├── common.glsl
            ├── fbo.glsl
            ├── lights.glsl
            ├── meshes.glsl
            ├── meshes_tr3.glsl
            ├── meshes_tr12.glsl
            └── ui.glsl</code></pre>
</details>

*\* Will not be present until the game has been launched.*
