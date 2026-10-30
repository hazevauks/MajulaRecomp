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

**Decided (the user, 2026-10-29):** the whole project moves to Scholar of the First Sin's executable and data (no
second build for the 2014 one). First planned for after the optimisation rounds, then the same day moved forward: the
migration comes first and the optimisation goes on with the new version. The 2014 port's last commit is tagged
`ds2-2014-final` (the 33rd build); its generated code and image are kept in `out/gen_2014/` (the remap reads them) and
its game data in `assets/game_root_2014/`.

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

## The migration, step by step (started 2026-10-29)

1. `assets/game_root` is Scholar's disc 1 (`tools/extract_iso.py`), the 2014 one moved to `assets/game_root_2014`.
2. `tools/xex_func_map.py` got a neighbour pass: a function the scan left out (short, ambiguous) is tried at the
   displacement of the mapped functions around it. 76,676 of the 82,889 functions map (92.5 %; was 38,874).
3. `tools/xex_addr_remap.py` moved every address of `app/` and `tools/` (`--write`; its report in
   `out/sotfs/remap_applied.txt`): code addresses by the function that holds them (at least four normalised words
   checked at the new place; the render state's tiny entries by their exact words near their neighbours), data
   addresses by the code that loads them (`lis`/D-form pairs) or by the code pointers stored around them. Tokens on a
   comment line after "N&B", "SA2" and the like are other games' addresses and stay. Left as they were: the
   addresses in `.pdata` and the other games' data addresses that banjo-nx's and sa2-nx's code kept (they named
   nothing in the 2014 executable either), and the comments' evidence addresses the tool could not place.
4. `app/overrides.toml`: 29 entries whose code is not in Scholar's executable and 3 that landed on an address already
   listed (vtable thunks that differ only by their offset) are commented out; the gap tools find what is missing.
5. 253 of the 294 hooks of `app/src` name an address that was no function of the 2014 code (banjo-nx's census of
   Nuts & Bolts's Direct3D, never matched in Dark Souls II): dead there. After Scholar's codegen every hook is
   checked again, so that none of them becomes live on a Scholar function it was never meant for.
6. Scholar's codegen (optimised flavour) with the moved overrides: 5 unresolved calls (50 without them), fixed in
   `app/overrides.toml` (three more frameless leaves bounded by hand, two 2-instruction thunks); 83,806 functions.
   `app/indirect_promotions.txt`: 61 of its 460 lines named a function, a target or a site Scholar's code does not
   have and are dropped (the guards make the rest safe; a census of Scholar on the PC replaces the list).
7. The hooks against Scholar's functions (`tools/hook_liveness.sh`): one dead 2014 hook would have become live on a
   Scholar function, and it is one of a whole class: `gpu/d3d/hooks/state.cpp`'s 255 hooks are N&B's Direct3D census,
   none of them Dark Souls II's Direct3D, and the 8 of them that were live in the 2014 build had replaced ordinary
   game functions (a constructor among them). Out of the build (`DS2_NB_STATE_HOOKS`, 0).
8. The game shaders: Scholar's disc has the same 1,481 shaders, byte for byte (only the 72 in `Default.xex` moved), so
   the deko3d package, the PC shader cache and the players' `deko3d_shaders.bin` / `pipeline_list.bin` stay valid.
9. The gameplay changes (`src/ds2_features.cpp`) are out of the build meanwhile (docs/features.md).
10. The first console boots (2026-10-30) stopped right after the main thread started, "Call to invalid or unregistered
    function at guest address 0x827AB150": a 3-instruction getter only a data pointer reaches, in a stretch of code
    Scholar laid out anew, so the remap had no neighbours to place its 2014 override with. `tools/pointer_fixpoint.py`
    over Scholar's image declared 26 such entries (vtable thunks, callbacks); it now reads the code range of newer
    generated trees, rejects inline jump tables (four of its candidates) and keeps the codegen flavour on re-runs.
11. With them the game reached its title screen and stopped there: "Unable to start DARK SOULS II because the
    Compatibility Pack is missing" (it lists marketplace content of the user and of xuid 0, and found none).
    `tools/install_dlc.py` installs disc 2's package the way the SDK's `ContentManager::InstallContent` does (its files
    in `<content root>/0000000000000000/465307E4/00000002/<package>/`, its `XCONTENT_AGGREGATE_DATA` in
    `.../Headers/00000002/<package>.header`) into `assets/dlc_content`; `tools/win/package_switch.ps1 -IncludeGame`
    copies that into the SD folder's `ds2/` (the console's content root, beside the saves). The package's only licence
    entry is for every user with no bits, so the header carries no licence mask, as a real install would.
