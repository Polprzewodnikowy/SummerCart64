# Windows (installer)

## Installing (simplified)

**The TR3 installer is not yet ready, but we'll eventually provide it.**

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

   Unfortunately, due to legal reasons, we cannot offer an easy packaging of The Lost Artifact expansion pack.

## Verifying the installation

If you install everything correctly, your game directory should look more or less like this (click to expand):

<details data-id="file-tree-win">
<pre><code>.
├── audio
│   └── cdaudio.wad
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
│   ├── tr3
│   │   ├── gameflow.json5
│   │   ├── strings-de.json5
│   │   ├── strings-it.json5
│   │   ├── strings-pl.json5
│   │   └── strings.json5
│   ├── tr3-la
│   │   ├── gameflow.json5
│   │   ├── strings-de.json5
│   │   ├── strings-it.json5
│   │   ├── strings-pl.json5
│   │   └── strings.json5
│   ├── tr3-level
│   │   ├── gameflow.json5
│   │   ├── strings-it.json5
│   │   ├── strings-pl.json5
│   │   └── strings.json5
│   ├── TR3X.json5*
│   ├── ui.json5
│   └── weapons.json5
├── cuts
│   ├── cut1.tr2
│   ├── cut2.tr2
│   ├── cut3.tr2
│   ├── cut4.tr2
│   ├── cut5.tr2
│   ├── cut6.tr2
│   ├── cut7.tr2
│   ├── cut8.tr2
│   ├── cut9.tr2
│   ├── cut11.tr2
│   └── cut12.tr2
├── data
│   ├── antarc.tr2
│   ├── area51.tr2
│   ├── chamber.tr2
│   ├── city.tr2
│   ├── compound.tr2
│   ├── crash.tr2
│   ├── house.tr2
│   ├── images
│   │   ├── 4x3
│   │   │   ├── antarc.webp
│   │   │   ├── credit01.webp
│   │   │   ├── credit02.webp
│   │   │   ├── credit03.webp
│   │   │   ├── credit04.webp
│   │   │   ├── credit05.webp
│   │   │   ├── credit06.webp
│   │   │   ├── credit07.webp
│   │   │   ├── credit08.webp
│   │   │   ├── credit09.webp
│   │   │   ├── house.webp
│   │   │   ├── india.webp
│   │   │   ├── legal_eu.webp
│   │   │   ├── legal_us.webp
│   │   │   ├── london.webp
│   │   │   ├── nevada.webp
│   │   │   ├── southpac.webp
│   │   │   ├── theend2.webp
│   │   │   ├── title_eu.webp
│   │   │   └── title_us.webp
│   │   ├── antarc.webp
│   │   ├── credit01.webp
│   │   ├── credit02.webp
│   │   ├── credit03.webp
│   │   ├── credit04.webp
│   │   ├── credit05.webp
│   │   ├── credit06.webp
│   │   ├── credit07.webp
│   │   ├── credit08.webp
│   │   ├── credit09.webp
│   │   ├── house.webp
│   │   ├── india.webp
│   │   ├── legal_eu.webp
│   │   ├── legal_us.webp
│   │   ├── london.webp
│   │   ├── nevada.webp
│   │   ├── og
│   │   │   ├── antarc.webp
│   │   │   ├── credit01.webp
│   │   │   ├── credit02.webp
│   │   │   ├── credit03.webp
│   │   │   ├── credit04.webp
│   │   │   ├── credit05.webp
│   │   │   ├── credit06.webp
│   │   │   ├── credit07.webp
│   │   │   ├── credit08.webp
│   │   │   ├── credit09.webp
│   │   │   ├── house.webp
│   │   │   ├── india.webp
│   │   │   ├── legal_eu.webp
│   │   │   ├── legal_us.webp
│   │   │   ├── london.webp
│   │   │   ├── nevada.webp
│   │   │   ├── nevadafff.webp
│   │   │   ├── southpac.webp
│   │   │   ├── theend2.webp
│   │   │   ├── theend.webp
│   │   │   ├── title_eu.webp
│   │   │   └── title_us.webp
│   │   ├── southpac.webp
│   │   ├── theend2.webp
│   │   ├── title_eu.webp
│   │   └── title_us.webp
│   ├── injections
│   │   ├── aldwych_fd.bin
│   │   ├── aldwych_pickup_meshes.bin
│   │   ├── antarc_airlock.bin
│   │   ├── antarc_sky.bin
│   │   ├── area51_sky.bin
│   │   ├── cavern_sky.bin
│   │   ├── city_textures.bin
│   │   ├── coastal_airlock.bin
│   │   ├── coastal_animating_bounds.bin
│   │   ├── coastal_sky.bin
│   │   ├── compound_cine.bin
│   │   ├── crash_pickup_meshes.bin
│   │   ├── crash_sky.bin
│   │   ├── cut1_setup.bin
│   │   ├── cut2_setup.bin
│   │   ├── cut3_setup.bin
│   │   ├── cut3_shell.bin
│   │   ├── cut4_setup.bin
│   │   ├── cut5_setup.bin
│   │   ├── cut5_textures.bin
│   │   ├── cut6_setup.bin
│   │   ├── cut7_setup.bin
│   │   ├── cut8_setup.bin
│   │   ├── cut9_setup.bin
│   │   ├── cut11_setup.bin
│   │   ├── cut12_setup.bin
│   │   ├── font.bin
│   │   ├── globe_model.bin
│   │   ├── gym_sky.bin
│   │   ├── india_sky.bin
│   │   ├── lara_animations.bin
│   │   ├── lara_extra.bin
│   │   ├── lara_guns.bin
│   │   ├── lara_gym_guns.bin
│   │   ├── lara_outfits.bin
│   │   ├── london_sky.bin
│   │   ├── luds_diver_animation.bin
│   │   ├── misc_sprites.bin
│   │   ├── nevada_sky.bin
│   │   ├── ora_dagger.bin
│   │   ├── pda_model.bin
│   │   ├── pickup_aid.bin
│   │   ├── rapids_sky.bin
│   │   ├── reunion_flames.bin
│   │   ├── scotland_sky.bin
│   │   ├── stpaul_animating_bounds.bin
│   │   ├── stpaul_textures.bin
│   │   ├── tinnos_cameras.bin
│   │   ├── tinnos_flames.bin
│   │   ├── undersea_animating_bounds.bin
│   │   ├── undersea_train.bin
│   │   ├── willsden_heli.bin
│   │   └── zoo_train.bin
│   ├── jungle.tr2
│   ├── main.sfx
│   ├── mines.tr2
│   ├── nevada.tr2
│   ├── office.tr2
│   ├── quadchas.tr2
│   ├── rapids.tr2
│   ├── roofs.tr2
│   ├── scripts
│   │   ├── area51.lua
│   │   ├── compound.lua
│   │   ├── crash.lua
│   │   ├── cut8.lua
│   │   ├── jungle.lua
│   │   ├── tower.lua
│   │   └── zoo.lua
│   ├── sewer.tr2
│   ├── shore.tr2
│   ├── stpaul.tr2
│   ├── temple.tr2
│   ├── title.tr2
│   ├── tombpc.dat
│   ├── tonyboss.tr2
│   ├── tower.tr2
│   ├── triboss.tr2
│   └── vict.tr2
├── fmv
│   ├── crsh_eng.rpl
│   ├── endgame.rpl
│   ├── intr_eng.rpl
│   ├── logo.rpl
│   └── sail_eng.rpl
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
- To play the Lost Artifact expansion pack, run `TRX.exe --gold`.

# macOS

## Installing

1. Download the latest TRX for TR3 installer image (e.g `TRX-0.1-Mac-tr3.dmg`). Mount the image and drag TR3X to the Applications folder.
2. Run TR3X from the Applications folder. This will show you an error dialog about missing game data files. This is expected at this point, as you have not copied them in yet. However, it's important to run the app first to allow macOS to verify the app bundle's signature.
3. Find TR3X in your Applications folder. Right-click it and click "Show Package Contents".
4. Copy your Tomb Raider 3 game data files into `Contents/Resources`. (See the Windows / Linux instructions for retrieving game data from e.g. GOG.)

In case you see a popup "TR3X is damaged" when you run the game, run `xattr -cr /Applications/TR3X.app`.

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
        ├── audio
        │   └── cdaudio.wad
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
        │   ├── tr3
        │   │   ├── gameflow.json5
        │   │   ├── strings-de.json5
        │   │   ├── strings-it.json5
        │   │   ├── strings-pl.json5
        │   │   └── strings.json5
        │   ├── tr3-la
        │   │   ├── gameflow.json5
        │   │   ├── strings-de.json5
        │   │   ├── strings-it.json5
        │   │   ├── strings-pl.json5
        │   │   └── strings.json5
        │   ├── tr3-level
        │   │   ├── gameflow.json5
        │   │   ├── strings-it.json5
        │   │   ├── strings-pl.json5
        │   │   └── strings.json5
        │   ├── ui.json5
        │   └── weapons.json5
        ├── cuts
        │   ├── cut1.tr2
        │   ├── cut2.tr2
        │   ├── cut3.tr2
        │   ├── cut4.tr2
        │   ├── cut5.tr2
        │   ├── cut6.tr2
        │   ├── cut7.tr2
        │   ├── cut8.tr2
        │   ├── cut9.tr2
        │   ├── cut11.tr2
        │   └── cut12.tr2
        ├── data
        │   ├── antarc.tr2
        │   ├── area51.tr2
        │   ├── chamber.tr2
        │   ├── city.tr2
        │   ├── compound.tr2
        │   ├── crash.tr2
        │   ├── house.tr2
        │   ├── images
        │   │   ├── 4x3
        │   │   │   ├── antarc.webp
        │   │   │   ├── credit01.webp
        │   │   │   ├── credit02.webp
        │   │   │   ├── credit03.webp
        │   │   │   ├── credit04.webp
        │   │   │   ├── credit05.webp
        │   │   │   ├── credit06.webp
        │   │   │   ├── credit07.webp
        │   │   │   ├── credit08.webp
        │   │   │   ├── credit09.webp
        │   │   │   ├── house.webp
        │   │   │   ├── india.webp
        │   │   │   ├── legal_eu.webp
        │   │   │   ├── legal_us.webp
        │   │   │   ├── london.webp
        │   │   │   ├── nevada.webp
        │   │   │   ├── southpac.webp
        │   │   │   ├── theend2.webp
        │   │   │   ├── title_eu.webp
        │   │   │   └── title_us.webp
        │   │   ├── antarc.webp
        │   │   ├── credit01.webp
        │   │   ├── credit02.webp
        │   │   ├── credit03.webp
        │   │   ├── credit04.webp
        │   │   ├── credit05.webp
        │   │   ├── credit06.webp
        │   │   ├── credit07.webp
        │   │   ├── credit08.webp
        │   │   ├── credit09.webp
        │   │   ├── house.webp
        │   │   ├── india.webp
        │   │   ├── legal_eu.webp
        │   │   ├── legal_us.webp
        │   │   ├── london.webp
        │   │   ├── nevada.webp
        │   │   ├── og
        │   │   │   ├── antarc.webp
        │   │   │   ├── credit01.webp
        │   │   │   ├── credit02.webp
        │   │   │   ├── credit03.webp
        │   │   │   ├── credit04.webp
        │   │   │   ├── credit05.webp
        │   │   │   ├── credit06.webp
        │   │   │   ├── credit07.webp
        │   │   │   ├── credit08.webp
        │   │   │   ├── credit09.webp
        │   │   │   ├── house.webp
        │   │   │   ├── india.webp
        │   │   │   ├── legal_eu.webp
        │   │   │   ├── legal_us.webp
        │   │   │   ├── london.webp
        │   │   │   ├── nevada.webp
        │   │   │   ├── nevadafff.webp
        │   │   │   ├── southpac.webp
        │   │   │   ├── theend2.webp
        │   │   │   ├── theend.webp
        │   │   │   ├── title_eu.webp
        │   │   │   └── title_us.webp
        │   │   ├── southpac.webp
        │   │   ├── theend2.webp
        │   │   ├── title_eu.webp
        │   │   └── title_us.webp
        │   ├── injections
        │   │   ├── aldwych_fd.bin
        │   │   ├── aldwych_pickup_meshes.bin
        │   │   ├── antarc_airlock.bin
        │   │   ├── antarc_sky.bin
        │   │   ├── area51_sky.bin
        │   │   ├── cavern_sky.bin
        │   │   ├── city_textures.bin
        │   │   ├── coastal_airlock.bin
        │   │   ├── coastal_animating_bounds.bin
        │   │   ├── coastal_sky.bin
        │   │   ├── compound_cine.bin
        │   │   ├── crash_pickup_meshes.bin
        │   │   ├── crash_sky.bin
        │   │   ├── cut1_setup.bin
        │   │   ├── cut2_setup.bin
        │   │   ├── cut3_setup.bin
        │   │   ├── cut3_shell.bin
        │   │   ├── cut4_setup.bin
        │   │   ├── cut5_setup.bin
        │   │   ├── cut5_textures.bin
        │   │   ├── cut6_setup.bin
        │   │   ├── cut7_setup.bin
        │   │   ├── cut8_setup.bin
        │   │   ├── cut9_setup.bin
        │   │   ├── cut11_setup.bin
        │   │   ├── cut12_setup.bin
        │   │   ├── font.bin
        │   │   ├── globe_model.bin
        │   │   ├── gym_sky.bin
        │   │   ├── india_sky.bin
        │   │   ├── lara_animations.bin
        │   │   ├── lara_extra.bin
        │   │   ├── lara_guns.bin
        │   │   ├── lara_gym_guns.bin
        │   │   ├── lara_outfits.bin
        │   │   ├── london_sky.bin
        │   │   ├── luds_diver_animation.bin
        │   │   ├── misc_sprites.bin
        │   │   ├── nevada_sky.bin
        │   │   ├── ora_dagger.bin
        │   │   ├── pda_model.bin
        │   │   ├── pickup_aid.bin
        │   │   ├── rapids_sky.bin
        │   │   ├── reunion_flames.bin
        │   │   ├── scotland_sky.bin
        │   │   ├── stpaul_animating_bounds.bin
        │   │   ├── stpaul_textures.bin
        │   │   ├── tinnos_cameras.bin
        │   │   ├── tinnos_flames.bin
        │   │   ├── undersea_animating_bounds.bin
        │   │   ├── undersea_train.bin
        │   │   ├── willsden_heli.bin
        │   │   └── zoo_train.bin
        │   ├── jungle.tr2
        │   ├── main.sfx
        │   ├── mines.tr2
        │   ├── nevada.tr2
        │   ├── office.tr2
        │   ├── quadchas.tr2
        │   ├── rapids.tr2
        │   ├── roofs.tr2
        │   ├── scripts
        │   │   ├── area51.lua
        │   │   ├── compound.lua
        │   │   ├── crash.lua
        │   │   ├── cut8.lua
        │   │   ├── jungle.lua
        │   │   ├── tower.lua
        │   │   └── zoo.lua
        │   ├── sewer.tr2
        │   ├── shore.tr2
        │   ├── stpaul.tr2
        │   ├── temple.tr2
        │   ├── title.tr2
        │   ├── tombpc.dat
        │   ├── tonyboss.tr2
        │   ├── tower.tr2
        │   ├── triboss.tr2
        │   └── vict.tr2
        ├── fmv
        │   ├── crsh_eng.rpl
        │   ├── endgame.rpl
        │   ├── intr_eng.rpl
        │   ├── logo.rpl
        │   └── sail_eng.rpl
        ├── icon.icns
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
