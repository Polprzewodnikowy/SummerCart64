## Glyph Generation Tools

These tools work alongside the injection tool to expand the original Tomb
Raider character set by adding new characters and accents.

### Overview

The game displays text using bitmaps, which are organized as sprites within the
alphabet object found in .phd and .tr2 level files. Additionally, the game
executable has hardcoded information regarding the locations, sizes, and
indices of these glyphs.

Expanding the character set involves a complex process requiring the use of
multiple tools.

**Summary:**

`mapping.txt` + `glyphs.png` →
`generate_defs` →
intermediary files for the injection tool →
TRXInjectionTool →
`font.bin` injected into the game.

### `mapping.txt` and `glyphs.png`

The master files are located at `data/tr*/glyphs/*.{png,txt}`. The PNG files
are sprite sheets containing each character, while the text files provide
metadata for each glyph or icon's location, as well as optional transforms such
as shifts or bounding box resizes, using a domain specific language.

Some characters are composed using special combining sprites. For instance,
instead of creating an accented version of the character `a` for each
variation, we use a single `a` sprite and separate sprites for all possible
accents. This method allows accents to be combined with various base characters
without redundancy. Every individual variation still needs to be defined in
`mapping.txt`.

All characters are encoded in UTF-8, meaning each character can consist of
multiple bytes. Icons and buttons follow the same approach and are represented
by ASCII sequences like `\{button x}`. The game processes these similarly to
other glyphs.

### `generate_keyboard_map`

Takes blank keycap bitmaps and a bitmap font and outputs a sprite sheet and a
mapping.txt suitable for the `generate_defs` script.

### `generate_defs`

This tool processes `mapping.txt` and the associated source images to create
intermediary files needed by the injection tool, as well as update internal
definition files that end up embedded into the executable at the compilation
phase. It requires an argument for the game version since different games have
unique font styles and specifications. The result is a packed texture atlas
(which the injection tool later repacks) and a JSON file detailing each sprite
texture's location. These files must be placed in their relevant directories
within the injection tool resources. Afterward, the injection tool can be ran
to generate the final output file, `font.bin`, which should be eventually
placed in the injections directory for the game's use.

### `test_alignment.html`

This testing tool showcases how `mapping.txt` segments the sprite sheets,
allowing the developers to verify and correct any alignment issues.

### `generate_compositions`

This is a simple development tool that outputs all valid accented characters to
the standard output, and it is not a direct part of the main pipeline.

### `test_language`

Another development tool, this tests language coverage for the languages
intended to be supported. It operates based on `mapping.txt`, but is not
directly involved in the main pipeline.
