---
title: Item drops
order: 4
---

## Item drops

In the original Tomb Raider I, items dropped by enemies were hardcoded such
that only specific enemies could drop, and the items and quantities that they
dropped were immutable. This is no longer the case, with the game flow providing
a mechanism to allow the _majority_ of enemy types to carry and drop items.
Note that this also means by default that the original enemies who did drop
items will not do so unless the game flow has been configured as such.

Item drops can be defined in two ways. If `enable_tr2_item_drops` is `true`,
then custom level builders can add items directly to the level file, setting
their position to be the same as the enemies who should drop them.

For the original TR1 levels, `enable_tr2_item_drops` is `false`. Item drops are
instead defined in the `item_drops` section of a level's definition by creating
objects with the following parameter structure. You can define at most one entry
per enemy, but that definition can have as many drop items as necessary (within
the engine's overall item limit).

<details>
<summary>Show example setup</summary>

```json5
{
    "path": "data/example.phd",
    "music_track": 57,
    "item_drops": [
        {"enemy_num": 17, "object_ids": [86]},
        {"enemy_num": 50, "object_ids": [87]},
        {"enemy_num": 12, "object_ids": [93, 93]},
        {"enemy_num": 47, "object_ids": [111]},
    ],
    "sequence": [
         {"type": "loop_game"},
         {"type": "level_stats"},
         {"type": "level_complete"},
    ],
},
```

This translates as follows.
- Enemy #17 will drop the magnums
- Enemy #50 will drop the uzis
- Enemy #12 will drop two small medipacks
- Enemy #47 will drop puzzle 2
</details>

<table>
  <tr valign="top" align="left">
    <th>Parameter</th>
    <th>Type</th>
    <th>Description</th>
  </tr>
  <tr valign="top">
    <td>
      <code>enemy_num</code>
    </td>
    <td>Integer</td>
    <td>The index of the enemy in the level's item list.</td>
  </tr>
  <tr valign="top">
    <td>
      <code>object_ids</code>
    </td>
    <td>Integer / string array</td>
    <td>
      A list of item <em>types</em> to drop. These items will spawn dynamically
      and do not need to be added to the level file. Duplicate IDs are permitted
      in the same array.
    </td>
  </tr>
</table>

You can also toggle `convert_dropped_guns` in
[global properties](../GLOBAL_PROPERTIES.md#convert-dropped-guns). When `true`, if an enemy drops a gun
that Lara already has, it will be converted to the equivalent ammo. When
`false`, the gun will always be dropped.

### Enemy validity

All enemy types are permitted to carry and drop items. This includes regular
enemies as well as TR1 Atlantean pods (objects 163, 181), TR1 centaur
statues (object 161), and TR2 statues (objects 42, 44). For pods, the items will be allocated to the creature
within (obviously empty pods are excluded).

Items dropped by flying or swimming creatures will fall to the ground (TR1 only).

For clarity, following is a list of all enemy type IDs which you can
reference when building your game flow. The game flow will ignore drops for
non-enemy type objects, and a suitable warning message will be produced in the
log file.

<table>
  <tr><th colspan="2">TR1</th><th colspan="2">TR2</th></tr>
  <tr valign="top" align="left"><th>Object ID <th>Name</th><th>Object ID <th>Name</th></tr>
  <tr><td>7</td><td>Wolf</td><td>15</td><td>Dog</td></tr>
  <tr><td>8</td><td>Bear</td><td>16</td><td>Masked Goon 1</td></tr>
  <tr><td>9</td><td>Bat</td><td>17</td><td>Masked Goon 2</td></tr>
  <tr><td>10</td><td>Crocodile</td><td>18</td><td>Masked Goon 3</td></tr>
  <tr><td>11</td><td>Alligator</td><td>19</td><td>Knife Thrower</td></tr>
  <tr><td>12</td><td>Lion</td><td>20</td><td>Shotgun Goon</td></tr>
  <tr><td>13</td><td>Lioness</td><td>21</td><td>Rat</td></tr>
  <tr><td>14</td><td>Puma</td><td>22</td><td>Dragon Front</td></tr>
  <tr><td>15</td><td>Ape</td><td>25</td><td>Shark</td></tr>
  <tr><td>16</td><td>Rat</td><td>26</td><td>Eel</td></tr>
  <tr><td>17</td><td>Vole</td><td>27</td><td>Big Eel</td></tr>
  <tr><td>18</td><td>T-rex</td><td>28</td><td>Barracuda</td></tr>
  <tr><td>19</td><td>Raptor</td><td>29</td><td>Scuba Diver</td></tr>
  <tr><td>20</td><td>Flying mutant</td><td>30</td><td>Gunman Goon 1</td></tr>
  <tr><td>21</td><td>Grounded mutant (shooter)</td><td>31</td><td>Gunman Goon 2</td></tr>
  <tr><td>22</td><td>Grounded mutant (non-shooter)</td><td>32</td><td>Stick Wielding Goon 1</td></tr>
  <tr><td>23</td><td>Centaur</td><td>33</td><td>Stick Wielding Goon 2</td></tr>
  <tr><td>24</td><td>Mummy (Tomb of Qualopec)</td><td>34</td><td>Flamethrower Goon</td></tr>
  <tr><td>27</td><td>Larson</td><td>35</td><td>Jellyfish</td></tr>
  <tr><td>28</td><td>Pierre (not runaway)</td><td>36</td><td>Spider</td></tr>
  <tr><td>30</td><td>Skate kid</td><td>37</td><td>Giant Spider</td></tr>
  <tr><td>31</td><td>Cowboy</td><td>38</td><td>Crow</td></tr>
  <tr><td>32</td><td>Kold</td><td>39</td><td>Tiger</td></tr>
  <tr><td>33</td><td>Natla (items drop after second phase)</td><td>40</td><td>Marco Bartoli</td></tr>
  <tr><td>34</td><td>Torso</td><td>41</td><td>Xian Spearman</td></tr>
  <tr><td colspan="2" rowspan="11"></td><td>42</td><td>Xian Spearman Statue</td></tr>
  <tr><td>43</td><td>Xian Knight</td></tr>
  <tr><td>44</td><td>Xian Knight</td></tr>
  <tr><td>45</td><td>Yeti</td></tr>
  <tr><td>46</td><td>Bird Monster</td></tr>
  <tr><td>47</td><td>Eagle</td></tr>
  <tr><td>48</td><td>Mercenary 1</td></tr>
  <tr><td>49</td><td>Mercenary 2</td></tr>
  <tr><td>50</td><td>Mercenary 3</td></tr>
  <tr><td>52</td><td>Black Snowmobile Driver</td></tr>
  <tr><td>214</td><td>T-Rex</td></tr>
</table>

### Item validity

The following object types are capable of being carried and dropped. The
game flow will ignore anything that is not in this list, and a suitable warning
message will be produced in the log file.

<table>
  <tr><th colspan="2">TR1</th><th colspan="2">TR2</th></tr>
  <tr valign="top" align="left"><th>Object ID</th><th>Name</th><th>Object ID</th><th>Name</th></tr>
  <tr><td>84</td><td>Pistols</td><td>135</td><td>Pistols</td></tr>
  <tr><td>85</td><td>Shotgun</td><td>136</td><td>Shotgun</td></tr>
  <tr><td>86</td><td>Magnums</td><td>137</td><td>Automatic Pistols</td></tr>
  <tr><td>87</td><td>Uzis</td><td>138</td><td>Uzis</td></tr>
  <tr><td>89</td><td>Shotgun ammo</td><td>139</td><td>Harpoon Gun</td></tr>
  <tr><td>90</td><td>Magnum ammo</td><td>140</td><td>M16</td></tr>
  <tr><td>91</td><td>Uzi ammo</td><td>141</td><td>Grenade Launcher</td></tr>
  <tr><td>93</td><td>Small medipack</td><td>142</td><td>Pistol Clips</td></tr>
  <tr><td>94</td><td>Large medipack</td><td>143</td><td>Shotgun Shells</td></tr>
  <tr><td>110</td><td>Puzzle1</td><td>144</td><td>Automatic Pistol Clips</td></tr>
  <tr><td>111</td><td>Puzzle2</td><td>145</td><td>Uzi Clips</td></tr>
  <tr><td>112</td><td>Puzzle3</td><td>146</td><td>Harpoons</td></tr>
  <tr><td>113</td><td>Puzzle4</td><td>147</td><td>M16 Clips</td></tr>
  <tr><td>126</td><td>Lead bar</td><td>148</td><td>Grenades</td></tr>
  <tr><td>129</td><td>Key1</td><td>149</td><td>Small Medipack</td></tr>
  <tr><td>130</td><td>Key2</td><td>150</td><td>Large Medipack</td></tr>
  <tr><td>131</td><td>Key3</td><td>152</td><td>Flare</td></tr>
  <tr><td>132</td><td>Key4</td><td>151</td><td>Flares Box</td></tr>
  <tr><td>141</td><td>Pickup1</td><td>174</td><td>Puzzle Item 1</td></tr>
  <tr><td>142</td><td>Pickup2</td><td>175</td><td>Puzzle Item 2</td></tr>
  <tr><td>144</td><td>Scion (à la Pierre)</td><td>176</td><td>Puzzle Item 3</td></tr>
  <tr><td rowspan="10" colspan="2"></td><td>177</td><td>Puzzle Item 4</td></tr>
  <tr><td>193</td><td>Key 1</td></tr>
  <tr><td>194</td><td>Key 2</td></tr>
  <tr><td>195</td><td>Key 3</td></tr>
  <tr><td>196</td><td>Key 4</td></tr>
  <tr><td>205</td><td>Pickup Item 1</td></tr>
  <tr><td>206</td><td>Pickup Item 2</td></tr>
  <tr><td>190</td><td>Secret 1</td></tr>
  <tr><td>191</td><td>Secret 2</td></tr>
  <tr><td>192</td><td>Secret 3</td></tr>
</table>
