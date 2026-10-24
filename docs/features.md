# Port features: plan (2026-10-24)

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

The options page is a list of rows whose value changes with left / right. Two rows at its end, after the game's own:

| Row | Key in `ds2.toml` | Default |
|---|---|---|
| Early Dull Ember | `ds2_early_dull_ember` | off |
| Better fog walls | `ds2_fog_wall_iframes` | off |

- The values are kept in `ds2.toml` (the menu writes it), not in the game's own option data: a change to the save or
  profile format could damage it.
- To investigate: where the page's row list is built and how a row's value is drawn and changed; the message lookup
  (the row names are not in the game's `.fmg` files: new message ids answered by a hook); how the page is drawn (the
  renderer's draw log, `banjo_renut_diag_draw_log_s`, and a trace of the menu's functions).
- Both settings take effect live (below), so the rows need no restart.

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

**Plan from this:** the two rows go at the end of **Screen Options** (after Adjust Brightness, in the layout's slots
5 and 6). At the end of Game Options they would need new layout slots (the `.flo` changed in memory), which is much
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
on it (a sphere held by flame-like blades on a stem and base, after the PlayStation trophy's silhouette, without
its symbols), lit like the game's reliefs (from the top left, light grey stone with grain and a dark rim). The
draft generator is a script of the investigation (pure Python), to become C++ next to `ds2_button_icons.cpp`, which
already decodes and re-encodes the game's DXT5 pages.

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

## 4. Later

- **Scholar of the First Sin (Xbox 360):** another executable (regenerate the code, census its Direct3D again, find
  this port's game-specific addresses again), two discs extracted into one `game_root`, DLC packages with
  `tools/stfs_extract.py`. The SDK, the renderer, the tools and the method carry over. After the vanilla port reaches
  its frame rate goal.
- **Local multiplayer (same Wi-Fi):** the game's online play needs Xbox Live's network layer (secure addresses,
  sessions) and FromSoftware's server protocol (summon signs, messages, matchmaking). Both would have to be replaced:
  direct sockets on the local network, and a small server of that protocol run by one of the consoles, as community
  private servers do on the PC. Months of work with real risk: long term.
