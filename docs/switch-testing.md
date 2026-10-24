# Testing Dark Souls II (MajulaRecomp) on the Switch

The builds below are newest first; each section says what to copy, what changed and what to test. The oldest one, at
the end, is the **first build for the console**: at that point the game had reached the first map with the right
picture on the PC, only through software Vulkan, and nothing had been tested on the Switch yet.

## The measuring points (from 2026-10-22)

The user's four points, each one a place the game itself puts the player in, with the same position and view every
time (the old three were spots where the player stopped by hand, and small differences between sessions could give a
false reading):

1. **The start of the game:** right after the opening cutscene, where a new game puts the player.
2. **The Fire Keepers' house:** where the game puts the player after creating the character.
3. **Heide's Tower of Flame:** travel there from Majula, go to the first bonfire and warp to that same bonfire; when
   the warp ends the player always stands in the same place.
4. **The Forest of Fallen Giants:** back through Majula to the forest, and the same warp to its first bonfire.

Stay about 30 s at each, at 540p (720p only once deko3d frees enough GPU). The goal: 30 frames a second without drops
at stock clocks.

## Twenty-ninth build (2026-10-24): the scene's pixel shaders three times smaller

Copy over: `ds2-dk.nro`. Keep `configs/ds2_profile.toml` as `ds2.toml` for this test (the GPU profile again). Expect
shader compiling stutters at first, more than usual: nearly every pipeline of the scene is new (once, then kept).

What the GPU profile of the twenty-eighth build showed (four points, 720p handheld): the GPU at 37-75 ms a frame, the
two costliest pixel shaders of the scene 12 ms together, the resolves 4 ms and the depth resolves 3 ms. The texture
fetch adjustments added for Dark Souls II's HDR copy (the 7e3 codes and the exponent adjust, twenty-sixth build) sat
in every texture fetch of every pixel shader, and made the scene's shaders 2.3-3.2 times larger on the console's
compiler, while only the few draws of the HDR chain need them. Now only the pipelines of those draws keep that code.

**Test:** the four measuring points, ~30 s each, noting the frame rate at each (after the first stutters settle).
Then the `logs/` folder (into `switch-logs`) and the normal `ds2.toml` back in place.

## Twenty-eighth build (2026-10-24): the effects' draw calls

Copy over: `ds2-dk.nro`. Back to the normal `ds2.toml` (the A/B files are no longer needed).

What the twenty-seventh build's test showed: none of the three configurations brought the bonfire flame back, and
the renderer dropped no draw. Also missing: the mist around the bonfire when sitting down and the water splashes.
Every one of them is a particle effect, so the cause was common to all: Dark Souls II draws its effects through two
Direct3D functions Sonic Adventure 2 never had (`BeginIndexedVertices` and `DrawIndexedVerticesUP`), and the new
renderer did not take them over. Their draws went into the Xbox 360's command ring, which nothing reads any more:
lost without a trace. Both are now the renderer's (the PC had the same gap).

**Test:** load the bonfire save: is the flame there, and the mist when sitting? Then anywhere with effects (item
glows, the fog wall, water splashes, the waterfall at the start). A photo of anything still missing or drawn wrong.
Then the `logs/` folder (into `switch-logs`).

## Twenty-seventh build (2026-10-24): finding the missing effects

Copy over: `ds2-dk.nro` and the `configs/` folder (two new files in it).

What the twenty-sixth build's test showed: the game draws completely, and the four measuring points ran at about
19.5 / 16.9 / 24.7 / 18.5 fps at 720p handheld (the Vulkan build at 540p: 19.6 / 16.4 / 24.3 / 18.5), with the GPU at
99 % (38-59 ms a frame) and the render thread at 10-18 ms (the old renderer's was about 50 ms). Missing: the bonfire
flames (their light on the ground is there), the fog walls' movement (a still cloud texture), the items' glows on the
ground and the waterfall at the start. On the PC the same renderer draws the waterfall, and it drops no draw there.

What changed:

- The log counts every draw by primitive type and every draw the renderer gives up on, by reason.
- A fix for point-sprite and rectangle draws (the sprite coordinate reached the pixel shader in the wrong slot since
  the Dark Souls II interpolator was added); the scene at the start draws none, so it may not be what is missing.
- Two A/B configurations: `ds2_ab_shaders_off.toml` (the deko3d renderer's shader specializations off) and
  `ds2_ab_memory_off.toml` (its depth, resolve, upload and stream cache shortcuts off).

**Test:** three short sessions at the same bonfire (the Forest of Fallen Giants' first one is fine): with the normal
`ds2.toml`, then with each A/B file copied over it (keep a copy of the normal one). In each: load, look at the bonfire
for ~20 s, a photo of the flame, quit through HOME. If an item glow or a fog wall is near, a look at it too. Say which
configuration (if any) brought the flame back. Then the `logs/` folder (into `switch-logs`) and the normal `ds2.toml`
back in place.

## Twenty-sixth build (2026-10-23): deko3d, the 3D fixed

Copy over: `ds2-dk.nro` (the only file that changed). You can delete `deko3d_shaders.bin` and `pipeline_list.bin`
next to the NRO: the shaders changed, so the old compiled ones would only be thrown away.

What the twenty-fifth build's test showed, and what changed:

- **The black 3D at 1 frame a second.** The game draws its scenery as triangle strips joined by a "restart" marker.
  The new renderer treated the marker as an ordinary vertex (and deko3d had restarts off), so every draw became
  screen-wide triangles of garbage: a second of GPU a frame, a black picture with the HUD over it. Fixed for both.
- **Then a flat grey 3D (seen on the PC).** The game copies its HDR scene (the 360's 7e3 format) through a 10-bit
  view of the same memory and decodes the colours itself with the texture's exponent adjustment; the new renderer
  did neither. Fixed: on the PC Majula now draws right (character, sky, HUD).

Still known: the save list's portraits, and 720p in handheld mode (540p is not ready on this renderer).

**Test:** first session: load the save at the bonfire, look around and walk a little (expect stutters while the
shaders compile, the first time only). Second session: the four measuring points, ~30 s each, and note the frame
rate in each. If anything draws wrong (missing objects, wrong colours, flashes), a photo helps.

Then the `logs/` folder (into `switch-logs`).

## Twenty-fifth build (2026-10-23): the first deko3d build

Copy over: `ds2-dk.nro` (new), `ds2.toml` and the `configs/` folder. `ds2.nro` and `ds2-opt.nro` stay for comparison.

What `ds2-dk.nro` is:

- **The new renderer**: Renut, the Direct3D-level renderer of sa2-nx 1.0.3, on **deko3d**, the Switch's own graphics
  API. No PM4 ring, no EDRAM emulation and no Vulkan driver (NVK) in the NRO (docs/deko3d.md).
- The game's code generated again with sa2-nx 1.0.3's generator. No LTO/PGO yet (that comes after a collection on this
  renderer).
- **720p in handheld mode** (the game's own resolution). 540p is not ready on this renderer yet. `ds2.nro` and
  `ds2-opt.nro` read the same `ds2.toml`, so they start at 720p too: L3 + R3 held for a second still switches them to
  540p.

What to expect the first time:

- **Shader compiling.** deko3d compiles each shader variant on the console the first time a draw needs it, so the first
  session stutters whenever something new appears. The results are kept in `deko3d_shaders.bin` next to the NRO;
  close the game normally (HOME, then close) so they are saved, and the second session should not stutter for them.
- **The save list's portraits** may come out as noise: known, the game reads them back from memory and this renderer
  does not write them there yet. The saves themselves are fine.

**Test 1 (first light):** open `ds2-dk.nro`. If it boots: the title, the menu, the save list, then load a save. If it
crashes or the screen stays black, note at which moment and send `logs/` and Atmosphère's report folders.

**Test 2 (if it plays):** the four measuring points, ~30 s each (the start of the game, the Fire Keepers' house, the
Heide bonfire warp, the Forest bonfire warp), in a second session so most shaders are already compiled.

Then the `logs/` folder.

## Twenty-fourth build (2026-10-17): the new code with LTO + PGO

Copy over: `ds2-opt.nro`. `ds2.nro`, `ds2.toml` and `configs/` are the twenty-third build's. You can delete
`ds2-pgo.nro` and the `pgo/` folder from the card (the collection was used).

What changes in `ds2-opt.nro`:

- It is now the twenty-third build's new code (sa2-nx's CPU rounds) with LTO + PGO, made with the profile of your
  30-minute collection. From the eighteenth to the nineteenth build the same step gave +13 to +14 % where the CPU
  limits.
- Long sessions: the texture cache also releases its coldest textures when the texture slots fill up (from sa2-nx
  1.0.2.1; there, after three to four hours, new textures were drawn black).
- A session with `configs/ds2_profile.toml` now writes `logs/rex/rex_profile_stacks.log`, so a later session does
  not erase it (last time the PGO collection overwrote the profiled session).

**Test 1, `ds2-opt.nro`:** the three standing points (the thicket, the Fire Keepers' house, the view of Majula),
~30 s each at 540p, then play through some transitions as far as the knights' water area and stay there ~30 s.

**Test 2 (the important one for the next round):** copy `configs/ds2_profile.toml` over `ds2.toml` and play about 10
minutes with `ds2-opt.nro`: the same three points, the water area and three or four area transitions. It records
what the renderer's thread and the game's main thread do in each frame, which is what the next optimisations need.
Put the normal `ds2.toml` back afterwards.

Then the `logs/` folder (with `rex` inside).

## Twenty-third build (2026-10-17): sa2-nx's CPU rounds, trophies and button pictures

Copy over: `ds2.nro`, `ds2-pgo.nro`, `ds2.toml` and the `configs/` folder. Keep `ds2-opt.nro` on the card: it is
still the twenty-second build's (old code with LTO + PGO), for comparison. Delete the old files in the `pgo/` folder
next to the programs before the collection (they belong to the old code and would only confuse the merge).

What each program is now:

| file | what it is |
|---|---|
| `ds2.nro` | **the new one**: sa2-nx 1.0.2's CPU rounds (promoted indirect calls, constants folded at build time, faster float loads, fewer flush-mode switches and loop barriers, registers in locals in loops and leaf functions), the trophy screen and notices, and the Nintendo-style button pictures. No LTO/PGO yet |
| `ds2-pgo.nro` | the same new code, instrumented, for the profile collection. It is slow on purpose |
| `ds2-opt.nro` | the **old one** (twenty-second build), for comparison only |

Also new in both: the reads the game repeats (half of what it read last session) now go through the read cache, and
the profiler only marks frames of 150 ms or more as hitches (`ds2_profile_hitch_ms`).

The whole game's code changed again, so the first test is about stability, as in the eighteenth build.

**Test 1, `ds2.nro` (stability and speed):**

1. Open it, go to the save list and load a save.
2. The usual three standing points (the thicket, the Fire Keepers' house, the view of Majula), ~30 s each at 540p.
3. The trophies: L + Minus opens the trophy screen; the button prompts in the menus should show the Switch's buttons.
   If a trophy unlocks, the notice should close by itself and the speed should stay the same afterwards.
4. Play 15 minutes or more with several area transitions; watch whether the transition hitches changed.
5. Optional, if there is time: one more session with `configs/ds2_profile.toml` copied over `ds2.toml`, going through
   three or four transitions (it records what each thread does during the hitches). Put the normal `ds2.toml` back
   afterwards.

If `ds2.nro` freezes or closes at any point, stop there and send the `logs/` folder (and Atmosphère's report folders,
if any): there is no point collecting a profile of a program that is not right.

**Test 2, `ds2-pgo.nro` (only if test 1 passed):** the profile collection: play 20 to 30 minutes of varied play
(menus, a bonfire, walking, fighting, dying, area loads) and leave through the HOME menu, closing the program. The
collection feeds the next `ds2-opt.nro` (new code with LTO + PGO).

Then: the `logs/` folder (with `rex` inside) and the `pgo/` folder that appears next to the program.

## Twenty-second build (2026-10-15): long sessions and the area hitches

Copy over: `ds2-opt.nro` and `ds2.nro`.

What changes:

- **Long sessions.** The renderer kept forever the images the game copies from its render targets (shadows,
  effects), one per address; in games with the same renderer that runs out of memory after one to two hours (wrong
  shadows, black textures). Now the ones left unused for a minute are released.
- **Area hitches.** In each of them the game stops sending frames while it creates five threads in a row. The
  stacks of those threads are now reused instead of being asked from memory again, and the log says how long each
  creation takes.

**Test, with `ds2-opt.nro`:** a long session (an hour or more, if you can), going through several area transitions.
Watch whether the area hitches changed and whether anything odd shows up over time (shadows, black textures, things
missing).

Then the `logs/` folder (with `rex` inside).

## Twenty-first build (2026-10-13): the robotic sound and logging the hitches

Copy over: `ds2-opt.nro` and `ds2.nro`.

What changes:

- **Sound.** The thread that mixes the game's sound (FMOD's mixer) ran at the same priority as the game's busy
  threads and waited for its turn behind them; the sound blocks went out late (robotic, dragging sound), worse further
  into the game, where there are more voices. It now has a priority above them.
- **Hitches.** Not fixed yet: the log did not record long frames after the first minutes. Now it records every frame
  of 0.4 s or more, with what the renderer was doing in it.

**Test, with `ds2-opt.nro`:** play through the areas where the sound was worst and go through some area transitions
where it hitched. Watch (1) whether the sound got better and (2) roughly when it hitches (no need to note the time:
the log has it).

If the sound gets worse or something new shows up, the priority goes back to what it was with this line at the end
of `ds2.toml`: `ds2_audio_mixer_priority = 0`. If music or voices still break up, also try
`ds2_audio_stream_priority = 45`.

Then the `logs/` folder (with the `rex` folder inside: `rex_profile.log` is the last session's, so the main session
should be the last one you run).

## Twentieth build (2026-10-12): the performance drop after an achievement

Copy over: `ds2-opt.nro` and `ds2.nro`.

What changes: in the 46-minute session performance fell from ~20 to ~12 frames a second at one exact moment, the
first achievement unlocked. The on-screen achievement notice left the program in a slower presentation mode until it
was closed (and the stretches at 1-3 frames a second came from it). The notice was turned off; the achievement is
recorded the same way.

**Test, with `ds2-opt.nro`:** play normally for 40 minutes or more and watch whether performance holds from start to
end. Switching resolution still works (L3 + R3).

Achievements belong to the profile, not to the save: the ones already unlocked do not unlock again in a new save.
They live on the SD card in `switch/ds2-nx/ds2/achievements/465307E4.toml`. To make the test go through an
achievement on purpose, there are two ways:

- play until one that has not unlocked yet (the next boss, for example); or
- before opening the game, rename that file to `465307E4.toml.bak`: the game then thinks none has unlocked, and the
  early ones (in a new save) unlock again. After the test, delete the new `465307E4.toml` and give the `.bak` its
  name back, to keep the original dates. The saves are not touched.

With the fix the notice is not even created, so even a long session without an achievement is enough; going through
one only confirms that this was the problem.

Then the `logs/` folder.

## Nineteenth build (2026-10-12): memory for long sessions and the new optimised program

Copy over: `ds2.nro` and `ds2-opt.nro`. You can delete `ds2-pgo.nro` from the card (no new collection is needed
now) and the `pgo/` folder (its files were already used).

What changes:

- **The crash at 28 minutes was a lack of memory**, not the new code: each of the game's threads reserved 16 MB of
  stack (about 50 threads, 800 MB), and with the texture cache growing over the session the process ran out of
  memory. The stacks now have the size each thread needs. This applies to both programs.
- `ds2-opt.nro` is now the **new code with LTO + PGO**, made with the profile of your collection (the 26 minutes
  before the crash were used).

| file | what it is |
|---|---|
| `ds2-opt.nro` | the program to use: optimised code + LTO + PGO |
| `ds2.nro` | the same code without LTO/PGO (a spare, and for comparison) |

**Test, with `ds2-opt.nro`:**

1. The usual three standing points (the thicket, the Fire Keepers' house, the view of Majula), ~30 s at 540p,
   L3 + R3, ~30 s at 720p.
2. A **long session, 40 minutes or more**, going through several areas and switching resolution a few times along
   the way (that is what brought it down last time). This is the test of the memory fix.

If one of the game's threads needs more stack than it got, the program closes suddenly (with no odd picture before).
If that happens, send the logs and, to keep playing, create a `ds2.toml` in the game's folder with the line
`switch_guest_host_stack_mb = 16` at the end (back to the old behaviour).

Then the `logs/` folder.

## Eighteenth build (2026-10-12): the game's code recompiled another way

Copy over: `ds2.nro`, `ds2-pgo.nro` and `ds2-opt.nro` (`ds2.toml` and the `configs/` folder did not change).

What each program is now:

| file | what it is |
|---|---|
| `ds2.nro` | **the new one**: the game's code translated with the speed options (the PowerPC registers in local variables, direct calls between functions). No LTO/PGO yet |
| `ds2-pgo.nro` | the same new code, instrumented, for the profile collection. It is slow on purpose |
| `ds2-opt.nro` | the **old one** (seventeenth build: classic code with LTO + PGO), for comparison only |

This is the deepest change since the first build: all 83 thousand of the game's functions were generated again. On
the PC the game went through the menus, the save list, Majula and the opening movie of a new game, but the Switch's
compiler is another one, so the first test is about stability.

**Test 1, `ds2.nro` (stability and speed):**

1. Open it, go to the save list (the four portraits must show) and load a save.
2. The usual three standing points (the thicket, the Fire Keepers' house, the view of Majula), ~30 s at 540p,
   L3 + R3, ~30 s at 720p. This is the comparison with the old `ds2-opt.nro`, which you measured already; no need to
   repeat it with that one.
3. Play about 10 minutes normally: combat, a bonfire, an area load.
4. If you can, start a **new game** and watch the opening movie for a few seconds (the video decoder was one of the
   parts that needed care). You can skip it afterwards.

If `ds2.nro` freezes or closes at any point, stop there and send the `logs/` folder (and Atmosphère's report folders,
if any): there is no point collecting a profile of a program that is not right.

**Test 2, `ds2-pgo.nro` (only if test 1 passed):** the profile collection, as last time: play 20 to 30 minutes of
varied play (from the start as far as you get, with combat and loads) and leave through the HOME menu, closing the
program. The old profile does not fit the new code; this collection feeds the next `ds2-opt.nro`.

Then: the `logs/` folder and the `pgo/` folder that appears next to the program.

## Seventeenth build (2026-10-11): the save portraits and another GPU cut

Copy over: `ds2-opt.nro` and `ds2.nro` (`ds2.toml` and the `configs/` folder are the sixteenth build's; if the card's
`ds2.toml` is still `ds2_melhorias_desligadas` from the last test, put the package's back).

What changes:

- **Portraits in the save list:** the game draws each portrait into a small texture and reads its pixels with the
  CPU; the renderer did not give those textures back to the game's memory. Now it does.
- **GPU:** the format conversion of the HDR image that was still left (about 5 ms a frame at 540p) got a shortcut.

The test, with `ds2-opt.nro`:

1. Open the save list and look at the four portraits (a photo helps). Load a save, save/quit to the title and open
   the list again.
2. The usual three standing points (the thicket, the Fire Keepers' house in the same spot as the first time, the
   view of Majula), ~30 s at 540p, L3 + R3, ~30 s at 720p.

Then the `logs/` folder.

## Sixteenth build (2026-10-11): the full profile and cheaper conversions

Copy over: `ds2-opt.nro`, `ds2.nro`, `ds2.toml` and the `configs/` folder.

What changes:

- `ds2-opt.nro`: PGO with both collections (6.5 + 27 minutes) and LTO;
- `ds2.toml` (applies to both programs): the format conversions of the HDR image, which cost 14-17 ms of GPU a frame
  at 540p, take a cheaper path. On the PC the picture came out the same.

**Test 1 (main), `ds2-opt.nro`:** the same three standing points as last time (the starting thicket, inside the Fire
Keepers' house, the view of Majula), ~30 s at 540p, L3 + R3, ~30 s at 720p, L3 + R3. No need to repeat with
`ds2.nro`. Watch for anything odd in colours, brightness or fog (that is what these conversions touch).

**Test 2 (the save thumbnails):** delete `ds2.toml`, copy `configs/ds2_melhorias_desligadas.toml` to the game's
folder, rename it to `ds2.toml`, open `ds2.nro` and go to the save list. Do the portraits come out right or still
noisy? A photo helps. Then put the package's `ds2.toml` back.

Send the `logs/` folder of both sessions.

## Fifteenth build (2026-10-11): PGO + LTO and switching resolution live

Copy over `ds2.nro` and `ds2-opt.nro` (the previous build's `ds2-pgo.nro` can stay: it serves new collections).

| file | what it is |
|---|---|
| `ds2.nro` | the normal build (no LTO, no PGO), the reference |
| `ds2-opt.nro` | the optimised build: LTO + PGO with the profile of your 6.5-minute session |

**Switching between 540p and 720p without closing the game:** hold both sticks pressed (**L3 + R3**) for a second.
The render resolution switches between 960x540 and 1280x720 at once (the picture flickers for a frame) and the log
notes the moment. It works in both programs and lasts until the game is closed; when it opens again, what is in
`ds2.toml` applies (540). In the game R3 also locks on and L3 also does its own action, so use it somewhere quiet.

The test, without overclock, preferably in the same places with both programs:

1. `ds2-opt.nro`, a long session (20-30 minutes), playing normally. Halfway through, at a standing point, leave
   ~30 s at 540p, press L3 + R3, leave ~30 s at 720p and go back with L3 + R3.
2. `ds2.nro`, a few minutes at the same standing point (30 s at 540p, L3 + R3, 30 s at 720p), for the comparison.

What to watch in `ds2-opt.nro`: anything different from `ds2.nro` besides speed (enemy behaviour, physics,
collision, sound, freezes). This build rearranges the whole game's code; if something goes wrong, `ds2.nro` is still
the trusted one.

Then the `logs/` folder (say the order of the sessions).

## Fourteenth build (2026-10-11): preventive functions and the start of PGO

The package folder now has up to three programs. They all use the same `ds2.toml`, the same game files and the same
saves; copy all three over (the ones that exist):

| file | what it is | what for |
|---|---|---|
| `ds2.nro` | the normal build, with 368 small functions of the game declared preventively | everyday play |
| `ds2-pgo.nro` | the **instrumented** build: it counts which parts of the code run | a collection session (below) |
| `ds2-opt.nro` | the build with LTO (link-time optimisation), when it is in the folder | to compare with `ds2.nro` |

In the homebrew menu, go into the `ds2-nx` folder to choose which one to open.

### 1. `ds2.nro`

Play normally. The 368 new functions are of the same kind as the one that brought the game down in the forest (empty
functions, getters and vtable shortcuts that nothing calls directly); they were declared before any was missed. If
the game closes and goes back to the console's menu, the `logs/` folder still says which address was missing.

### 2. The collection session with `ds2-pgo.nro`

This build is **slower** (every branch of the code is counted) and every 2 minutes it stalls for a few seconds while
it writes the counts to the card. It is only for collecting; it is no good for measuring performance.

- Open `ds2-pgo.nro` and play 15 to 20 minutes as varied as you can: menus, a bonfire, walking, fighting, dying, an
  open area (Majula) and a closed one (the forest). What is not exercised is not optimised.
- Leave through the game's menu or through HOME (it writes when it loses focus and when it quits).
- Send the `sdmc:/switch/ds2-nx/pgo/` folder (files `ds2_pgo_<N>.stream`, one per session) with `logs/`.

With those files I make the profile-optimised build.

### 3. `ds2-opt.nro` (if it is in the folder)

The usual measuring routine, without overclock: ~30 s standing at one point and a stretch walking, once with
`ds2.nro` and once with `ds2-opt.nro`, in the same place. Say which session was which.

## Thirteenth build (2026-10-10): the crash in the Forest of Fallen Giants

Copy over: `ds2.nro`.

The game closed because it called one of its own functions that the recompiler had not found (it is only called
through a pointer built in code). That one and its sibling were added. There may be others of the same kind in areas
not visited yet: if the game closes again and goes back to the console's menu, send the `logs/` folder
(`logs/rex/rex_crash.log` and the end of `ds2_NNN.log` say which one it was).

Carry on from where you stopped (the save is before the forest's first bonfire) and keep playing normally.

## Twelfth build (2026-10-10): round 5, the renderer's CPU (indices)

Copy over: `ds2.nro`, `ds2.toml` and the `configs/` folder (`ds2.toml` stops sampling stacks again).

The profile showed the renderer's thread spending 28 % of its time converting geometry indices one by one, because
the game uses a format (strips with restart) the inherited fast path did not accept. Now it does.

Play normally, without overclock; about 30 s standing somewhere and the rest walking. What to watch: missing
geometry, triangles stretched across the screen or deformed objects (that would mean an error in this conversion).

Then the `logs/` folder.

## CPU profile round (2026-10-10): only `ds2.toml` changes

Copy over **only `ds2.toml`** (`ds2.nro` is the same as in the eleventh build; do not replace it).

After the last rounds the limit became the processor: the game uses almost all 3 available cores. This `ds2.toml`
turns on sampling of the CPU stacks, which says in which functions the time goes. The game is a little slower while
it is on.

One session of ~3 minutes, without overclock:

1. ~1 minute standing at the bonfire;
2. ~2 minutes in the tutorial area you were in (walking and fighting normally).

Then the `logs/` folder (the file that matters is `logs/rex/rex_profile.log`).

Since the twelfth build the normal `ds2.toml` comes without sampling; it lives in `configs/ds2_perfil.toml`.

## Eleventh build (2026-10-10): after the system crash

Copy over: `ds2.nro`.

The Atmosphère error screen of the last test came from one of the system's own modules (`pcv`, which handles clocks
and voltages): it ran out of memory for sessions and brought three other modules down with it, among them your
clock sysmodule. Who caused it cannot be said. The game was one of the programs with sessions open in that module
(only to note the clocks in the log), so this build stopped keeping them open.

- Play normally; if the error screen comes back, send the `crash_reports` and `fatal_reports` folders again and tell
  me what was open (status bar, the clock sysmodule's menu).
- If it comes back, a test with the status bar turned off would isolate it.
- The shadows near the Fire Keepers' house and the bonfire are still worth checking.

## Tenth build (2026-10-10): performance round 4 (the lights' shadows)

Copy over: `ds2.nro` (`ds2.toml` and the `configs/` folder stay the same).

What changes: looking at the Fire Keepers' house the game dropped to ~8 frames a second because, for each light with
a shadow, the renderer converted the whole shadow buffer there and back. Those clears are now recognised and done
directly. On the PC the conversions in that view went from ~170 to ~25 small ones a frame.

The same routine as the last test, at 540p and without overclock:

- ~30 s standing looking at the Fire Keepers' house and ~30 s looking at the exit (the fog walls);
- watch the **shadows** near the house and the torches/bonfire: any shadow missing, flickering or in the wrong place?

Then the `logs/` folder.

## Ninth build (2026-10-10): fixing the "ghosts" at 540p

Copy over: `ds2.nro` (`ds2.toml` and the `configs/` folder stay the same).

In the eighth build the colours came back, but a second, shifted silhouette of the character appeared and the scene
looked doubled. The cause: at 540p the game's full-screen effects (shadows and light) read the pixel position at the
wrong scale and came out magnified by 4/3. Fixed; on the PC the picture at 540p now matches the one at 720p.

The first time, the game may stutter a little more in the first minutes (the shaders are prepared again for 540p;
then they are kept).

What to watch, at 540p and without overclock:

- is the ghost gone? Are the character's and the scenery's shadows in place?
- turn the camera both ways (the Fire Keepers' house and the opposite direction) and stand about 30 s facing each.

Then the `logs/` folder.

## Eighth build (2026-10-10): fixing the yellow screen at 540p

Copy over: `ds2.nro` (`ds2.toml` and the `configs/` folder are the seventh build's).

In the seventh build the scene came out yellow/green on the console, with the HUD right: the Switch's video driver
rounds the viewport size to whole pixels, and the one-line draw of the tone-mapping table (0.75 pixel high at 540p)
vanished. On the PC that does not happen, which is why it passed the test here. The renderer now only sends viewports
of whole pixels.

One session: up to the bonfire, ~1 minute standing, without overclock. Are the colours back to normal? Do the text
and the HUD at 540p bother you? Then the `logs/` folder.

If it is still yellow, go back to 720p (below) to keep playing and send me the log anyway.

## Seventh build (2026-10-10): round 3, 540p in handheld mode

Copy over: `ds2.nro`, `ds2.toml` and the `configs/` folder.

What changes: this package's `ds2.toml` renders at **960x540** in handheld mode (56 % of 720p's pixels) and scales up
to the screen. On the PC the picture came out right after a fix (the tone-mapping table was not drawn at 540p); only
the console tells the gain in frames a second. The interface also comes out at 540p, so the text is a little less
sharp.

One session is enough: with the package's `ds2.toml`, go to the bonfire and stand ~1 minute, without overclock.
Watch:

- are the scene's colours normal (no yellow/green screen, nothing too dark)?
- are the text and the HUD still readable? Does the loss of sharpness bother you?

To go back to 720p: delete `ds2.toml`, copy `configs/ds2_teste_720p.toml` to the game's folder and rename it to
`ds2.toml`.

Then the `logs/` folder, as always.

## Sixth build (2026-10-10): performance round 2

Copy over: `ds2.nro`, `ds2.toml` and the `configs/` folder.

What changes: the screen clears the game makes through "aliased" 4x surfaces are now done directly on the normal
targets, without converting the whole target afterwards. On the PC the whole-target depth conversions went from ~6
to 2 a frame and the picture stayed the same; only the console tells the gain in milliseconds.

Two sessions at the bonfire, ~1 minute standing each, without overclock:

1. **Session A:** with the package's `ds2.toml`. Watch the picture: sky/background, shadows and fog the same as before?
2. **Session B:** delete `ds2.toml`, copy `configs/ds2_teste_sem_atalho_clear.toml` to the game's folder and rename
   it to `ds2.toml` (the previous build's behaviour, for comparison).

Then the `logs/` folder, as always.

## Fifth build (2026-10-10)

Copy over: `ds2.nro`, `ds2.toml` and the `configs/` folder.

- **ZCULL on by default** for the scene's depth target (test B of the previous round: 137.9 → 127.2 ms of GPU a
  frame, 7.3 → 7.9 frames a second). If you saw anything wrong in the picture in session B, tell me.
- **Fixed the rejected copy in the bonfire area:** the copy of the scene into an 8-bit texture (used by the fire's
  heat effect) was not made. Watch whether the air around the bonfire ripples now.
- Test: one session at the bonfire, ~1 minute standing, without overclock, and the `logs/` folder.

## Fourth build (2026-10-10): the first performance round

Copy over: `ds2.nro`, `ds2.toml` and the `configs/` folder.

This build is not faster by itself: it measures better and brings a test. It is **two sessions in the same place**
(the first bonfire; with the save, "Continue" opens there), without overclock, each with ~1 minute standing looking at
the scene:

1. **Session A:** with this package's `ds2.toml`, as it is.
2. **Session B:** delete `ds2.toml` from the game's folder, copy `configs/ds2_teste_zcull.toml` there and **rename the
   copy to `ds2.toml`**. (The game only reads the file with that name.) Run again in the same place. Also watch
   whether the picture stays right, especially shadows and object edges.

Then send the `logs/` folder and, if you can, the card's save folder `switch/ds2-nx/ds2/`: with your save I can open
that same area on the PC to investigate.

## Third build (2026-10-10)

Copy over the old ones: `ds2.nro`, **`ds2.toml`** and the `configs/` folder. `game_root/` does not change.

- **Keyboard for the character's name:** when the game asks for text, the Switch's own keyboard opens. Not tested on
  the console yet: if the keyboard does not open, freezes or the name is not accepted, tell me exactly what happened.
- A new **icon** in the homebrew menu.
- **Measuring on in the normal `ds2.toml`:** the game only reads the file named `ds2.toml`; a `ds2_perfil.toml` next to
  the `.nro` is ignored (you can delete it). So this build's `ds2.toml` already has the GPU time per category on. Just
  play: enter the map, walk a little, stand about 30 s looking at the scene, and send me the `logs/` folder.
- No overclock, please, so the numbers are comparable (I disregarded the first log, with a light overclock).

## Second build (2026-10-10)

Only `ds2.nro` and the `configs/` folder changed: copy those two over (`game_root/` need not be copied again).

- **Fixed:** the "Save and quit game" box (and the HUD drawn after it) came out purple/pink. Now it is dark grey, with
  the YES button blue and the NO one orange.
- **A measuring request:** to know where the GPU's time goes in the map (the first test stayed at 5–7 frames a
  second, with the GPU at its limit), run a session with `configs/ds2_perfil.toml` copied over `ds2.toml`: enter the
  map, walk a little and stand about 30 s looking at the scene. Then send me the `logs/` folder again. This
  configuration runs a little slower than the normal one.

## What goes on the SD card

Copy the whole `ds2-nx` folder to `sdmc:/switch/` (it becomes `sdmc:/switch/ds2-nx/`):

```
switch/ds2-nx/ds2-dk.nro               the game (the deko3d renderer; the only NRO since 2026-10-24)
switch/ds2-nx/ds2.toml                 the configuration (logging on in these tests)
switch/ds2-nx/game_root/               the game's files, loose (121 files, ≈ 5.8 GB)
switch/ds2-nx/configs/                 alternative configurations (see below)
```

The game writes next to them: `deko3d_shaders.bin` and `pipeline_list.bin` (the shaders it compiled, kept between
sessions), `cache/`, `logs/`, `deko3d.log`. Left over from earlier builds and no longer read: `ds2.nro`,
`ds2-pgo.nro`, `ds2-opt.nro`, `ds2_shaders.mesp` + `.idx` (the Vulkan renderer's), `deko3d_shaders_f<number>.bin`
(made by the A/B configurations): they can be deleted.
```

The card is **FAT32**, which does not take files of 4 GB or more: that is why the game goes as the `game_root/` folder
and not as one package. The biggest file (`GameData1.bdt`) is 2.8 GB and fits. The copy takes a while; check at the
end that all 121 files arrived (Windows sometimes interrupts big copies to cards).

## How to open it

The game needs more memory than a homebrew opened from the Album gets. Open the homebrew menu **holding R while
starting any game** (title override), then choose "Dark Souls II". Opened from the Album it should not get past the
start.

## What to watch

1. Does it open? Does any picture show (the opening warnings, the Havok logo, the title screen)?
2. On the title screen, START (the + button) and then A go through the menus. "Continue" does not exist on the
   console (the PC's save does not go along): choose New Game.
3. Is there sound?
4. How far does it get: the opening movie, the first map (a field of dark grass with stone pillars)?
5. If it freezes or closes: on which screen, and after how long.

A photo or a short video of the screen helps a lot, especially if the picture comes out wrong.

## What to send back

The whole `sdmc:/switch/ds2-nx/logs/` folder (or just `logs/rex/`), after each try. If the game closes with an
error, the file `logs/rex/rex_crash.log` says where. If it makes an Atmosphère error report
(`sdmc:/atmosphere/crash_reports/`), the most recent one helps too.

## Alternative configurations

In the `configs/` folder; to use one, copy it over `ds2.toml`:

- `ds2_default.toml`: the normal configuration (the same as the `ds2.toml` shipped).
- `ds2_profile.toml` (2026-10-24): measures the GPU's time per pass and per pixel shader every 10 s, and samples the
  CPU stacks (`logs/rex/rex_profile_stacks.log`); a little slower. For finding where the time goes.

The other configurations of earlier builds (the native renderer's and the A/B tests) were removed on 2026-10-24.

Up to the twenty-second build these files had Portuguese names (`ds2_padrao`, `ds2_melhorias_desligadas`,
`ds2_gpu_emulada`, `ds2_perfil`, `ds2_teste_*`), which the older sections below still use.
