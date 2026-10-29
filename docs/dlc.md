# The DLCs (Crown of the Sunken, Old Iron and Ivory King): what it takes

Studied on 2026-10-29 with the user's three discs in `game isos/` (git-ignored): this port's Dark Souls II (2014) and
Scholar of the First Sin's two discs (2015). Nothing here is built yet; this is the plan.

## The discs

| | This port's disc | Scholar of the First Sin, disc 1 | Scholar, disc 2 |
|---|---|---|---|
| What it is | the game | the game | an installer (`default.xex`, title FFED2000, `media/ExpansionInstaller.xzp`) |
| `Default.xex` | 0.0.0.8, title 465307E4, entry `82993BB0`, image 23,855,104 bytes | **0.0.0.22, the same title 465307E4**, entry `829C3DE0`, image 24,051,712 bytes | - |
| Data | `GameData1/2` 2.84 + 2.46 GB, `regulation.bnd.dcx` 674,750 bytes | `GameData1/2` 2.72 + 2.34 GB, `regulation.bnd.dcx` 826,432 bytes and `regulation_v100.bnd.dcx` 674,750 (the 2014 one, same size) | `content/0000000000000000/FFED2000/FFFFFFFF/D4B9...` (1.23 GB) |

Disc 2's package (`tools/stfs_extract.py` lists and extracts it) is a PIRS package of title 465307E4, content type 2
(marketplace), "DARK SOULS II Compatibility Pack 4" ("a free compatibility pack... required for online play"):
`Data/catalog/DlcData010306_110.bdt` + `.bhd` (1.1 GB, the three DLCs' areas, `BHF4` index), the DLC maps' sound banks
(`frpg2_sm5035`-`5038`, `sm6014`-`6027`, `sm7021`, `sm7024` and their `xm`), and `Data/License/license32.bin` (25
bytes: the text "DARK SOULS II DLC Catalog"). That is how the DLCs were sold on the Xbox 360: the areas in free
compatibility packs, the purchase a licence.

## Why the 2014 executable cannot load them

Its strings name a `DLCThread_Xbox360` and nothing else of the DLCs. Scholar's executable has the whole path:
`dlc_root`, `dlc_temp`, `dlc_temp:\Data\Catalog\DlcData??????_???.bhd`, `dlc_temp:\Data\Catalog\%s.bdt`,
`dlc_temp:\Data\License\license??.bin`, and an installed copy (`install:/Install/DlcData.bdt` / `.bhd`). The 2014
game got that code from its title updates (Crown of the Sunken King came with patch 1.08), which this port does not
have; the purchase licences would be needed too. **The DLCs need Scholar of the First Sin's executable**, which has
them included (and shares the title id, so its saves, achievements and content folders are the same title's).

## How close Scholar's executable is to this port's

`tools/xex_func_map.py` (2026-10-29) maps this port's functions into another image by their normalised instructions
(registers and opcodes; immediates and branch displacements left out), from the generated code's function list and
`out/default_image.bin`; Scholar's image came from a plain codegen run in `out/sotfs/` (its manifest there; the run
stops at 50 unresolved calls, the same kind of discovery gaps `app/overrides.toml` fixed for this disc):

- 38,874 of the 68,268 functions of 6 instructions or more (**56.9 %, 72.7 % of the instructions**) are in Scholar's
  executable unchanged and once; the rest changed, appear more than once or moved inside other code. The code moved in
  blocks (displacements of +0x2FCA0 to +0x35290), so it is a rebuilt program, not a patched one.
- Every Direct3D function of the renderer's census maps, most of them by +0x301D0 (`CreateDevice` 829BA480 ->
  829EA648, `Swap` 829BB520 -> 829EB6E8, the draws 829C2E28 -> 829F2FF0, ...): the renderer's hooks move by table.
- The audio threads' hooks map; two hooks on the game's own code do not (`82271CC8` of the gameplay options,
  `82556A60` of the native layer) and would be found again by hand.
- `app/overrides.toml`'s ~1,090 addresses: those that are function starts move with the map; the rest come again from
  the discovery tools (`find_gaps.py`, `pointer_scan.py`, `gap_fixpoint.py`, `runtime_gap_resolver.py`) on Scholar's
  executable, as they did for this one.
- Global data addresses (the Direct3D device's `0x8343B708`, the `VdGlobalDevice` import slot) are not covered by
  the map; they are found again from the functions that use them.

## The plan

**Decided (the user, 2026-10-29):** after the optimisation rounds, the whole project moves to Scholar of the First
Sin's executable and data (no second build for the 2014 one).

1. `game_root` from Scholar's disc 1; the codegen on its `Default.xex` with overrides made for it (the gap tools,
   then the console's runtime gaps).
2. Every address of `app/src` moved with `out/sotfs/func_map.txt` (a script over the sources), the unmapped ones by
   hand; the shaders dumped again from its `shader/*.bnd` (the chain is automatic).
3. The DLC package extracted (`tools/stfs_extract.py`) into the content folder the SDK lists marketplace content from:
   `<content root>/0000000000000000/465307E4/00000002/<package>/` (`sdk/src/system/xam/content_manager.cpp`); then
   whatever the game asks of the licence calls (`XamContentGetLicenseMask` and the like) answered.
4. A new PGO profile, the features (the Dull Ember lots are Scholar's own: the fork's lot numbers come from the 2014
   regulation) and the trophies checked again.

Scholar of the First Sin on the Xbox 360 is the 2014 game with its title updates and the three DLCs (the user,
2026-10-29): the changed enemy placement and the rest of the "Scholar" changes are the next generation's (Xbox One, PS4,
the PC's DirectX 11 version). The disc agrees: the same title, an executable built from the same code, the 2014
regulation kept beside the new one. Moving to it keeps the game as it is, with the patches' balance and the DLCs.
