# Port features: plan (2026-10-24)

> **Out of the build since 2026-10-29** (the user): while the port moves to Scholar of the First Sin and the work is on
> performance, `src/ds2_features.cpp` (Early Dull Ember, Better fog walls) is not compiled (app/CMakeLists.txt). Its
> item lots are the 2014 regulation's and its hooks were moved by tools/xex_addr_remap.py untested: both are checked
> against Scholar's before it returns.

Features the user wants beyond the straight port, from a brainstorm (no code yet). Every change is made at run time
on the player's own files: nothing of the game is shipped modified.

## Order

1. **Investigate the game's menus**, starting with the options page of the system tab (the gear), to add two rows at
   its end: "Early Dull Ember" and "Better fog walls", on/off.
2. **The two gameplay changes** behind those rows (and behind `ds2.toml` keys, which stay as the fallback if the menu
   rows cannot be done).
3. **Trophies as a sixth tab** next to the gear, hybrid first (below), using what step 1 taught about the menus.
4. Later: Scholar of the First Sin (Xbox 360, 2015) and local multiplayer.

## 1. The options rows (system tab)

**Decided (2026-10-24): no menu rows.** The two gameplay changes are on by default and kept only in `ds2.toml`
(`ds2_early_dull_ember`, `ds2_fog_wall_iframes`; false gives the original game). Game Options is the game's again
(Voice chat and Cross-region play). The port's own things get a tab of their own (section 3). Before that, the rows
were built and tried on the PC in Game Options, in the places of the two online rows (commit 0d26b63): the notes
below keep what that taught about the menus.

### What the menu code does (static reading, 2026-10-24)

The system tab (gear) lists Game Options (2101030), Screen Options (2101031), Mouse / Keyboard Settings (PC
leftovers) and Quit Game. Each options page is a class of its own over one base class (vtables of 36 slots); slot 26
builds the page, slot 13 handles input, and the rows are "items" of the menu manager (`68(this)`), 44 bytes each,
whose widgets sit at `+22576` and after.

| Page | vtable | Build (26) | Input (13) | Items | Rows |
|---|---|---|---|---|---|
| Game Options | `82003B68` | `822A3E60` | `822A4720` | 59 to 73 | 15 |
| Screen Options | `82003C10` | `822A34A8` | `822A3128` | 75 to 78 | 4 |
| third page (tables empty on the 360) | `82003CB8` | `822A39A0` | `822A2DE8` | 80, 81 | 2 |

- **Layout:** both pages draw into the same list of the In-Game layout (`menu\02.febnd.dcx`,
  `l02_01_In-Game.flo`), which has exactly **15 row slots** (element ids 2010241, 2010240, 2010247, 2010254, 2010246,
  2010245, 2010244, 2010243, 2010242, 2010334 to 2010329, top to bottom). Game Options fills all 15, so two rows
  after its last one have no slot in the layout. Screen Options uses only the first 4: **slots 5 and 6 (2010246,
  2010245) are already there, placed and empty**.
- **Tables in the executable** (all indexed by row): Game Options' row names at `83413388`, its layout slots at
  `8341334C`, help at `83413498`; Screen Options' row names at `83413404` (Blood, Subtitles, HUD, Brightness), its
  slots at `834133F4`, help at `834133E4`, key guides at `834133C4` / `834133D4`. They are read past their end if
  the row count grows, so the functions that read them are wrapped for our rows instead of the tables extended.
- **Screen Options' values** are 4 bytes of the game's option object (`+16`: blood, subtitles, HUD, brightness),
  changed by `822A2918(page, row, delta)` (rows past 3 change nothing there) and written back by `8238ADB0`; the
  value's text comes from `822A22B8(page, row)`. Help and key guide: `822A2388`.
- **Texts:** every menu text goes `82272778(widget, category, id)` → `8264D2D8` → `8264CF80` → `8264CF08(messages,
  category, id)`, which returns the UTF-16 string (category 7 is `ingamemenu.fmg`; an unknown id gives an empty
  string). A hook on `8264CF08` answers our new ids (row names, help lines) with strings of our own in guest
  memory; the values reuse the game's "On" / "Off" (2603020 / 2603030).

**Plan from this (superseded on the PC, see above):** the two rows were to go at the end of **Screen Options**
(after Adjust Brightness, in the layout's slots 5 and 6). At the end of Game Options they would need new layout slots (the `.flo` changed in memory), which is much
more work for the same result. The hooks:

1. Build (`822A34A8`): run the game's, then add rows 5 and 6 as items 79 and 80 the same way it adds its own
   (`82414020` puts the item in the page's list, `82312AD8` gives the manager its widget), with their name and value
   texts.
2. Value change (`822A2918`) and help (`822A2388`): rows 0 to 3 go to the game's; rows 4 and 5 are ours (flip the
   setting, write `ds2.toml`, set the value text).
3. Message lookup (`8264CF08`): our ids.

To check on the PC (when builds are allowed): that up / down reaches the new rows (the base class walks the page's
item list), that item 80 is free while Screen Options is open (the third page also uses it, but it looks unused on
the 360), and how "Default" (the key guide's reset) treats our rows.

## 2. Gameplay

### Better fog walls (invulnerable while crossing)

- What: while the player crosses a fog wall ("Traverse the white light") enemies can hit them in vanilla. A hook on the
  crossing marks the player invulnerable for the length of its animation, as PC mods do.
- Live: yes. The hook reads the setting at every crossing.
- To investigate: the crossing's function (from the prompt's action), the character's invulnerability flag, the end of
  the animation (or a fixed time measured on the crossing).

### Early Dull Ember

**Decided (2026-10-24):** the Dull Ember goes in the iron chest by The Tower Apart bonfire in the Lost Bastille (lot
10165140, with its Antiquated Key and Covetous Silver Serpent Ring), near Scholar of the First Sin's place; the Iron
Keep's pickup gives a Titanite Slab. The Last Giant's lot stays as it is. Done in `app/src/ds2_features.cpp`.

The first plan, kept for its notes:

- What: the Last Giant (the first boss) drops the Dull Ember with its soul. The Dull Ember's original place gives a
  **Titanite Slab** instead (the user's choice: a valuable item, so the trip there is still worth it).
- How: the Last Giant's reward and the original pickup are item lots in the game's parameters
  (`regulation.bnd.dcx`). Their rows are changed in memory after the game loads them (or the reward function adds the
  item when the boss dies); no file is changed.
- Players past the Last Giant: when a save loads with the setting on and the boss already defeated, the Dull Ember is
  given once, automatically. "Once" needs a marker of our own per save (a small file next to the saves, keyed by the
  save slot and character), so it is never given twice and nothing is written into the game's save format. If the
  player already has the Dull Ember, or has given it to the blacksmith (McDuff), nothing is given.
- Live: the drop, yes (the setting is read when the boss dies). The swap of the original pickup applies from the next
  area load. The automatic gift happens at the next save load (or at once when the row is turned on, to investigate).
- To investigate: the Last Giant's item lot and its "defeated" event flag, the Dull Ember's and the Titanite Slab's
  item ids and the original pickup's lot, the function that gives an item to the player, how to read the inventory
  and McDuff's state.

### What the game data says (static reading, 2026-10-24)

The global parameters are in `regulation.bnd.dcx` (a loose file of the game root, not in `GameData`): a BND4 of 252
files, the item lots among them. A `.param` here is big-endian: row count (u16) at 0x0A, the type name at 0x0C,
the data's start at 0x30, the row table at 0x40 (id, data offset, name offset; 12 bytes a row).

- **Items** (`ItemName.fmg`): Dull Ember 50990000, Titanite Slab 60990000, Soul of the Last Giant 64010000, Soldier
  Key 50600000.
- **Item lots** (`ItemLotParam2_*`, 124 bytes a row): 4 bytes of header, then for ten slots the amount (u8 x10),
  reinforcement (x10), infusion (x10), a per-slot byte (x10, 1 on the slots in use), the item ids (s32 x10, at 44,
  unused slots hold 10) and the chances (f32 x10, at 84).
  - **The Last Giant's reward**: `ItemLotParam2_Other` row **309600**: Soul of the Last Giant and the Soldier Key,
    both at 1.0. `BossBattleParam` row 1010010 ties the boss (309600) to that lot, with the flags 110000081 and
    100971 and the boss index 31 (which flag says "defeated" is for the PC test).
  - **The Dull Ember's pickup**: `ItemLotParam2_Other` row **10196160**, map m10_19, the **Iron Keep**: the Dull
    Ember alone at 1.0.
- **Where the game keeps them:** `823A4860` loads the item params by name from the regulation (`82472150`) into a
  manager: +4 ItemParam, +12 ItemTypeParam, +20 ItemUsageParam, +28 ItemLotParam2_Chr, **+36 ItemLotParam2_Other**,
  +44 ItemLotParam2_SvrEvent, then the Estus flask, menu icon and shop params.

**Plan from this:** the two rows are changed in memory while the setting is on, and put back when it is turned off:
row 309600 gets the Dull Ember in its third slot (amount 1, chance 1.0, slot byte 1), row 10196160 gets the Titanite
Slab instead of the Dull Ember. A lot is read when it is awarded, so both follow the setting live, the Iron Keep
pickup included (to confirm: that the map does not copy its item at load). For the gift to saves past the giant,
the simplest award is the game's own: the original row 10196160 (the Dull Ember alone) given through the game's
lot-award function, found on the PC by tracing an item pickup.

**Scholar of the First Sin's place, on the 360's map (investigation, 2026-10-24):** in SotFS the Dull Ember is in
the right one of two iron chests next to The Tower Apart bonfire, where the Pursuer's eagle drops the player in the
Lost Bastille (m10_16). On the 360 the map's pickups are parts of `map\m10_16_00_00\m10_16_00_00.msb`: each part
named `oXX_YYYY_ZZZZ` holds its lot id 0x2C bytes after its name and its position (x, y, z, floats) at 0x48. The
models: `o00_0200` and `o00_0210` chests, `o00_0240` glowing items on the ground, `o00_0256` others. The lots follow
the map: 10165xxx the chests, 10166xxx the ground items. Near the eagle's arrival (the guides put the Antiquated Key
near the Tower Apart bonfire), a group of chests at x -82..-78, z 586..593: `o00_0210_0012` (lot 10165140, Antiquated
Key and Covetous Silver Serpent Ring, the only iron one there), `o00_0200_0011` (Target Shield), `_0013` (Large
Titanite Shard), `_0014` (Titanite Shard x5), `_0015` (Iron Arrow x10). The bonfire's own position is not in the MSB
under its id (16660), so the group is placed by the guides, to be checked in the game.
- The Dull Ember there, as data (no new object): its lot added to one of those lots, live and kept by the game's own
  "taken" state. In the iron chest (SotFS's kind of place), or in a ground item for a glowing pickup (the nearest are
  higher or farther: Soul of a Nameless Soldier at (-87, 15, 569), Scimitar at (-45, 2, 581)).
- A glowing item at a free spot instead: the drop function below at a position near the bonfire, when the map loads
  with the option on and the player has no Dull Ember. The game would not remember it (a drop is not a map pickup),
  so our own per-save marker decides; more work, and the player's position or the map's loading is needed.

**The gift to saves past the giant (to do, static leads):**
- "Past the giant" without an event flag: the Soldier Key (50600000) comes only from the Last Giant's lot and is
  never used up, so a save holding it has beaten him. Holding the Dull Ember is checked the same way; a player who
  already gave it to McDuff would get a second one (harmless), unless McDuff's flag is found.
- The gift as the game makes a drop: `825E8AA8` (drop manager, lot id, f1..f3, v1 = position, r8) puts a lot on
  the ground as a glowing pickup; its wrappers are `825E8D20` / `825E8D40`, and the drop manager is
  `[[0x834797B8] + 80]` (as `825BF240` takes it). The Dull Ember alone is the original row 10196160. Missing: the
  player's position (the player character from the same globals), on the PC.
- The event flags: `825B4B58` loads the event params (`BossBattleParam`, `EventFlagListParam`, ...) into the event
  manager; the flag reader itself is not found yet.

### Fog walls (static reading)

- A fog wall is a "white door": `MapObjectWhiteDoorParam` (117 rows of 32 bytes): flags, a crossing distance (1.5),
  an action pair (280 to 289; 280, 281, 285 and 286 are most of them), two message ids (400 / 410 and per-map ones)
  and an event flag (for example 102000160 in Things Betwixt).
- The crossing itself is an animation of the player (`timeact\chr\c000100_pl.tae`, TAE version 0x1000C,
  big-endian, 1097 animations). The action ids do not name the animation directly.
- Two ways to make the crossing invulnerable: a code hook (while the player is in the crossing, the character's
  invulnerability flag stays set) or a data patch (an invulnerability event added in memory to the crossing's
  animations, as a roll has). Both need, from the PC: the crossing's animation id and the player's state while it
  plays (a crossing done with the live input file, logging the player's animation and flags each frame).

## 3. Trophies as a sixth tab

The game's menu is a bar of five tabs (L1 / R1: equipment, items, status, the fourth, the system gear). The trophy
screen (`docs/trophies.md`, today an ImGui window on L + Minus) becomes a sixth tab.

- **Hybrid (first):** the tab is real in the bar (its icon, L1 / R1 reach it) and the game's menu frame stays; only
  the content area is drawn by us, with the game's own textures (menu panels, frames, title bars, slot frames, read
  from the player's `GameData` archives: `BHF4` headers, not encrypted; TPF pages as the button icons already read)
  and the game's own font (FeFont, whose pages we already read).
- **Full (later, if the reverse engineering allows):** the content drawn by the game's own list widgets, the trophy
  texts answered through the message hook.
- To investigate: where the five tabs are listed and switched, the tab icons' texture page, how a tab's content is
  drawn and given the input.

### What the menu code does (static reading, 2026-10-24)

- **The bar** is one page of the menu framework, built by `822B0B98` (slot 8 of the vtable at `82003D50`) on the
  In-Game layout (`l02_01_In-Game.flo`, root node 2010025). Its items:

  | Item | Tab | Icon node | x in the bar | Dropdown |
  |---|---|---|---|---|
  | 1 | Equipment | 2010018 | -4.6 | none, opens its screen |
  | 2 | Inventory | 2010019 | 49.4 | none, opens its screen |
  | 3 | Status | 2010020 | 103.4 | node 2010007, items 4-5 (Player Status, Player Info) |
  | 12 | Message | 2010022 | 157.4 | node 2010008, items 13 onwards |
  | 6 | System (gear) | 2010021 | 211.6 | node 2010319, items 7-11 (Game Options, Screen Options, Mouse, Keyboard, Quit) |

  The icons are 54 units apart, the gear is the rightmost, and **the layout has exactly five icon nodes** (no sixth
  to fill, unlike the options rows). The tab labels (2101000 to 2101006, the L / R glyphs at 2101005 / 2101006) are
  set by `822B1A68`.
- **Screens:** the menu manager holds 27 screen objects (pointers at `+22328` to `+22436`); each is a page class
  like Game Options. `822B36A8` updates them, `822B2578` closes the whole menu.
- **Input:** `82312C38` turns the pad into the menu's bits (pressed / repeat / held at `+12` / `+16` / `+20` of the
  input block), from the table at `82008318`: 0x1 up, 0x2 down, 0x4 left, 0x8 right; the face and shoulder buttons
  follow. The pages move their cursor through their registered items, so a new item needs a node in the layout.

**Plan from this (hybrid):** the sixth icon is drawn by us at the next step of the bar (x 265.6, right of the gear),
in the game's style, with a texture from the game's menu pages; the game's layout is not changed.

**The icon:** the bar is one picture on `menu\40.febnd.dcx` → `In-game_01.tpf` (1024x1024 DXT5, x 8 to 290,
y 770 to 842): five octagonal stone medallions on a thorned chain, 54 pixels apart (the layout's units are the
page's pixels), the gear's centre at (257, 811). The sixth is made at run time from the player's own page: the
chain and the gear's medallion copied one step to the right, the gear replaced by stone, and a trophy in relief drawn
on it: a chalice with two handles (bowl, knotted stem, foot), lit like the game's reliefs (from the top left), its
grain the high frequencies of the head medallion's stone so the material matches. The inside of the medallion is
the sword medallion's own stone with the sword painted out (the guard's rows blended from above and below, then the
blade's columns from left and right), and the chalice takes the sword's tones (about 130 facing, 210 on the lit
edges, 80 in shade) and its shadow, cast 1.6 pixels down and to the right. A sphere wrapped in
flames (the PlayStation trophy's silhouette) was tried first and read worse at the icon's size (about 40 pixels).
The draft generator is a script of the investigation (pure Python), to become C++ next to `ds2_button_icons.cpp`,
which already decodes and re-encodes the game's DXT5 pages.

**The selected tab (seen on the PC):** the game draws no other picture for it: it adds a colour to the same medallion.
Fitted on captures (selected against plain, per channel, the gain stays near 1): with the cursor on the tab
(+116, +63, +8), a gold; with the tab's page open (+75, +17, +8), a red. The chalice takes the same two tints.

**The content (a mock-up made only of the game's pieces, on the player's files):**

| Piece | Where |
|---|---|
| The window: leather panel, border, blue title bar | `In-game_01.tpf` (40.febnd), x 15..895, y 6..549 |
| A row: leather strip (three-slice, the ends kept) | `ItemSellect_02.tpf` (99.febnd), x 8..500, y 392..440 |
| The selected row: orange glow bar | `waku_02.tpf` (99.febnd), x 4..784, y 448..512 |
| Progress: blue track, orange fill | `waku_03.tpf` (99.febnd), x 10..220 / 231..427, y 212..246 |
| Divider with its ornament | `waku_03.tpf`, x 0..462, y 8..28 |
| Icon tile: grey, orange on the selected row (the item screens' tiles) | `IconBase.tpf` (99.febnd), x 4..70 / 108..175, y 10..85 |
| Key guide (D-pad: select, B: back) | the port's FeFont, its button pictures already in the Nintendo style |
| The trophies' icons, names, descriptions, gamerscore | the executable's XDBF (38 achievements, 64x64 PNG icons) |
| The text | the game's FeFont (below) |

- **FeFont** (`font\<language>\FeFont_Small.fontbnd.dcx`, loose files): a BND4 with `FeFont_Small.ccm` and its pages
  (`_0000_x360.tpf`, the 512x512 DXT5 of the button pictures; `_0001` to `_0007`, 256x256 DXT3 with the letters).
  The `.ccm` (big-endian): version 0x20000, file size, height (28) at 0x08, page width and height at 0x0A / 0x0C,
  region count at 0x0E, glyph count (u32) at 0x10, the regions' offset at 0x14 (x1, y1, x2, y2 as s16) and the
  glyphs' at 0x18, 24 bytes each: code (u32), region offset (u32), page (u16), space before, width and advance
  (s16). English has 768 glyphs.
- **How it is drawn:** through the overlay the SDK's ImGui already uses (`renut_engine/gpu/overlay`): it takes
  RGBA textures (the pages decoded once at start) and draws them on the presenting thread over the game's frame, so
  the renderer needs nothing new. The FeFont becomes an ImGui font (each glyph a custom rect of the atlas, its
  advance and offset from the `.ccm`), so the text is the game's.
- **To check on the console:** that an ImGui window alive while the menu is open costs no frames with Renut's
  overlay (the old presenter painted from the UI thread then, the slow path of `docs/trophies.md`), and where the
  panel sits against the game's own screens (the layout's units to the screen's pixels).

1. Cursor on the gear with its dropdown closed, right pressed: the trophy tab becomes active (our state). From then
   on the menu's input bits are cleared after `82312C38` (the game's pages see nothing), the gear's highlight is
   replaced by ours, and the game's dropdown stays closed.
2. Our panel is drawn in the content area with the game's panel and frame textures and its font (FeFont), the list
   as the ImGui window shows it today (name, description, unlocked or not, progress).
3. Left or B gives the input back with the cursor on the gear; Start closes the menu as usual (`822B2578`).

To check on the PC: the bar's current item (to know the gear is selected and its dropdown closed), where the bar is
on screen (the layout's units to pixels), and which texture page has the tab icons' frame and highlight. A real
sixth node in the layout (the `.flo` rewritten in memory when the game reads it) stays the "full" option: the bar's
container would need its child list and offsets rebuilt.

### A tab of the port's own (investigation, 2026-10-24)

**Decided:** two new tabs, Trophies (the chalice) and Port (a bonfire's coiled sword), left of the gear: the bar
reads Equipment, Inventory, Status, Message, Trophies, Port, System. The gear's medallion, its dropdown and the RB
label move two steps right (108 units); our two medallions are drawn in the gap with the chain's pieces of their
slices, so the bar stays one chain.

The user's idea: the two options leave Game Options for a tab of things exclusive to the port. The hybrid tab above
can carry it: the sixth medallion opens a dropdown of our own, drawn like the game's (Status, Message and System
have one: an icon and a name per entry, the icon orange when selected, from `In-game_01.tpf`'s grey and orange icon
rows), with "Trophies" and "Port options"; each opens a page drawn by us in the game's window (the mock-up's
pieces), the options as rows with the game's value box and arrows, On / Off. Our input handling already takes the
menu while our tab is active, so the rows need no game page; Voice chat and Cross-region play go back to the game.
Missing for it: two dropdown icons in the game's style (a small chalice, and one for the options), and the
dropdown's position and spacing (from the layout, or measured on the PC).

### The bar in the game's memory (PC, 2026-10-24)

- **The layout stays loaded as it was read.** `l02_01_In-Game.flo` is kept in memory with its offsets made pointers.
  A layout element (a node object, 0x80 bytes: vtable `82106C98`, parent and siblings at +0x10..+0x18, an identity
  matrix at +0x40) points at its node record in the file at +0x2c. The record's +4 points at its property record,
  whose first two floats are x and y.
- **Moving an element.** Change that x before or while the menu shows: the gear's tab node (2010021, its highlight
  and its dropdown's chevron), its dropdown (2010319) and the RB label (2010182) moved 108 units right at once, live.
- **The bar is one sprite.** The chain and its five stone medallions are not the tab nodes: they are one sprite of node
  599 (sprite record at file 0x6c40, texture In-game_01, its texture rectangle at file 0x8c50: (1.1, 779.2) to
  (337.6, 850.9)), with the two arrows (0x6c70, the right one mirrored at x 1305.45; 0x6ca0). A selected tab is lit
  by a separate sprite (the orange square at 475..553, 772..852), so nothing else reads the bar's rectangle.
- **The file's tables, as far as needed:** the header's offsets (0x80: per-node lists of sprite records, 0x30 bytes
  each: x, y, scale x, scale y, colour, texture entry, texture rectangle pointer; the rectangles, 16 bytes each, from
  0x6d00; nodes, 0x20 bytes each, from 0xfca0; textures at 0x39d00: name, index and declared size; names at 0x3a420).
  In-game_02 is declared 1024x32, so a sprite cannot use its free rows.
- **Seven medallions drawn by the game:** the bar's rectangle grows by 108 (x2 445.6), the right arrow moves 108, and
  In-game_01's page gets the new strip in x 230..446 (the chalice, the coiled sword, then the gear and the end). The
  two icons that sat there (x 336..474, y 761..853, records 0x7cf0 / 0x7d00: an axe and a shield) move to the free
  (672, 928), only their texture rectangles change. The page is replaced in physical memory, the store cards marked;
  the renderer checks a tiled texture by its ends and eight 64-byte probes, so the replacement must land before its
  first upload (done when the bar is built; to check that nothing draws In-game_01 earlier).
- **The prototype** (`app/src/ds2_tabs.cpp`, PC only, `ds2_tabs_page_dir`): the new page comes from a file made by an
  investigation script (`tools/features/`); the shipped version builds it at start from the player's own page, as
  the button icons do.

**Seen on the PC (prototype, 2026-10-24):** the game draws the bar of seven medallions itself, at every opening of
the menu (the layout is read again each time and patched through `[layout+0x14]`, the page replaced once), with LB,
the arrows and RB in place. Found on the way:
- LB / RB go through the five tabs by their order (Equipment ... System, then back), not by position: the gear's
  tab node moved 108 units is still reached. But it must move after the build (moved before, RB never reaches it:
  the build reads the positions).
- The gear's dropdown moved 108 units clips its texts ("Game Opt.", "Screen Op"): a clip box placed elsewhere.
- The bar page's first 0x400 bytes do not change with RB: the cursor is kept elsewhere (the menu manager).
- Still to do: the cursor stopping on the two new medallions (LB / RB in the game's order, and the highlight: the
  selected tab's orange medallion and name are its node's sprites, so ours are drawn by us), the dropdown's clip,
  and the two pages.

## 4. Later

- **Scholar of the First Sin (Xbox 360):** another executable (regenerate the code, census its Direct3D again, find
  this port's game-specific addresses again), two discs extracted into one `game_root`, DLC packages with
  `tools/stfs_extract.py`. The SDK, the renderer, the tools and the method carry over. After the vanilla port reaches
  its frame rate goal.
- **Local multiplayer (same Wi-Fi):** the game's online play needs Xbox Live's network layer (secure addresses,
  sessions) and FromSoftware's server protocol (summon signs, messages, matchmaking). Both would have to be replaced:
  direct sockets on the local network, and a small server of that protocol run by one of the consoles, as community
  private servers do on the PC. Months of work with real risk: long term.
