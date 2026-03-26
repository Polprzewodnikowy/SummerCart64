---
title: Commands
order: 2
---

# Commands

TRX introduces a developer console, by default accessible with the <kbd>/</kbd> key.
This key can be rebound in the controls dialog to anything of your choice. Note that
where <kbd>/</kbd> is used in command documentation, you should interpret that as
whichever key you have bound, and not include it as part of the command itself.

## Gameplay commands

- `/help`  
  `/help {command}`  
  Shows a list of the available commands or a detailed help message for the chosen one. Even Lara needs a lifeline!

- `/pos`  
  Reveals Lara's exact coordinates in the universe. Knowledge is power!

- `/tp {room_number}`  
  `/tp room {room_number}`  
  `/tp r{room_number}`  
  `/tp item {item_number}`  
  `/tp i{item_number}`  
  `/tp precise {x} {y} {z}`  
  `/tp {x} {y} {z}`  
  `/tp {object}`  
  `/tp enemy`  
  `/tp pickup`  
  Instant travel! Teleports Lara to:
  - a random spot within the specified room;
  - an item's position by item number;
  - the specified X,Y,Z coordinates in grid units;
  - the specified world-space coordinates with `precise` (no `1024` scaling);
  - the next pickup in round-robin order with `pickup`;
  - the next hostile creature in round-robin order with `enemy`;
  - the nearest object of a specific type.
  - legacy: `/tp {room_number}` is still accepted.

- `/hp`  
  `/hp {health}`  
  Displays or sets Lara's health. Tougher trials await!

- `/heal`  
  Tough day, Lara? Heals our girl back to full health.

- `/give {item_name}`  
  `/give {num} {item_name}`  
  `/give all`  
  `/give guns` or `/guns`  
  `/give moreguns` or `/moreguns`  
  `/give keys` or `/keys`  
  Gives Lara an item. Try `/give guns` to arm her to the teeth, and `/give keys` to get her all important puzzle items. Ain't nobody got time for searching!

- `/secret`  
  `/secret take`  
  `/secret take {num}`  
  `/secret give`  
  `/secret give {num}`  
  Uncovers Lara's secret stash: list discovered secrets, pilfer one or all with `take`, or gift one or all back with `give`.

- `/kill`  
  `/kill all`  
  `/kill {enemy_type}`  
  Tired of all of those pesky creatures and goons trying to spoil your adventure? Instantly dispose of the nearest one, or kill them all at once.

- `/fly`  
  `/fly on`  
  `/fly off`  
  Turns on the fly cheat. Why even walk? Levitate like a legend.

- `/immune`  
  `/immune on`  
  `/immune off`  
  Turns on immunity, making Lara impervious to harm. Perfect for when you'd rather explore every nook than tiptoe past traps.

- `/restless`  
  `/restless on`  
  `/restless off`  
  Turns on infinite sprint. Lara's always been a speedster, but with this, even cheetahs are asking her for running tips!

- `/teatime`  
  Calls your loyal butler to whatever ends of the world you're exploring right now. Effective immediately.

- `/spawn {object}`  
  Spawn an object of your choice. Not guaranteed to behave, but good for testing and oddly therapeutic for goofing off.

- `/trigger {item_num}`  
  `/trigger {item_name}`  
  `/trigger {object}`  
  Force-triggers one or more items by their item ID, item name, or object name. Great for testing switches, traps, and scripted events.

- `/untrigger {item_num}`  
  `/untrigger {item_name}`  
  `/untrigger {object}`  
  Reverses `/trigger` for one or more items.

## Configuration commands

- `/set {option}`  
  `/set {option} {value}`  
  `/set {option} -`  
  Retrieve or change specific configuration options, like a tech-savvy wizard.
  - use `-` as `{value}` to restore the option to default.
  - some options need a game or level re-launch to apply.
  - option names use `-`, not `_`, because reasons.

- `/cheats on`  
  `/cheats off`  
  Enables or disables the cheater's toolkit. But let's face it – you're reading _this_, so that ship has sailed.

- `/braid on`  
  `/braid off`  
  Toggle Lara's braid like it's a fashion accessory. Hair today, gone tomorrow.

- `/wireframe on`  
  `/wireframe off`  
  Enables or disables the wireframe mode. Enter the debugging realm!

- `/lighting on`  
  `/lighting off`  
  Enables or disables the lighting system. Bask in dynamic shadows or embrace bright clarity!

- `/textures on`  
  `/textures off`  
  Enables or disables texture rendering. Peek the exact polygons that power these pretty visuals!

- `/debug on`  
  `/debug off`  
  `/debug {option} on`  
  `/debug {option} off`  
  Toggles debug mode, turning your screen into a glorious display of dev scribbles.
  - floor triggers - enemy skips incoming!
  - room portals - wait, there are _how_ many rooms?!
  - room clip rectangles – the source of developers nightmares.
  - object mesh spheres - see hitboxes in their natural habitat.
  - Lara's position and animation details - nerdy stats, you've gotta love them.
  - bounding boxes – to marvel at the collision code.

- `/speed`  
  `/speed {num}`  
  Displays or sets the speed of the game. Because sometimes you want to moonwalk through mayhem.

- `/vsync on`  
  `/vsync off`  
  Turns vertical sync on or off. For the smooth freaks among us.

- `/fps`  
  `/fps {num}`  
  Displays or sets the game's frames per second. Higher FPS = smoother Lara.

- `/weather off`  
  `/weather snow`  
  `/weather rain`  
  Changes the current level weather. Your game, your forecast.

## Environmental commands

- `/flip`  
  `/flip off`  
  `/flip on`  
  Switches the global flipmap on or off. Turn the reality around you on its head.

- `/flood`  
  `/drain`  
  `/flood {room_num}`  
  `/drain {room_num}`  
  Floods or drains rooms at will. Act like you're Poseidon with a plumbing license, for when drowning is preferable to puzzles!

- `/music {track_id}`  
  Plays a music track by its ID. Perfect for setting the mood at will.

- `/sfx`  
  `/sfx {sound}`  
  Plays a sound effect on demand. Because sometimes you just need Lara to grunt on cue.

## Game flow commands

- `/endlevel`  
  `/nextlevel`  
  Too cool to finish puzzles? Smash-cut to the ending! Lara doesn't have time for this nonsense.

- `/level {num}`  
  `/level {name}`  
  `/play {num}`  
  `/play {name}`  
  Launches any level you like. Start with `/play 0` to warm up in the gym – or skip straight to the danger zone with `/play 1` onwards.

- `/cut {num}`  
  `/cutscene {num}`  
  Plays a dramatic cinematic. Follow the lore!

- `/gym`  
  `/home`  
  Sends Lara to her humble abode. Even tomb raiders can't skip leg day.

- `/save {slot_num}`  
  `/save quick`  
  `/quicksave`  
  `/qs`  
  Save your progress to a normal slot, or do a rotating quick-save.

- `/load {slot_num}`  
  `/load quick [slot_num]`  
  `/load q[slot_num]`  
  `/quickload [slot_num]`  
  `/ql [slot_num]`  
  Time-travel to a previous normal save, or load a quick-save by sorted quick-save position (most recent is `1`).

- `/demo`  
  `/demo {num}`  
  Starts a demo. No number? They'll just cycle.

- `/title`  
  Had enough? Let's return to the main menu.

- `/mod {name}`  
  Switches to another installed game/mod pack and reloads the game flow. Great for hopping between adventures without relaunching TRX.

- `/exit`  
  Closes the game. Ends the adventure. We're done here.

## Miscellaneous commands

- `/cls`  
  `/clear`
  Wipes the console logs, quickly erasing all traces of your cheat spree (or that ugly pile of debug misery).

- `/strings`  
  Reloads the current language files on the fly. Très utile for translators.

- `/screenshot [path]`  
  Commemorates Lara's antics by taking a picture and saving it to the optional path (relative to the game root directory).

- `/lua {string}`
  Type any LUA code to run it on the spot. Proceed with caution, or at least a sense of adventure!
