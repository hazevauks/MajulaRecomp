# Button pictures in the Nintendo style

Dark Souls II was made for the Xbox 360 pad: its prompts show a green A, a red B, a blue X, a yellow Y, BACK, START,
LB/RB/LT/RT. On the Switch the port redraws those pictures in the Nintendo style, from the game's own art.

## Where the pictures are

Every button prompt (Confirm and Back at the bottom of the menus, the key guide when there is something to interact
with, the tutorial messages, the character creation, the title menus) is text: the game's messages
(`menu\Text\<language>\*.fmg`) use characters such as ⑲ (A), ⑳ (B), ① (Y), ② (X), ⑥ (BACK), ⑯ (START), ⑦ (RB),
⑧ (RT), ⑨ (LB), ⑩ (LT), ⑪ / ⑫ (the sticks pressed), ⑰ / ⑱ (the sticks moved), and the font maps them to pictures on
one 512x512 DXT5 page: `FeFont_Small_0000_x360.tpf` inside `font\<language>\FeFont_Small.fontbnd.dcx`. The page is
the same in the eight languages of the disc (the unused Portuguese folder has another one). The font is a loose file
that the game opens at start (`game:\font\English\FeFont_Small.fontbnd.dcx`), not an entry of the `GameData` archives.

| Character | Picture | Becomes |
|---|---|---|
| ① ② ③ ④, ⑲ ⑳ | Y X B A (spheres) | dark grey spheres; by position X Y A B and B A |
| ⑥ ⑯ | BACK, START (silver pills with arrows) | the same pills with − and + |
| ⑦ ⑧ ⑨ ⑩ | RB RT LB LT (shoulder plates) | R ZR L ZL on the same plates |
| ㌦ ㌧ | LB, RB as plain text (the equipment tabs) | L, R |
| ⑤ ⑭ ⑮ ㊤ ㊦ ㊧ ㊨, ⑪ ⑫ ⑰ ⑱ ㎏ ㎎ | the d-pad, the sticks | unchanged |

## How it is drawn (`app/src/ds2_button_icons.cpp`)

- **A/B/X/Y.** Each coloured sphere becomes the dark grey of a Switch controller's buttons. Its brightness is matched,
  rank for rank, to the green A's, so the four come out in the same tone whatever their colour was, and the gloss
  stays where it was; the white letters and their dark outline are the game's own. With the buttons by position (the
  SDK's default, `input_xbox_layout = true`: the bottom button, B, acts as A) the spheres also swap places, so each
  position shows the Switch's letter: the Xbox A becomes B, B becomes A, X becomes Y, Y becomes X. By letter only the
  colour changes.
- **BACK / START.** The arrow is removed from the pill (each row blends the pill's pixels on both sides) and a minus or
  a plus is drawn in the arrow's colour.
- **The shoulder plates.** The band of the old letters (rows 7 to 19) is repainted with the plate's face colour, and
  L, R, ZL or ZR is drawn with a real typeface in the game's letter style: white 10 pixels high (rows 8 to 17), a
  1-pixel dark outline, a soft shadow below, the stroke about 2 pixels wide. The letters are a signed distance field
  of the typeface at eight times the size (stb_truetype, the copy ImGui carries), so a regular weight is made as bold
  as the game's letters and the outline has an exact width. On the console the typeface is the Switch's own system
  font (the `pl` service); on the PC, Arial Bold.
- **The plain LB / RB.** Only the first letter is kept, centred.

Only the 4x4 blocks that changed are compressed again (BC3: the colour along the main axis, then least squares; the
alpha in the eight- or six-value mode, whichever is closer); the rest of the page keeps its original bytes, so nothing
else in the file moves. The DCX is written back with stored zlib blocks (no compression; the game's inflater reads
them like any other).

## Where the result goes

`cache/ds2_fonts/<language>/FeFont_Small.fontbnd.dcx` next to the program (on the Switch,
`sdmc:/switch/ds2-nx/cache/ds2_fonts/`), with a small `.stamp` that names the version of the drawing, the style and
the original file's size and hash: the next start uses the cache unless one of those changed. The folder is mounted as
a device of its own (`\Device\DS2Fonts`) and each language's font folder is a symbolic link to it (the file system resolves
a file's folder and then looks the name up there, so a link on the file alone would never be consulted). Nothing of
the game is shipped: the pictures are rebuilt from the user's copy, and a page that is not byte for byte the expected one is
left alone (the log says so).

## Settings

| Setting | Default | What it does |
|---|---|---|
| `ds2_button_icons` | `auto` | `nintendo`, `xbox`, or `auto` (Nintendo on the Switch, Xbox on the PC). Read at start |

The log line `[buttons] Nintendo button pictures (...)` says which languages were built or taken from the cache.

## Not covered

These are inside the `GameData` archives, which the game reads by offset, so they would need a hook on the game's
archive reader: the "Xbox 360 Controller" name in the options, the "PRESS START BUTTON" picture of the title screen,
and the Xbox Live messages (which do not show offline). Remapping single buttons (`input_p1_a = ...`) is not followed
by the pictures.
