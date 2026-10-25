# Investigation scripts of the menu bar's new tabs (docs/features.md, section 3)

Pure Python, read-only on the player's files; they write only into their working folder (`tex/`, next to them when
run from a scratch folder). They draw the two new medallions and the bar's texture page for the PC prototype
(`ds2_tabs_page_dir`, `app/src/ds2_tabs.cpp`); the shipped port will build the page in C++ at start, as
`app/src/ds2_button_icons.cpp` builds the button pictures.

- `tpf_png.py`: a one-texture 360 TPF (DXT5, tiled, 16-bit swapped) to PNG; `crop.py`: PNG load, crop and save.
- `gen_cup.py`: the Trophies medallion (a chalice) on the sword medallion's stone, from In-game_01's page
  (`tex/ingame01.png`); writes `tex/bar6.png`.
- `gen_port.py`: the same with the bonfire's coiled sword (the Port tab); writes `tex/bar6_port.png`.
- `patch_page.py`: In-game_01's page with the bar of seven medallions and the two moved icons, as the game's bytes
  (`tex/ingame01_orig.bin`, `tex/ingame01_patched.bin`).
