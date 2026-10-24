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

## 4. Later

- **Scholar of the First Sin (Xbox 360):** another executable (regenerate the code, census its Direct3D again, find
  this port's game-specific addresses again), two discs extracted into one `game_root`, DLC packages with
  `tools/stfs_extract.py`. The SDK, the renderer, the tools and the method carry over. After the vanilla port reaches
  its frame rate goal.
- **Local multiplayer (same Wi-Fi):** the game's online play needs Xbox Live's network layer (secure addresses,
  sessions) and FromSoftware's server protocol (summon signs, messages, matchmaking). Both would have to be replaced:
  direct sockets on the local network, and a small server of that protocol run by one of the consoles, as community
  private servers do on the PC. Months of work with real risk: long term.
