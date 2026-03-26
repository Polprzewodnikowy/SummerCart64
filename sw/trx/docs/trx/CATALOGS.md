---
title: Catalogs
order: 7
---

# Catalogs

TombEditor normally lets builders manage WADs and object slots through its
catalogs. The snag is that the game engine itself still references certain
slots directly in code. That means if a builder repurposes one of those
hardcoded slots for an animation command, they might get an ugly surprise when
another in‑game object tries to use that same slot behind the scenes.

That's where TRX catalogs come in.

Before TRX 1.0, all object IDs and music tracks were hardcoded for TR1 and TR2.
Now, builders can freely re‑assign those IDs however they want. In the future,
the original lists will allow extensions by including objects from other games!

Under the hood, each entity is identified by its stable name string. The catalog
maps numeric slots to these name keys, so when the engine references an entity,
it can grab the correct sample or resource tied to that slot.

Catalogs are just comma‑separated value (CSV) files you can edit with any text
editor, including Notepad or Excel. They live in the `cfg/` folder and must be
present for the game to function properly.

TRX catalogs only include data that's directly referenced by the game's code.
Entries used *only* in animation commands or other editor‑controlled behaviors
aren't included, since those can already be managed freely within the level
editor.

## Working example

Let's say, as a builder, you wish to use the bird monster from TR2 in a TR1
level. Here are the necessary steps to set this up.

1. Make a note of the OG TR2 slot for the bird monster e.g. from WadTool or
trview. In this case, it's `46`.
2. Look-up the object name for TR2. To do this, download a copy of TRX with TR2
assets and open the `cfg/catalog_objects.csv` file. For slot 46, the name is
`O_BIRD_GUARDIAN`.
3. Choose a slot in your TR1 level that you wish to use for this object. You can
pick a new slot, or replace an existing one. Add the object to the slot in
WadTool and keep a note of the number you chose.
4. In your TR1 level folder, open `cfg/catalog_objects.csv`. Add an entry on a
new line with the slot number and the object name, ensuring you have a comma in
between. For example, if you chose slot `247`, the line would be:
<br/><br/>
`247, O_BIRD_GUARDIAN`
<br/><br/>
If you chose to replace an existing object, instead of adding a new line, locate
that slot in the CSV file and just replace the object name.

That's it! You can now place a bird monster in your level in TombEditor. You can
also proceed to check which sounds the bird monster uses in its animations in
WadTool and set up appropriate ones for TR1.
