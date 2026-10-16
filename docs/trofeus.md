# Trophies (branch `trofeus`)

A trophy system for the console, built on what the SDK already had: the game's own achievement list (name,
description, icon and gamerscore, read from the executable's XDBF) and the unlock record on the SD card
(`switch/ds2-nx/ds2/achievements/465307E4.toml`). The code is in `app/src/ds2_trophies.cpp`.

## What there is

- **Trophy screen**: **L + Minus** opens and closes it (L + R + Left, the SDK's shortcut, works too; F7 on the PC).
  It shows the progress (trophies and gamerscore), the platinum card and the list: unlocked trophies first, most
  recent on top, with their date; locked ones in grey with the game's hint. The D-pad browses and scrolls; **B**
  closes it, and so does **Minus** on its own. While it is open the game does not receive the controller (the SDK's
  rule for its menus).
- **Unlock notice**: a card in the lower right corner for 5 s (8 s for the platinum). Its window closes itself when it
  has nothing left to show: with any ImGui window alive the SDK's presenter paints from the UI thread, the slow path
  that took the twentieth build from ~20 to ~12 frames a second after an achievement, so the notice must not stay.
- **Our own platinum**, "Bearer of the Curse": it unlocks by itself when every trophy of the game is unlocked (the
  1000G). Its emblem is drawn in code (no third-party art) and its notice is bigger. It is kept in the same unlock
  file, with the id `0x504C4154` ("PLAT"). A profile that already had everything gets it at the first check.
- **Language**: the screen's own words follow the language the game runs in (`user_language`, the same setting that
  picks the achievement names from the XDBF): English, Japanese, German, French, Spanish, Italian, Polish or Russian,
  English for any other. The dates follow the language's usual format.
- **Font**: the Switch's own system font (the `pl` service; nothing is shipped); Segoe UI on the PC. The atlas gets
  the glyphs of the game's language (Latin with its extended block, Cyrillic, or ImGui's common Japanese set).

## Settings (`ds2.toml`)

| key | default | |
|---|---|---|
| `ds2_trophy_notices` | `true` | the unlock notice |
| `ds2_trophy_platinum` | `true` | the platinum |
| `ds2_trophy_test_at` | `0` | test: at that presented frame the notices show (the platinum and the first trophy, nothing is unlocked); 600 frames later the screen opens and 900 frames after that it closes |

## Checked

- On the PC (`ds2_090`, `ds2_091`): the font loads, and the notices and the screen open and close through the test,
  with no error. The picture cannot be seen on the PC (the software driver does not show the window), so the look is
  checked on the console.
- First console test: the screen opened with L + Minus but L + Minus did not close it. The screen's own Minus check
  closed it and the shortcut opened it again in the same instant; Minus now closes it only without L.
