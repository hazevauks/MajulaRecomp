# Dark Souls II (Xbox 360): technical notes

Facts measured in the game, each with the tool or the log that showed it. What is still an assumption is marked as
such. Translated from Portuguese on 2026-10-16; the log lines, file names and identifiers quoted are unchanged.

## The disc

`Dark Souls II (World) (En,Ja,Fr,De,Es,It,Pl,Ru).iso`, 7,838,695,424 bytes, XGD2 (the game partition at
`0x0FD90000`). `tools/extract_iso.py --list` (2026-10-09): 121 files, 19 folders, 6,131,414,015 bytes.

| File(s) | Size | What it is |
|---|---|---|
| `Default.xex` | 21.9 MB | The disc's only executable |
| `GameData1.bdt` + `.bhd`, `GameData2.bdt` + `.bhd` | 2.84 GB + 2.46 GB | The engine's data archives (BHD = index, BDT = data) |
| `sound/*.fsb`, `*.fev` | ~0.5 GB | FMOD banks |
| `movie/prologue.wmv` | 298 MB | The only video; WMV |
| `shader/*.bnd`, `shader/Simple/*.fxo`, `font/*.fxo` | ~2.8 MB | The game's shaders (`FlverShader.bnd` 2.47 MB, `SfxShader.bnd`, `Filter.bnd`, `Decal.bnd`, `FeShader.bnd`) |
| `font/<language>/FeFont_Small.fontbnd.dcx`, `regulation.bnd.dcx`, `sfx/`, `param/` | small | Fonts, parameters, effects |
| `$SystemUpdate/`, `AvatarAssetPack`, `nxeart` | — | The Xbox 360's own; the port does not use them |

**Shaders.** The loose `.fxo` files (`shader/Simple/`, `font/`) are uncompressed Xenos shader containers: they start
with `102A1100` (pixel) or `102A1101` (vertex), the format XenosRecomp reads. The `.bnd` files are `BND4` archives with
2,203 (`FlverShader`), 241 (`SfxShader`), 105 (`Filter`), 18 (`Decal`) and 8 (`FeShader`) entries, **2,575 in all**;
the `102A110x` signature does not appear in them in the clear, so the entries are compressed or wrapped (a format not
opened yet). For comparison, sa2-nx's library has 35 shaders: here the ahead-of-time translation and the pipeline
prewarm have another scale.

There is no other `.xex` in the file list, so the manifest has no `[[modules]]`. **Not verified:** whether there is
code inside the `GameData*.bdt` archives (unlikely in a disc game, but only the boot confirms it: an `XexLoadImage`
would show in the log).

## The executable

The XEX2 header read straight from the file (`tools/extract_iso.py --info` and reading the optional headers,
2026-10-09):

| Field | Value |
|---|---|
| Title ID | `465307E4` |
| Media ID | `72EA8B94` |
| Version | 0.0.0.8 |
| Original PE name | `FRPG2.exe` |
| PE date | 2014-01-27 04:52:15 UTC |
| Base address | `0x82000000` |
| Entry point | `0x82993BB0` |
| Image size | `0x016C0000` (23,855,104 bytes) |
| Encryption / compression | AES-128 "normal" / basic |
| Default stack | 262,144 bytes |
| TLS | 64 slots, data at `0x83643A00`, 1,460 bytes |
| Resource | `465307E4` at `0x83650000`, 435,802 bytes |

Imported libraries: `xam.xex` (210 records) and `xboxkrnl.exe` (353 records), both version 2.0.21256.0, minimum
2.0.16537.0.

Static libraries (the XDK built into the game), all **2.0.21256.0**: `XNET`, `XONLINE`, `XAPILIB`, `D3D9`, `D3DX9`,
`XGRAPHC`, `XBOXKRNL`, `XAUDIO2`, `XAPOBA`, `XMP`, `XMIC`, `XMCORE`, `XHV2`, `XPARTY`, `XWMAD`. Compiler: `C1`/`C2`
16.0.11886.0, `LINK` 10.0.11886.0, `LIBCPMT` 2.0.11886.0.

What that already says for the port:

- **Direct3D of XDK 21256** (September 2013: the build paths in the image say `e:\xenon\xdksep13\`). The layout of
  the device structure (the guide, section 10) depends on that version; sa2-nx documents the "2012" layout and
  Futari's differences. No mention of build 21256 in the reference repositories: the offsets have to be checked in
  this game.
- **Network and voice** (`XNET`, `XONLINE`, `XHV2`, `XPARTY`, `XMIC`): the game expects Xbox Live for multiplayer. The
  port runs offline; seeing how the SDK's XAM answers the session calls is work for the first boot.

## Middleware (strings of the decompressed image)

Read from `out/default_image.bin` (written by `rexglue codegen` with `REXGLUE_DUMP_IMAGE`), 2026-10-09:

| Library | Evidence |
|---|---|
| FMOD Ex 4.40.05, with the Xbox 360 XMA codec | `N:\FMOD_FRPG2\Source\fmodexsrc44005\fmod4\xbox360\src\fmod_codec_xma.cpp`, `FMOD XMA Codec`, 188 strings |
| Havok 2012.1.0-r1 (physics and animation) | `hk_2012.1.0-r1`, `Havok Job Thread`, `HavokAnimationSampler` |
| The XDK's WMV decoder, in software | `...\xdksep13\...\codecs\wmv\codecdsp-cayenne\video\wmv\decoder_c9e\...` |
| zlib 1.2.7 | `deflate 1.2.7 Copyright 1995-2012` |

- The audio goes out through XAudio2 with XMA under FMOD: the path the SDK already covers (XMA decoder + mixer).
- The video (`movie/prologue.wmv`) is decoded by the game's own code, which is recompiled with it. On the Switch that
  may weigh; nfsmw-nx has a native WMV3 decoder (`nfsmw_video_wmv3.cpp`) as an alternative.
- The image's only `.xex` string is `xam.xex`: nothing points to modules loaded with `XexLoadImage`.
- 108 shader names ending in `.fpo` (pixel) / `.vpo` (vertex) appear in the image (for example
  `BilateralBlurAO4.fpo`, `BlackOut_Vs.vpo`).

## The build machine

Measured on 2026-10-09: 4 logical threads, 12 GB of RAM, 232 GB free. sa2-nx's LTO link (an image of ~39 MB) already
asks for 15–30 min and a lot of memory on a bigger machine; here `-flto` will have to use few jobs and perhaps stay off
in the everyday builds.

devkitPro in `C:\Users\Usuario\Downloads\devkitPro` (devkitA64 GCC 16.1.0, libnx, `portlibs/switch`). The system's
`DEVKITPRO` variable is `/opt/devkitpro` (an MSYS2 path), and the Windows registry points to an old path
(`Downloads\trabalho\devkitPro`): the Switch build scripts need the real path.

## First boot on the PC (2026-10-09)

The `win-pc` build (clang 20, RelWithDebInfo, classic codegen), the SDK's emulated GPU on Direct3D 12 over WARP
(`REX_D3D12_ADAPTER=-2`), 60 s of running with `tools/win/boot_test.ps1 -VideoAt 3 -VideoFrames 50 -VideoEvery 60`
(log `out/pc/logs/ds2_003.log`, frames in `out/pc/captures/video_3s.rgba`):

- The process stays up for the 60 s, with no "unregistered function", no host exception and no `REX_FATAL`.
- Frames 0–8 (3 to 11 s): a black screen with the loading icon (a bonfire + "DARK SOULS II") in the lower left
  corner. From frame 9 on: the box "This game uses an autosave function. Do not turn off the console or remove the
  storage device while the save indicator is shown at the top of the screen." with the OK button. Text, textures and
  transparency right at 640x360 (half of 1280x720).
- The game stays at that box: the test sends no input. **Nothing beyond that has been seen yet** (menu, character
  creation, gameplay, the opening movie).
- Audio: FMOD opens the device and the mixer delivers ~1,877 frames every 10 s with no silence for lack of data
  (running muted; the sound itself was not heard).
- Files the game looks for and does not find, with no visible consequence so far: `game:\config.properties`,
  `game:\userconfig.properties`, `game:\param\KeyConfigParam.param`, and the `cache:\` and `install:\` devices (not
  mounted by the SDK).
- A repeated warning of the emulated GPU during loading: `Texture fetch constant (88000000 13FC8852 ...) has
  "invalid" type`, about 4 a second for 9 s; it stops when the box shows.

**The development PC's GPU.** Intel HD Graphics 4000 (driver 10.18.10.4252, from 2015): no Direct3D 12 and no Vulkan
driver. The emulated GPU only opens on WARP, in software, which is enough to check the picture but no measure of
speed. The native renderer (Vulkan) has no way to run on this PC with the real GPU: the ways out are a software Vulkan
(lavapipe) on the PC, another PC, or testing on the Switch directly.

## How far the game goes on the PC (2026-10-09)

With scripted input (`ds2_input_script`, `tools/win/boot_test.ps1 -Pad`) and the emulated GPU, with no crash,
"unregistered function" or `REX_FATAL` in sessions of up to 15 minutes:

1. The autosave notice → logos (Bandai Namco, FromSoftware, Havok) → the title screen with "PRESS START BUTTON".
2. Start → "Checking save data" → "Saving content" (the save is created in `Documents\ds2\<profile>\465307E4\`) → the
   notice "Ethernet cable is not connected" with OK.
3. The main menu (New Game / Continue / Go Offline) → New Game → brightness adjustment → options → "Saving content".
4. In a second session, Continue → the data list → the opening movie (`movie/prologue.wmv`: it starts black, with only
   narration, subtitles and credits; then the CGI forest, with the letterbox bars) → a loading screen with an item
   description (right).

The opening movie is decoded by the game's own code and drawn with `DrawVertices` (type 4, 6 vertices a frame). **On
the Vulkan backend (lavapipe) it comes out right** (`out/pc/logs/ds2_008.log`, frames 50–79 of `video_100s.rgba`), at
~4 frame swaps a second. On Direct3D 12 over WARP the same movie came out as a plain blue rectangle: a defect of that
path, not investigated, and of no importance for the port (the Switch uses Vulkan).

**What has not been seen or verified yet:**

- The game in real 3D (Things Betwixt): the sessions ended during the opening movie. No 3D scene of the engine has
  been drawn yet.
- At 3–8 frames a second in the menus, a 150 ms press of the script gets lost between two reads of the controller:
  use ranges (`135-135.6:a`).
- Sound: FMOD decodes XMA all the time (about 1,300 contexts every 10 s in the menus), but everything runs muted.

## The game's Direct3D (trace of 2026-10-09)

Candidates taken from the PM4 headers each function builds (`lis rX,0xC0nn; ori`) and checked at run time with
`ds2_d3d_trace = true` (`app/src/ds2_d3d_trace.cpp`; log `out/pc/logs/ds2_007.log`). The device at `0x40042980`.

| Function | What it is | Evidence |
|---|---|---|
| `sub_829BF328` | draw-time shader load | writes `IM_LOAD`; reads the VS at `device+13072` and the PS at `device+13068` (the same offsets as sa2-nx's, XDK "2012"); called from inside `sub_829C2340` (`0x829C2410`), 4 a frame in the menus |
| `sub_829C2340` | `BeginVertices` (dev, type, count, stride) | writes a 2-word `DRAW_INDX`; returns the pointer `sub_829C2800` copies the vertices to |
| `sub_829C2800` | `DrawVerticesUP` (dev, type, count, data, stride) | 0x48 bytes: calls `sub_829C2340`, then `memcpy(ptr, data, count*stride)`; it is the menus' draw (32 a frame, type 6, 4 vertices of 24 bytes) |
| `sub_829C2848`, `sub_829C2D88` | `BeginIndexedVertices` / `DrawIndexedVerticesUP` (an assumption from their shape) | a 4-word `DRAW_INDX`; not called in the menus |
| `sub_829C2E28` | `DrawVertices` (dev, type, start, count) | a 2-word `DRAW_INDX`; called by the game at `0x830802D0` with type 4, start 0, 6 vertices: the opening movie's rectangle, once a frame |
| `sub_829C3240` | `DrawIndexedVertices` (dev, type, base, start, count) | a 4-word `DRAW_INDX`, 0x418 bytes after `DrawVertices` as in sa2-nx; called by the game at `0x82F0E500` (type 6, counts of 0x66 to 0xA7F indices) |
| `sub_829BB060` | frame swap (Swap) | called by the game at `0x82FECE38`, once a frame (42 in 5 s); two `WAIT_REG_MEM` |
| `sub_829CBAE8` | an internal rectangle draw (clear or resolve) | `DRAW_INDX_2`, once a frame, from `0x829CBFBC` |
| `sub_829A86A8` | command buffer submission | `INDIRECT_BUFFER`, ~10 a frame, from `0x829A92A8` |
| `sub_829A7F70` | the GPU interrupt handler | registered by the game with `SetInterruptCallback` |

`CreateVertexShader` / `CreatePixelShader` have not been located yet; the native renderer reads the containers straight
from the shader objects at load time, so they are only missing for the shader dump.

## Software Vulkan on the PC (lavapipe)

Mesa 26.2.4 for Windows (`mesa3d-26.2.4-release-msvc.7z` from pal1000/mesa-dist-win, SHA-256 `351fc8c8...`) is
extracted in `_tools\mesa`, with nothing installed in the system. To use it:

```powershell
$icd = (Resolve-Path _tools\mesa\x64\lvp_icd.x86_64.json).Path
$env:VK_DRIVER_FILES = $icd; $env:VK_ICD_FILENAMES = $icd
$env:REX_DS2_GPU_BACKEND = 'vulkan'     # the emulated GPU on Vulkan instead of Direct3D 12
```

Measured in the menus (log `ds2_007.log`): the device `llvmpipe (LLVM 23.1.2, 256 bits)`, Vulkan 1.4; the same pictures
as Direct3D 12/WARP up to the title screen, at ~8.4 frames a second (42 swaps in 5 s) against 4–5 on WARP. It is the
same code path (the emulated GPU on Vulkan) the Switch uses in the first frames.

## The game's shaders

`tools/extract_shaders.py` (2026-10-09) reads the containers straight from the extracted disc: each entry of the
`shader/*.bnd` archives is a zlib stream with one container (`102A1100` pixel, `102A1101` vertex), and the `.fxo`
files are loose containers. From 2,584 entries come **120 distinct vertex shaders and 1,289 distinct pixel shaders**
(1,174 are identical copies of others; entry 0 of `FlverShader.bnd` is not a container). They go to `out/shaders_in/`
with the names `tools/build_shader_library.sh` expects, so the library can be built without running the game. What is
missing is Direct3D's internal shaders (clears, copies), which only show up with the native renderer's discovery dump.

## The first 3D map and the limit of the emulated GPU on lavapipe (2026-10-09)

Session `out/pc/logs/ds2_009.log` (the emulated GPU on Vulkan, lavapipe): Continue, the movie skipped with Start at
135 s, about 150 s of loading screen, and the process crashes with `0xC0000005` at 305 s.

- Just before the crash the log shows the emulated GPU creating dozens of pipelines with new shaders (for example
  `VS D24FF649F7459F81, PS A3249875A446A537`): the game had finished loading the map and started drawing in 3D.
- Several of those pipelines fail: `SPIR-V parsing FAILED: Source and destination types of SpvOpStore do not match:
  int (%7) vs. float (%15)` and `vkCreateGraphicsPipelines failed (result=-13)`. That is the SPIR-V the emulated GPU's
  shader translator makes, rejected by Mesa's parser.
- The access violation comes next. The Windows dump (`%LOCALAPPDATA%/CrashDumps/ds2.exe.14192.dmp`) puts the address
  at `vulkan_lvp.dll+0xBFDF2` (a write), inside lavapipe.

The consequence: the emulated GPU is no good for seeing the 3D on this PC. The way is the native renderer, whose
shaders come from XenosRecomp + DXC (another translator).

## The native renderer: first run (2026-10-09)

The shader library (`tools/build_shader_library.sh out/shaders_in`, XenosRecomp + DXC 1.9.2607): of the 1,409
containers, 1,409 translated to HLSL and **1,383 compiled to SPIR-V** (`ds2_shaders.mesp`, 61.4 MB, + `.idx`). The 26
DXC refuses:

| How many | DXC's error |
|---|---|
| 12 (vertex) | `partial explicit stage input location assignment via vk::location(X) unsupported` |
| 9 (pixel) | `use of undeclared identifier 'ToneMapTableSampler_TextureNDDescriptorIndex'` and `no matching function for call to 'getWeightsND'` (the same group of shaders) |
| 4 | `expected unqualified-id` |

They are defects of the translator for constructions SA2 did not have (for example a 3D texture sampler for the
tone-mapping table); until they are fixed, the draws with those shaders have no picture.

First run with `ds2_renderer_native = true` on lavapipe (`out/pc/logs/ds2_010.log`, 78 s, up to the network notice):

- The autosave notice, the logos, the title screen and the dialogs come out the same as with the emulated GPU (a
  visual comparison of the contact sheets, not pixel by pixel).
- About 16 frames a second (911 swaps in 54 s), against ~8 for the emulated GPU on Vulkan on the same lavapipe.
- `[verify] ... differences: 0` in every line (6,390 self-checks of the ring's fast paths).
- The game's shaders are identified by their objects (for example VS `480B3420` → entry n874). Four internal Direct3D
  shaders, loaded by `IM_LOAD_IMMEDIATE` (VS of 15, 24 and 27 words, PS of 9 words), stay unidentified: they are the
  clear/copy ones, which the guide says to wrap with `tools/wrap_discovered.py` from the discovery dump. Not done yet;
  what is left undrawn because of them is not known.

## The native renderer in the first 3D map (2026-10-09)

Session `out/pc/logs/ds2_011.log` (native renderer, lavapipe; Continue, the movie skipped, loading):

- The opening movie and the loading screen come out right (`video_100s.rgba`).
- At 305 s the process crashes with `0xC0000005`, the same moment as the session with the emulated GPU. The dump
  (`ds2.exe.6344.dmp`) shows the fault in a lavapipe rasteriser thread: a read of address 0 in JIT code (outside any
  module), with only `vulkan_lvp.dll` on the stack. No `ds2.exe` frame in that thread. **It is a crash inside the
  software driver while drawing, not in the game's translated code**; what the renderer asked for wrongly (or what
  lavapipe does not tolerate) has not been isolated yet.
- The renderer's last lines before the crash: the 3D's post-processing chain being created (targets of 640x360, 64x64,
  16x16 in format 32, a 256x1 texture), and three warnings:
  - `unidentified pixel shader: 78 words, fingerprint 433CF019AE460530` (a shader that is not in the library);
  - `VS identity mismatch 18 ... loaded_words=126 selected_n=496` (a loaded vertex shader that does not match the
    entry chosen);
  - `signed textures not supported yet: read as unsigned` (format 29).

The next work is the guide's shader cycle (section 6.2): the discovery dump, the vertex variants Direct3D rewrites, the
26 shaders DXC refuses and Direct3D's internal ones, until the log has no unidentified load. To find the draw that
brings lavapipe down, the Vulkan validation layers would help.

## The shader cycle and the engine's first 3D frame (2026-10-09)

**The translator (XenosRecomp).** The 26 containers DXC refused came from four defects, all fixed in
`shaders/XenosRecomp` (`ds2-nx` comments in the code):

| Defect | Shaders | Fix |
|---|---|---|
| a named sampler read as a 1D texture (`ToneMapTableSampler`, 256x1) with no 1D index | 9 pixel | the 1D index is the 2D one of the same slot, as it already was for the unnamed `sN` |
| `getWeights2D` called with 1/size, which only `tfetch2D` accepted | 9 pixel (the shadow map filter; partly the same as above) | an overload that ignores the constant and queries the texture |
| a fetch whose destination only receives constants: `r0. = tfetch2D(...).;` | 4 pixel | nothing is assigned from the fetch; only the constants |
| vertex inputs `TANGENT1` and `COLOR2` with no `vk::location` | 12 vertex (models) | they get the locations of `BINORMAL0` (3) and `TEXCOORD4` (12), which none of them uses; the renderer's `LocationOfUsage` the same |

The library rebuilt with the disc's 1,409 + 18 containers the renderer rebuilt from Direct3D's immediate loads
(`shader_misses/missing`) + 1 wrapped internal shader (15 words): **1,428 containers, 0 failures, 1,417 shaders**
(`ds2_shaders.mesp`, 62.5 MB). In the menus the draws with no shader fell from thousands to 1 every 10 s.

**Direct3D's interpolator relinking.** In 3D, XDK 21256's Direct3D loads the models' vertex shaders as immediate
microcode that is already altered: besides the fetches rewritten for the vertex declaration, it swaps for empty
instructions (`C8000000 00000000 02000000`) the ALU instructions that export interpolators the pixel shader in use does
not read (seen in instructions 39 and 40 of a 126-word shader: the library's `C8038001 00B0C600 A100FF00`). The identity
check refused those shaders (`reason=non-fetch-word`) and the draw was left with no vertex shader. It now accepts
exactly that case (`me_shader_identity.h`, key `ds2_native_vs_removed_exports`): the loaded instruction is the empty one,
the library's is an executed ALU (the control flow's sequence bit) that only exports to registers 0..15. The library's
shader, without the alteration, draws the same picture: it only writes outputs no one reads. **There is no self-check of
this path** (there is no old path that draws to compare against).

**Result** (`out/pc/logs/ds2_014.log`, 435 s, no crash): after the loading the renderer draws the opening scene of
Things Betwixt, ~700 draws and ~60 target copies a frame, 7 draws with no shader every 10 s, at ~0.5 frames a second on
lavapipe. `video_200s.rgba`, frames 100–109: tree trunks, grass and the ground inside the letterbox bars, **very dark and
with the wrong colours**. What is left in the log:

- 58 `identity mismatch`, 25 of them `word-count`: the draw's record points to a shader object of another size (the
  draw ↔ object pairing goes wrong); most are pixel shaders of 9/15/18 words.
- `signed textures not supported yet: read as unsigned` (format 29), in the post-processing chain.
- The crash inside lavapipe at ~300 s showed in 2 of the 4 sessions with the native renderer that reached the map (logs
  011 and 013; not in 012 and 014), with no clear relation to the options turned on. In both the address ends the same
  (`...1678`, a read of 0, JIT code): it looks like the same spot. Cause not isolated.

## The 3D scene after the fade-in (2026-10-09)

Session `out/pc/logs/ds2_017.log` (native renderer, lavapipe, 675 s, no crash; `video_300s.rgba`):

- The opening scene of Things Betwixt shows whole in shape (a trunk in the foreground, grass, ground, ruins in the
  background, letterbox bars), at ~0.6 frames a second: ~700 draws and ~56 target copies a frame, 7 draws with no shader
  every 10 s.
- **The colours are wrong**: after the fade-in the picture saturates in green and yellow (green at maximum in almost
  everything, red at maximum in part, blue zero). The geometry and the depth look right; the defect is in the colour
  chain (HDR targets, copies with format conversion, tone mapping). Leads in the log, none confirmed as the cause:
  - targets in format 2 (floating-point 2_10_10_10, "7e3") copied to a texture of format 7:
    `resolve conversion: guest RT 2 host 97 -> guest texture 7 host 64`;
  - format 2 textures read by the game (`C3: texture ... format 2 linear ... swizzle 124`);
  - `signed textures not supported yet: read as unsigned` (format 29, 64x64, in the luminance chain);
  - the tone-mapping table, a 256x1 target drawn every frame and read as a 1D texture.
- 64 `identity mismatch` in the log (draw ↔ shader object pairing with different sizes).

**The lavapipe crash.** Sessions with the native renderer that reached the map: 011, 013 and 016 crashed (always at
~295–305 s, on entering the 3D); 012, 014 and 017 did not. 017 ran with `ds2_native_textures_binding_thread = false`
(the hypothesis: a draw uses the slot of a new texture before the binding thread writes it, which on a real GPU passes
and on lavapipe is a null pointer read). A session with no crash does not prove the hypothesis: with half the sessions
crashing, several in a row would be needed.

Without a frame debugger there is not much further to go by reading logs: the natural next step is capturing a frame
with RenderDoc (the renderer already has the hook, `me_renderdoc.cpp`; the reference ports have `rdc_*.py` scripts) and
checking pass by pass, and using the Vulkan validation layers for the crash.

## The 3D colour chain, pass by pass (2026-10-09)

**Tools.** RenderDoc (1.46, in `_tools/renderdoc`) captures, but is no use with this renderer over lavapipe: under its
layer the device stops advertising `bufferDeviceAddress` and the renderer does not draw (`C6: the Vulkan device does not
have bufferDeviceAddress: nothing is drawn`). In its place came two diagnostics of our own, tied to the frame trace
(`banjo_trace_frame_s`):

- `DS2_TRACE_DUMP=1` writes the destination texture of every copy (resolve) of the traced frame to
  `out/pc/trace_dump/`, and `tools/trace_dump_png.py` makes the PNGs and the table of minimum/mean/maximum per channel;
- `DS2_TRACE_PEEK=<address>:<bytes>,...` shows bytes of the game's physical memory at the end of the frame.

The Vulkan validation layers are in `_tools/vulkansdk` (SDK 1.4.363.0, installed only by copying files); they are
turned on with `VK_ADD_LAYER_PATH=_tools\vulkansdk\Bin` and `REX_VULKAN_VALIDATION_ENABLED=true`. Not used yet.

**What the frame showed** (`out/pc/logs/ds2_024.log`, the frame after swap 1658, already in game, with the HUD):

- The scene's HDR target (`117AC000`, 1280x720) is **right**: the character on the stone platform of Things Betwixt,
  pillars with ivy, grass, the blue rift in the sky. Read as 7e3, red/green/blue have means 0.12/0.17/0.23 and maxima
  14/25/32.
- The game draws the scene in 7e3 (target formats 3 and 12), copies the same target declaring it 10-bit integer
  (format 10) to a 2_10_10_10 texture — the raw bits — and reads it with format 54. The renderer does that
  reinterpretation by the bits (`EDRAM mode4 pair C000/f3 -> C000/f2`), as it should.
- The tone-mapping table (`111BE000`, 256x1, drawn by a pixel shader from the adapted luminance) came out **all zero**,
  and the final composition (`10220000`) is only the HUD over black.
- The adapted luminance texture (`12556000`) is (0.045; 0.13; 55.7; 1) in the written region.

**Cause found: the textures' `exp_adjust`.** The final pixel shader takes `floor()` and `frac()` of the value read from
the scene and uses the integer part as the exponent and the fraction as the mantissa of the 7e3. That only works if the
fetch returns the 10-bit code multiplied by 8, which on the Xbox 360 is the texture constant's `exp_adjust` field (the
GPU multiplies the sampled value by 2^exp_adjust). The renderer did not apply that field to textures (it only logged a
warning for vertex fetches). Now the exponent (6 bits) goes in bits 16..21 of the word that already carried the
texture's index and signs, and the translator's fetches multiply by 2^exponent. **Verified at run time** with the
rebuilt library (`ds2_026.log`): the trace shows `t0=117AC000 ... fmt 54 ... exp 3`, the scene's 640x360 reduction went
from a mean of ~0.015 to ~0.12 and the measured luminance from 0.045 to 0.14.

A hypothesis discarded on the way: the `[FLT_MIN, FLT_MAX]` limit of the translator's "clamped" log/reciprocal
instructions looked wrong, but `FLT_MIN` there is defined as the largest negative (`0xff7fffff`); it is right.

(Explained in the next section: it was the 256x8 resolve shrunk to 256x1.) The zeroed table was not yet explained by
itself: by the shader that draws it, it comes out exactly zero when the texture coordinate arrives zeroed (or the
luminance read is zero). It could be a consequence of the same cause (the luminance textures are read with another
format) or a second defect; to be checked after the new library.

## The 3D on screen: the tone-mapping table and the lavapipe crash (2026-10-09)

Session `out/pc/logs/ds2_035.log` (native renderer, lavapipe, 560 s, no crash): the final composition of the traced
frame (`10220000`) and the presented frames show the game — the character in Things Betwixt, pillars with ivy, the
grass, the blue rift, the HUD and the banner with the area's name. Two causes, both in the inherited renderer and none
in the game's code:

**1. The tone-mapping table came out zeroed because of a "shrunk" resolve.** The game draws the table on line 0 of an
EDRAM target and copies a **256x8** window to the **256x1** texture. On the console that copy is 1 to 1 (what goes past
the texture's size falls in the padding of its storage). The renderer had a path, made for Mass Effect drawn above the
target's size, that **reduces** the source to the destination with a linear blit whenever the requested window is 1.25
times the destination or more: the 8 lines became 1 and the only drawn line vanished (the log said
`[resolution] the 256x8 scene is downscaled to 256x1 when resolved (supersampling)`). That path now only comes in with
`ds2_native_resolve_shrink = true` (default `false`): the copy is 1 to 1 and cropped. After that the table is
0.004–0.54 with alpha 1 and the final composition has means 0.18/0.20/0.22 (before 0.008/0.007/0.006, only the HUD).

What was discarded on the way, with measurements: the pixel shader's constants (`DS2_TRACE_PS_CONSTANTS`), the framing
of the one-line draw (`DS2_TRACE_THIN_DRAWS`: viewport 0,0 256x1, half pixel right), the vertex attributes of the
table's VS (`DS2_TRACE_VS_ATTRIBUTES=<n>`: a float4 position at offset 0, a float2 coordinate at 16, stride 24) and the
ownership of the EDRAM blocks (the draw published; there was just no change of owner to record).

**2. The crash inside lavapipe was a texture index outside the heap, by a key collision.** The crashes had identical
state (the same JIT code, the same registers, a read of address 0 in a rasteriser thread), that is, one specific draw,
not a race. The code at the crash point loads a texture descriptor's `functions` pointer and finds it null. A new
self-test (`DS2_CHECK_SLOTS=1`: every texture and sampler index handed to a draw has to be inside the heap and already
written) pointed at the draw: the cubemap sampler of register 9 (the environment map) received slot 107 or 110, with the
cubemap heap having 64 entries and a single slot allocated. What returned the slot was `SlotView`, by looking up an
existing view.

The views' key was `XXH3_64bits_withSeed(&image, 8, heap << 40 | swizzles)`. For an 8-byte input XXH3 only XORs the
input with (constant − seed) before the final mix, so two images whose handles differ by 0x200 (bit 41 of the seed falls
on bit 9 of the handle; 0x100 for 3D), with the same swizzles, have the **same** key as a 2D view and as a cubemap view.
Handles are pointers the driver allocates in sequence, hence it happening in some sessions and not others. The 0x200
difference between the handles is a deduction from XXH3's formula; what was observed directly is `SlotView` returning
110 for heap 2 with `next = 2` and an empty free list. Fix: the key is now the hash of the four fields as data (16
bytes), and a key hit whose image or heap does not match uses the empty texture and warns (cause 59). On a real GPU this
brings nothing down: the shader samples whatever is in that position — the defect exists in the reference ports and
would be worth reporting there.

With that the earlier hypothesis (the texture binding thread) falls: sessions 029, 031 and 033 crashed with
`ds2_native_textures_binding_thread = false`. The Vulkan validation layers (session 032, 480 s, up to the 3D) reported no
error; only warnings about fragment shader outputs with no colour attachment.

**How the 3D runs on lavapipe** (i5-3210M, no GPU): ~0.5 frames a second in game, ~670 draws and ~28 copies a frame, 136
pipelines, 212 textures. It is for checking the picture, not for playing.

**What the log still showed, per frame (the first two solved in the next section):** 3 draws with no identified shader;
2 rejected draws (cause 109: a 128x128 copied texture read with another logical size, `resolved logical crop rejected
14B11000 128x128`); identity divergences in the draw ↔ shader object pairing (VS n505/n882, PS n838/n904); signed
textures read as unsigned (format 29).

Diagnostics added: `DS2_TRACE_VERTICES=<bytes>` (the small vertex buffers of each traced draw),
`DS2_TRACE_VS_ATTRIBUTES=<n>`, `DS2_TRACE_PS_CONSTANTS`, `DS2_TRACE_THIN_DRAWS=1`, `DS2_CHECK_SLOTS=1`.

## A complete frame in the first map: no rejected draw and none without a shader (2026-10-09)

Session `out/pc/logs/ds2_037.log` (560 s, no crash, `DS2_CHECK_SLOTS=1` with no error at all): in the traced frame, 673
draws, **0 rejected and 0 with no shader** (before, 2 and 3 a frame); the sky got the stars that were missing. Two
fixes, the two open items of the previous section:

**Compressed textures at addresses that were once a copy's destination.** During loading the game copies small targets
to textures (256x152 at `14AE9000`, 128x76 at `14B11000`), frees that memory and loads the map's DXT1 textures into it
(128x128 at `14B09000` and `14B11000`). The renderer still had the old copy's record, treated the fetch as a read of a
copied texture, the size did not match and the draw was rejected (cause 109). A copy (resolve) never produces
block-compressed data, so a fetch of a DXT format is now always read from the game's memory
(`ds2_native_compressed_fetch_not_resolved`, default `true`).

**Vertex shaders with the fetches reordered across two streams.** The game's Direct3D rewrites the shader's vertex
fetches for the vertex declaration in use and groups them by stream. The reordering proof inherited from sa2-nx
(`me_vertex_fetch_reorder.h`) only accepted a fetch that moved between equivalent bases (the same stream); the map's
geometry uses two streams (strides of 20 and 28 bytes) and r2's fetch leaves the end of the library's order for the
first stream's group: `DECL permutation crosses inequivalent actual full/mini bases`, and the draw was left with no
vertex shader. The condition existed because the offline tool the proof copies fixes a container that is then read in
its own order; at run time the draw path takes buffer, stride, offset and format of each element from the slot where
the **loaded** program has it, and a mini fetch's base from the previous full fetch in the loaded program. Now only what
the order could change is required: the same index source in both bases and no fetch between them writing that source
(`ds2_gpu_vs_fetch_reorder_streams`, default `true`). The same proof now also accepts, together with the reordering,
the exports removed by the interpolator relinking. Identified this way: VS n33 (261 words, 10 fetches) and VS n939 (138
words, 7 fetches). The host test `shaders/tests/test_vertex_fetch_reorder.cpp` (from sa2-nx, against SA2's package) was
not redone for the new condition.

The log still records identity divergences in the pairing by command buffer address (a wrong shader object is picked
first and discarded by the microcode check); every draw ends up identified.

## Polishing on the PC: what else the log and the validation showed (2026-10-09)

- **A render resolution below 720p does not work in this game (blocked).** With `ds2_render_height_handheld = 360` (an
  attempt to speed up the tests on lavapipe) the process crashed with heap corruption in the first 3D frame. The
  validation layer showed the cause: `vkCreateImage(): extent.height is zero`. sa2-nx's scaling converts the edge of a
  game pixel to the host by the pixel-centre rule; at q/4 = 1/2 the edge of line 1 is 0, and the game's one-line targets
  (the 256x1 tone-mapping table) and the 1x1 ones get height 0. The render heights were then limited to 720–2160. Later
  (2026-10-10) the minimum became 540, where the same arithmetic gives height 1: see "Round 3" at the end.
- **`signed textures not supported yet` was a false alarm** in this game's case: they are the floating-point luminance
  targets (format 29, read by the host as 16F), which already have a sign. The warning now stays only for integer
  formats.
- **The "difference" the `[verify]` line counts (1)** is the `C6 prewarm: MISMATCH`: on lavapipe creating a pipeline
  that was already prewarmed takes as long as compiling (there is no binary cache), and the prewarm thread turns itself
  off. It belongs to the software driver, not the game; to be checked again on the Switch.
- **Pairing draws ↔ `Draw*` records:** `draws paired with a Draw* record 0`. Every draw arrives inside indirect buffers
  and none matches the records of the three `Draw*` hooks; the pairing falls back to the command buffer address (which
  sometimes points to an old shader object, hence the `identity mismatch`) and the final identification is by the
  microcode. The picture does not depend on it; it is CPU cost on the ring thread, to be handled with the CPU
  optimisations.
- **Vulkan validation** over a whole session up to the 3D at 720p (`ds2_032.log`): no error.
- The game keeps its progress in `Documents\ds2\<profile>\465307E4\00000001\-GAME_0000` (23 files, 7.2 MB), opened every
  session: the test sessions continue from where the previous one saved.

## Going through the game on a PC with no GPU: tools and what was covered (2026-10-09)

**The problem.** On lavapipe the 3D map runs at ~0.5 frames a second, and the game advances per frame: at that rate it
runs in slow motion (the character took ~5.5 min of wall clock to run from the starting platform to the first rock).
Timed scripts are no use for walking around the map, and it cannot be played by hand.

**New tools:**

- `ds2_diag_draw_every = N` (a diagnostic): only one frame in N is sent to the GPU; in the others the ring thread reads
  every packet, identifies the shaders and checks the draws, but sends no draw, clear or copy. With N = 61 the game runs
  at 10–12 frames a second in the map (20 times faster) and the picture updates every ~5 s. N odd so that the game's
  alternating targets (adapted luminance) keep alternating. In the first 3D frame the warning `texture format not
  supported yet (cause 429)` shows: it is the luminance texture read before the first copy, because of the skipped
  frames.
- `ds2_input_live = <file>`: live control. The game rereads the file when it changes; items `<buttons> [seconds]`
  separated by commas (`lup, a 0.6`).
- `ds2_snap_live = true`: capture on demand (`captures/snap_request` → `captures/snap.rgba`).
- `tools/live.py`: `pad "<items>"`, `snap [name]`, `wait <s>`, `sheet a b c` (one sheet with several captures), to guide
  the game step by step in a long session (launched separately with `Start-Process`, since it outlasts a command).
- `audio_dump_output_s` now also records with `audio_mute` (the test sessions are muted): **the only difference of
  `sdk/` from sa2-nx's**, in `sdk/src/audio/sdl/sdl_audio_driver.cpp`, marked with `ds2-nx`.

**What was covered and checked by picture:**

- From boot to the game: the first notices, the Havok logo, the title screen, the save's "Now loading", the main menu
  (New Game / Continue), loading screens with an item description, the map's fade-in. All right.
- A 25-min session at 720p walking through the grass of Things Betwixt (`ds2_040.log`, every frame drawn): 0 rejected
  draws, 0 with no shader, 0 slot errors, no crash. Grass, rocks, HUD and the area banner right.
- A guided 22-min session with 1 frame in 61 (`ds2_042.log`): 14,766 game frames, 0 rejected, 0 with no shader, 183
  pipelines, 404 textures. The character fell into a chasm, **died, the game reloaded and he respawned** on the starting
  platform: death, loading screens and the map reload work.
- **Audio** (70 s recorded from the title screen and the menus, `out/pc/audio_output.wav`): 24 s of silence during the
  first notices, then a continuous signal, peak 0.29, no clipping, no NaN, stereo (L ≠ R), with a pulse every 6 s that
  matches the script's A presses (the menu sound). It looks like real audio; the quality can only be judged by ear.

**Where it stopped.** Leaving the platform in a straight line the character hits a rock after the grass; going round to
one side there is a chasm, to the other he ran into the same rock again. With a picture every ~5 s and the camera
swinging back behind the character, each "look and act" step takes more than a minute; finding the way to the hut
(where the cutscene and the character creation are) is a matter of hours on this PC. It went no further than the
starting area.

## Building for the Switch: what this machine needed (2026-10-10)

The user's decision: the PC (software Vulkan) is too weak to keep testing on; the tests move to the console.

**The game (`tools\win\build_switch.ps1`).** devkitPro here is in `Downloads\devkitPro` and the system's `DEVKITPRO`
variable is `/opt/devkitpro` (an MSYS2 path). CMake rereads the toolchain file inside the compiler tests, where
`-DDEVKITPRO` does not reach, and the configuration failed with `libnx linker specs not found: /opt/devkitpro/libnx/
switch.specs`; the script now exports the Windows path. The configuration also requires the driver
(`REXGLUE_SWITCH_NVK_SDK` with `lib/libvulkan.a`); to compile the objects while the driver does not exist, a folder with
an empty `libvulkan.a` serves (`out\nvk_stub`), and at the end one reconfigures with the real SDK (only the link is
redone). No LTO or PGO at this stage (`app/CMakeLists.txt` does not turn them on yet).

**The driver (NVK, Mesa).** There is no ready binary: one compiles
[danfromtico/mesa-switch](https://github.com/danfromtico/mesa-switch) at commit `1a8c1a66d6f` with nfsmw-nx's patch
(`_ref/nfsmw-nx/mesa/mesa-switch-nfsmw.patch`), in `_tools/mesa-switch` (outside git). The scripts were written for an
MSYS2 in `C:\msys64`; what was installed and adjusted here:

- MSYS2 in `C:\msys64` (winget `MSYS2.MSYS2`), and `C:\msys64\opt\devkitpro` as a junction to `Downloads\devkitPro`;
- packages: `flex bison git make patch diffutils pkgconf`; from MINGW64 `gcc meson ninja python python-mako python-yaml
  python-packaging rust-bindgen pkgconf`; from CLANG64 `clang llvm lld libclc spirv-llvm-translator spirv-tools cmake
  pkgconf cbindgen zlib zstd` (Mesa's host tools are compiled with CLANG64's clang);
- in devkitPro's MSYS2: `switch-libexpat` and `switch-libzstd` (Mesa needs them; they did not come installed);
- Rust through `rustup-init` inside `_tools/mesa-switch/build/deps/{cargo,rustup}` (where the wrappers look for it):
  `stable-x86_64-pc-windows-gnu` with the target `aarch64-unknown-linux-gnu`;
- `switch_native_tools_msys2.txt`: `cbindgen` in `clang64` (the file pointed to `clangarm64`, the author's machine);
- `windows_x64_host_cross_file_msys2.txt`: the `cmake` entry points to an executable that does not exist. With CMake 4.4
  and Windows in Portuguese, Meson 1.12 breaks when it queries CMake for an optional dependency (`libudev`: `'utf-8'
  codec can't decode byte 0xba`, "This is a Meson bug"); nothing in that step needs CMake;
- running, in a MINGW64 shell: `PATH=$PATH:/clang64/bin DEVKITPRO=C:/msys64/opt/devkitpro ALLOW_DIRTY=1
  ./build-unified.sh` (with `NINJA` pointing to a `ninja -j2` wrapper, to share the machine with the game's build).

**The first NRO (2026-10-10).** `out\nx\ds2.nro`, 105 MB, classic codegen, no LTO or PGO. Times on this PC: ~2 h 15 min
for the game's 938 steps (4 threads, sharing the machine), ~35 min for Mesa with 4 threads.
`tools/check_switch_build.py` (adapted to DS2) passes:

- `_start` at address 0; the NRO carries the freshly compiled driver (`Mesa 26.2.1 (git-1a8c1a66d6)`, the same as the
  SDK's);
- 5 branches to self in recompiled functions (`sub_828FC278`, `sub_82B11CB8`, `sub_82BE8748`, `sub_82C3FAA0`,
  `sub_82DE5A18`). The game's image has no `b .`; they are loops of the original code that do not end for an input the
  game does not produce and that read no memory on that path (a list search that keeps comparing a null node; a fill
  whose counter is not reread). GCC proves they do not end and emits `b .`. Two were read in the generated code; the
  other three have the same shape in the disassembly. They are not wait loops without a barrier;
- 2 vector `fmla` in `sub_82C31A30`: they are two scalar `fmsubs` of the game itself (there are 24 in a row in the
  function) that GCC joined into one two-lane `double` instruction; it is not a contraction that leaked (VMX arithmetic
  is single precision).

The link without the driver also flagged `posix_memalign` (used by FFmpeg): on the console it is defined by mesa-switch's
own `libvulkan.a` (`rust_switch_stubs.c`).

**The SD card.** The user's card is FAT32: the game goes as a loose `game_root/` folder (121 files, 5.71 GB, the biggest
2.64 GB), with no single package. `tools\win\package_switch.ps1 -IncludeGame -LooseGame` assembles
`out\nx\sdcard\switch\ds2-nx` (129 files, 5.87 GB); instructions in `docs/switch-testing.md`.

## First boot on the Switch (2026-10-10)

Logs in `out/switch_logs/001` (outside git): `ds2_001.log`, `rex_profile.log`, `rex_stderr.log`. The console in handheld
mode, CPU at 1020 MHz, GPU at 460.8 MHz (the SDK's clock manager raises the GPU from 307 to 460 MHz).

- **It worked first time:** a 3 min 18 s session, from boot to the first map (New Game) and to the exit through the
  menu, with no crash. The user's report: the picture right from start to end, except the exit confirmation box,
  pink/purple.
- **Renderer:** 417,049 draws, **0 rejected, 0 with no shader**, 192 pipelines, 251 textures, 3 vertex shaders
  identified by the fetch reordering. No renderer warning in the log.
- **Sound:** 471 of 471 audio buffers with signal in the map, peak 0.19, none clipped.
- **Save:** the game created and rewrote the save on the card (`ds2/B13EBABEBABEBABE/465307E4/00000001/-GAME_0000`, 23
  files).
- **Frame rate:** menus and 2D screens at 43–56 frames a second; in the 3D map **5–7 frames a second** (up to 13
  depending on the view).
- **The limit in 3D is the GPU:** `GPU per Swap: total ~150 ms`; the ring thread spends ~80 % of the time waiting for
  the GPU (`fences waited 46/s, 805 ms/s`) and uses 19 % of a core. Per frame: ~606 draws, ~28 copies, and the EDRAM
  transfers (per 10 s: `color alias=402 depth import=636 guestspace depth resolve=804`, and 8.5 representation
  conversions of the whole depth target per frame). The GPU time categories were off in this session; the per-pass
  detail comes with `configs/ds2_perfil.toml`.
- **CPU:** the game's main thread stays at 92–96 % of a core both in the menus (50 frames a second) and in the map (6
  frames a second), that is, it spins waiting instead of sleeping; the profile counts ~38,600 `svcSleepThread` of up to
  1 ms a second. To be looked at with stack sampling.
- This NRO has no LTO or PGO, and none of the reference ports' per-game optimisations.

**The purple "Save and quit game" box (fixed).** Reproduced on the PC with the live control (Start, the system icon,
Quit Game): the box came out purple, the selected button pink, and the HUD drawn after it tinted. The frame trace (now
triggerable at any moment: `DS2_TRACE_LIVE=1` + `tools/live.py trace`) showed all those draws with right inputs: the
same shader pair (VS n505, PS n838: `texture * g_MulColor + g_AddColor`, times the vertex colour), white vertex colours
and `c32 = (1,1,1,1)`. The textures, extracted from the game's data (`DS2_DUMP_TEXTURE=<bases>` + `tools/bc_to_png.py`),
are dark grey leather, blue frames and an orange glow: nothing purple. The purple comes from **one** HUD draw the game
itself tints with `c32 = (0.77; 0.29; 0.90; 0.69)`, in additive blending.

The cause was in the inherited constant deduplication (`ds2_native_constants_dedupe`): when a new generation of
constants has the bytes the shader reads equal to those of the last block sent, the block is kept. But it stayed marked
as valid for its whole old size. The game's sequence in every interface frame: the purple quad (the shader reads up to
c33; block sent with a purple c32), text (the text shader reads fewer registers; the prefix is equal: block kept and
taken as current), the other quads (a white c32 in the registers, the same generation: the purple block reused). Fix:
when a block is kept, it is now valid only up to the compared size (for VS and PS). The same defect exists in sa2-nx;
it did not show there because that game's shaders do not combine like this.

Verified on the PC: a dark grey box, YES blue, NO orange, the soul counter normal. The NRO was rebuilt with the fix.

## Second and third tests on the Switch (2026-10-10)

Logs in `out/switch_logs/002` (sessions `ds2_004` and `ds2_005`, already with the fixed NRO).

- The user confirmed on the console: the "Save and quit game" box came out right.
- **Without overclock the map stays at 6.8 frames a second, steady** (68 swaps every 10 s for minutes; `GPU per Swap:
  total 148.5 ms`; ~608 draws a frame). The 9–13 frames a second of the first log came from a light overclock the user
  tried: disregarded.
- The GPU time categories were not on: the game only reads `ds2.toml`, and `ds2_perfil.toml` was copied next to it
  without replacing it. The test `ds2.toml` now carries `ds2_gpu_marks_categories = true`.
- **A progress blocker:** further on the game asks for the character's name (`XamShowKeyboardUI`), and the SDK's dialog
  is an ImGui box that needs a physical keyboard. On the Switch the call now opens the system keyboard (libnx `swkbd`),
  in a "headless" run of the SDK that returns the text or `X_ERROR_CANCELLED`. The second difference of `sdk/` from
  sa2-nx's. Not tested on the console yet; on the PC nothing changes.
- The NRO's icon: `assets/icon.jpg` (outside git; a 256x256 JPEG supplied by the user), used by CMake when it exists.

## The first GPU profile on the console (2026-10-10)

Session `out/switch_logs/003/ds2_006.log` (9.5 min; character creation with the system keyboard, which worked:
`XamShowKeyboardUI: software keyboard confirmed (7 of at most 13 characters)`; then the first bonfire, standing still).

**At the bonfire: 6.3 frames a second, `GPU per Swap: total 158.3 ms`**, ~980 draws, ~80 target switches and ~60 copies
a frame. By category (ms a frame): scene 34.0; 640 targets: 6.1; copies 3.9; scene without Z 2.7; gaps 10.6;
`edram_import` 35.0; `edram_alias` 50.1; `edram_import9` 15.1. The last three are passes that **begin** with an EDRAM
transfer; the time includes the game's draws that come after in the same pass. By the time per pass (a sum over 10 s,
63 frames), what is only transfer, with no game draw:

| Pass | ms a frame | What it is |
|---|---|---|
| cat 10, no shader | 21.5 | depth imports (a change of representation of the whole target, ~8 a frame) |
| cat 13, no shader | 9.3 | stencil imports in 9 passes (~4 a frame) |
| cat 12, no shader | 1.5 | colour conversions between views of the same EDRAM |
| cat 12, PS n720 | 22.6 | the game's draws (2 full-screen passes with a 3.2 KB shader) |
| cat 2, PS n95 / n1125 | 10.0 / 6.6 | scene |

The 16 passes listed add up to 56 % of the time; the rest is spread over dozens of small passes (the list now has 64
entries). That is, ~32 ms a frame (20 %) is pure EDRAM emulation, and the rest is the cost of the game's own frame on
this hardware at 720p with the GPU at 460 MHz.

**ZCULL off on the main depth target.** The driver logs `ZCULL: SIN plano para profundidad 1280x1280: usos
incompatibles (TRANSFER_DST?)`, and the same for the shadow maps (800x800, 1600x1600, 1040x1040). The renderer gives
`TRANSFER_DST` to the single-sample depth views to import stencil through the copy engine
(`ds2_native_edram4_stencil_copy`), and that costs the ZCULL plane. In SA2 the scene's depth is multisampled and does
not fall under that rule; in DS2 it is single-sample, so the whole scene runs with no hierarchical culling. A/B test
prepared: `configs/ds2_teste_zcull.toml` (`ds2_native_zcull_1x_depth = true`).

**A new rejected copy:** `resolve exponent bias / format not supported (cause 14)`, from the bonfire area on (1,226
rejected copies of 112,297 in the session). The path with an exponent bias only covers FP16 → FP16; the log now names
the formats. No report of a visible defect, but it is content that does not reach the texture.

## Round 1 on the console: ZCULL on the single-sample depth and the biased copy (2026-10-10)

Logs in `out/switch_logs/004` (`ds2_007` = A, `ds2_008` = B), the same scene (the first bonfire, standing still), no
overclock.

| | A: default | B: `ds2_native_zcull_1x_depth = true` |
|---|---|---|
| frames a second | 7.3 | 7.9 |
| GPU a frame | 137.9 ms | 127.2 ms |
| `edram_import` | 35.0 | 20.0 |
| `edram_import9` | 15.1 | 19.0 |
| `edram_alias` (includes the scene draws that come after) | 50.0 | 50.1 |
| scene | 23.4 | 23.6 |

The gain came from the depth imports, not from the scene: the scene's passes did not change, that is, their cost is
fill/shader, not hidden pixels. With ZCULL the stencil imports leave the copy engine and go back to the 9 passes
(+3.9 ms), and still the balance is −10.7 ms. Adopted in the test `ds2.toml`.

Reading the pass list (relative weights within each category; the absolute values of that list do not add up with the
categories', which are the calibrated ones): in `edram_alias`, ~70 % is PS n720 in two passes (~35 ms a frame: it is
the scene's main material, not a transfer); `edram_import` (20 ms) and `edram_import9` (19 ms) are only transfer:
**~39 ms a frame, 31 % of the GPU time, in conversions the original game does not do.** On the PC, the pair report
shows what the 9-pass ones are: colour-to-depth imports of a few blocks (8 to 48) when the shadow map and other
targets reuse the start of the EDRAM where the scene drew colour.

**The copy with an exponent bias.** The console's log gave the formats: bias −2, from the 7e3 target (format 12, FP16 on
the host, 1280x1280) to the RGBA8 1280x720 texture `10951000`, command 1. It is the copy of the scene to 8 bits with a
1/4 factor, read by PS n723 in a full-screen draw (the bonfire's heat effect). Now done by the copies' fragment pass
(`ds2_resolve_copy.frag` gained a factor; 1.0 for the usual copy). Verified on the PC **with the console's save**
(`logs/ds2` copied to `Documents\ds2`; the PC's earlier save is in `out/pc_save_backup`): the bonfire area opens
through Continue, the log records the copy and the texture comes out with the scene.

## Round 2: the clears through an aliased 4x surface (2026-10-10)

Console, round 1's build (`out/switch_logs/005/ds2_009.log`): 7.4 frames a second, 136.4 ms of GPU a frame at the
bonfire, no rejected copy (the biased copy now runs; the pass that reads it costs ~9 ms).

**Where the imports come from.** A trace of an EDRAM block on the PC (`ds2_diag_trace_tile`), in the same scene, with
the console's save. Every frame the game uses Direct3D's Clear through a 4x surface of half the pitch, which aliases the
same blocks as the single-sample target:

1. the shadow map's depth (base 0, pitch 800), depth only;
2. colour in format 3 (7e3, the 640x360 HDR target) + depth at 0xB8;
3. colour in format 0 (the 1280x720 target) + depth at 0x2D0;
4. other half-resolution depths (0x658, 0x710).

The renderer has sa2-nx's shortcut ("redirected clear": clearing straight into the single-sample owner), but it refused
these cases, drew the clear in the aliased view and then converted the whole target to the single-sample view: one
depth import (with ZCULL, in 9 passes when the previous owner was colour) and one colour conversion per clear. The
reasons for the refusal, from the `depth clear redirect declined by` report:

- `also writes color (color 0 not redirectable)`: the colour half only accepted format 0 with an RGBA8 owner. In DS2 the
  start of the EDRAM is reused by 8-bit and floating-point targets, so the blocks' owner almost never fits;
- `target C.../f0 ... (clear of ... stencil -1)`: a depth-only clear over blocks of a colour owner, with no known
  single-sample view. It was only learned through the pointer of the import's source view, and in DS2 that source is a
  view with a raster grid (`msaaXY=0,1 gridX=1`), a different object from the plain 4x view the shortcut looks up.

**What changed** (`ds2_native_edram4_clear_color0_alias`, default `true`):

- the single-sample view of a 4x colour view is learned where the blocks are transferred, and the colour half of the
  clear goes straight to it, which becomes the blocks' owner, when the current owner cannot receive it;
- format 3 is accepted with any colour: the value written is what the clear's own draw would leave in the RGBA16F image
  (output clamped as in every draw into a 7e3 target: RGB in [0; 31.875], alpha in [0; 1]), rounded to the nearest
  half. The clear colour of the game's HDR target is not zero;
- a rectangle that ends in the middle of a block (the 640x360 targets: 22.5 rows) is accepted when the owner of the
  partial block has the same format;
- the single-sample view of a depth is also remembered by the surface's geometry (base, format, encoding, pitch in
  blocks).

**Measured on the PC** (lavapipe, 4 frames drawn per window): whole-target depth conversions, from 6 to 2 a frame (1,924
→ 352 blocks); the 9-pass import of the main depth's 720 blocks went away; 896 colour blocks a frame cleared directly
(676 handed to the single-sample view). The same picture (a capture at the bonfire). Still refused per frame: 1 colour
clear and 2 depth clears where the partial block of the last row belongs to a view of another format (the 640x360
targets); the way would be to synchronise only those blocks before clearing.

**Measured on the console** (`out/switch_logs/006`, bonfire, standing still, no overclock; both sessions on the same
build):

| session | shortcut | frames a second | GPU a frame | edram_alias | edram_import | edram_import9 |
|---|---|---|---|---|---|---|
| `ds2_010` | on (default) | 12.7 | 78.8 ms | 30.0 | 5.8 | 3.9 |
| `ds2_011` | `ds2_native_edram4_clear_color0_alias = false` | 7.9 | 127.9 ms | — | — | — |

In the session with the shortcut: scene 23.7 ms, 640 targets: 5.0, copies 4.8, scene without Z 2.6. No rejected draw or
copy, no picture defect reported. Left per frame on the console: the colour clear with a partial block of another
owner and two 9-pass imports of 8 blocks each (`C000:1280x1280 → 000:800x800` and `→ 0B8:640x720`).

A curiosity reported: the New Game/Continue menu runs at 55–56 frames a second the first time and at 60 after loading
the game and going back. The profile shows the game's main thread near 97 % of a core in both cases (the menu is bound
by the game's CPU, not the GPU) and the pipeline prewarm ends in 0.9 s; the cause of the difference was not
determined.

## Round 3: 540p in handheld mode (2026-10-10)

sa2-nx's render scaling (`ds2_render_height_handheld`, q/4 of the game's size) was blocked below 720p because at 360p
(q = 2) the one-line targets get height 0. At 540p (q = 3) the edge of line 1 is `ceil(3/4 − 1/2) = 1`, so the minimum
became 540 (`QuartersOf` in `app/src/me/ds2_render_mode.cpp`).

**First attempt on the PC: the whole scene in yellow/green noise, the interface right.** No crash and no new validation
error. The dump of the frame's textures (`DS2_TRACE_DUMP`, now in host pixels: before it read the game's size from a
smaller image) compared with 720p's showed a single different stage: the tone-mapping table (`111BE000`, 256x1 → 192x1)
had stale EDRAM content instead of the grey ramp. Cause: `ViewportAndScissor` (`ds2_native_draws.cpp`) drops draws with
a viewport less than 1 pixel high, and the game's 256x1 viewport becomes 192x0.75 on the host. Such a viewport still
covers the centre of line 0's pixel; the limit is now `min(1, q/4)` host pixels (one game pixel below 720p; the same as
before at 720p or more). The drop showed in no counter ("0 rejected").

**After the fix:** the picture right at 960x540 on the PC (scene, HUD, text, the tone-mapping table identical to 720p's),
the same three validation messages as before, `DS2_CHECK_SLOTS` clean. A crop of the same dark region at 540p and at
720p shows no new pattern (the half-resolution fog layers show in both).

Round 3's package `ds2.toml` goes with `ds2_render_height_handheld = 540` to measure on the console;
`configs/ds2_teste_720p.toml` goes back to the game's resolution. The interface is also drawn at 540p and scaled up
(less sharp text): whether the trade is worth it is the player's decision.

**On the console the scene stayed yellow** (`out/switch_logs/007/ds2_012.log`, a photo from the player; the HUD right).
The fix above only holds on lavapipe. NVK programs the hardware viewport's clip rectangle with integer fields
(`nvk_cmd_draw.c`: `.y0 = ymin`, `.height = ymax - ymin`, truncated floats): a height of 0.75 becomes 0 and nothing is
drawn; on top of that, a viewport of 33.75 pixels loses its last line (the bloom's 96x45 targets become 72x34).

The fix in the renderer (`ds2_native_viewport_whole_pixels`, default `true`, in `ComputeFraming`): outside the game's
scale, a viewport with a fractional edge becomes the whole host pixels whose centres it covers (the same rule as
`me_resolution_scale.h`) and the difference goes to `g_NdcScale` / `g_NdcOffset`, which every vertex shader applies in
clip space. The geometry lands on the same host coordinates; D3D's half pixel is already computed from the viewport
sent. For the table: `0..192 x 0..0.75 → 0..192 x 0..1`, NDC scale `1 0.75`, offset `0 0.25`. On the PC the picture
stayed the same as with the fractional viewport and the table is the same ramp. The effect on NVK only the console
confirms.

**Performance at 540p on the console, still with no firm number.** In the same session (wrong picture, ~80 s inside the
map, no overclock) the 10-s windows gave 13.3 / 13.7 / 13.8 / 12.2 / 15.7 / 17.2 / 17.0 frames a second with 76.2 /
73.2 / 72.5 / 82.5 / 63.4 / 58.1 / 58.6 ms of GPU a frame: it did not settle as at 720p (127 frames per window, 78.8 ms,
in 17 windows in a row). In the last two: scene 22.4 ms (almost the same as 720p), edram_alias 17.1 (it was 30.0), 640:
4.7, copies 2.9, edram_import 3.3, edram_import9 2.2. The valid comparison is left for the session with the right
picture.

**The eighth build on the console** (`out/switch_logs/008/ds2_013.log`): right colours, that is, the whole-pixel viewport
fixed the table on NVK. But the player saw "ghosts": a second silhouette of the character shifted down and to the right,
and the scene looking doubled.

**Cause: the pixel position read by the pixel shaders.** The translator starts the position register with
`iPos.xy - 0.5`, in host pixels. The game's full-screen passes (the shadow/light mask `11EDE000`, among others) multiply
that position by the game's `1/size` to get the texture coordinate; at 540p the position only goes up to 960x540, the
coordinate up to 0.75, and the input comes out scaled up by 4/3 from the top left corner. sa2-nx does not cover this in
the render scaling (SA2 does not read the position). It was wrong on the PC too: the stage-by-stage comparison of the
540p and 720p dumps (`out/view/s540_a.png` / `s720_a.png`) shows stage 015 shifted, and the final 540p picture was
darker and without the steps in the corner. I had taken the 540p picture as right looking only at the reduced capture;
it was not.

**Fix** (`ds2_native_position_scale`, default `true`): the renderer already had a SPIR-V rewrite of the pixel position
for the raster grid (`me_fragcoord_xy_spirv.h`); it gained the `Scale` mode, which multiplies X and Y by `4/q` (and by
0.5 in X together with the grid). The scale goes into the pipeline key (`PipelineKey::fill`, bits 16–23, only with a
pixel shader and outside 720p), into the prewarm and into the module cache. A shader the rewrite does not cover keeps
host pixels and produces a warning, without rejecting the draw. On the PC at 540p: 8 modules rewritten in the first map,
no warning, stage 015 aligned and the final picture equivalent to 720p's.

**Performance at 540p on the console** (the same session; 10-s windows): from 9.6 to 17.9 frames a second depending on
the camera's direction, GPU from 55.8 to 104.5 ms a frame. Looking at the firekeepers' house: scene 34.9 ms, 640
targets: 13.7, **edram_import 29.8** (against ~4 in the opposite direction), edram_alias 17.3. That window's report:
`draw/depth-repr-switch=3919ops/297744tiles`, `whole-target runs: depth-repr-switch=844` in ~96 frames, that is, ~9
whole-target depth conversions a frame from a change of representation. It is the next target.

## Round 4: the shadow atlas clears in depth-only mode (2026-10-10)

**The ninth build on the console** (`out/switch_logs/009/ds2_014.log`, 540p, no overclock): the picture right according
to the player (the ghost went away), 8 shaders rewritten, no warning. Two steady states at the first bonfire:

| direction | frames a second | GPU a frame | scene | 640 | edram_import | edram_alias |
|---|---|---|---|---|---|---|
| the exit (the three fog walls) | 14.1–14.5 | 69–71 ms | 23.0 | 5.1 | 3.3 | 17.1 |
| the firekeepers' house | 8.4 | 119.2 ms | 32.3 | 10.7 | **50.1** | 17.1 |

In the house view: `draw/depth-repr-switch=7392ops/355152tiles` in 84 frames (88 depth conversions a frame, 13 of them
of the whole target).

**What they are.** Reproduced on the PC by turning the camera (`tools/live.py pad "rright ..."`) with
`ds2_native_edram4_pair_report`. In that view there are lights with their own shadow: the game uses a depth atlas in
EDRAM (base 0, pitch 1040, D24FS8) and, for each shadow map (regions of 128 or 256 pixels), does Direct3D's Clear in
three parts: the block-aligned core through the aliased 4x surface (pitch 520), the edges as ordinary 1x rectangles, and
only then draws the objects and copies the region to the texture `1326C000`. The same goes for the sun's cascades (pitch
800, D24S8), which use the same blocks before.

The clear shortcut (round 2) did not catch those Clears: the new `ds2_diag_rect_proof` report showed `estimated,
overwrite not proven` for the 3-vertex rectangle on surface `08020208`. The game draws those clears in the depth-only
EDRAM mode (`RB_MODECONTROL` 5) with the pixel shader left loaded from the previous draw, and in that view it has a
`kill`; the proof of a full overwrite requires a shader with no discard and no alpha test. Without the proof, the
renderer imported the whole atlas into the aliased view, drew the clear there and then brought the blocks back to the
single-sample view piece by piece, at every clipped draw of every map: on the PC, 160–190 conversions and ~6,000 blocks
a frame.

**Fix** (`ds2_native_edram4_overwrite_depth_only`, default `true`, in `ProveFullOverwrite`): in mode 5 no pixel shader
runs (the renderer itself draws with no fragment stage), so discard, alpha test, alpha-to-coverage and depth export of
the loaded shader do not enter the proof.

**On the PC, the same view:** no conversion from a change of representation; ~25 small imports a frame remain (32
blocks) from the previous owner of another format (`D000/f0:800x800 → D000/f1:1040x1040`), in the edges' partial blocks
(`partial tile: owner unsupported`; the edges come in one draw of two rectangles, which does not become a redirected
clear). The shadow atlas and the final picture checked by eye in a traced frame (regions with scene depth and a cleared
background); there was no bit-for-bit A/B comparison.

**The tenth build on the console** (`out/switch_logs/010/ds2_016.log`, 540p, no overclock, the firekeepers' house):

| | frames a second | GPU a frame | edram_import | ring thread a frame |
|---|---|---|---|---|
| ninth build | 8.4 | 119.2 ms | 50.1 ms | — |
| tenth build | 11.2–12.0 | 84.5–89.2 ms | 1.2 ms | 75.4 ms (1,169 draws) |

No conversion from a change of representation (`depth-repr-switch` left the report); ~48 small imports a frame remain
(`draw/depth-depth=5635ops/9200tiles` in 116 frames) and the edges' refusals `depth not proven constant` and `partial
tile: owner unsupported` (3 a frame each). Of the 86 ms of GPU, 17.9 are `gap` (the GPU idle): in that view the limit
became the **CPU**, with the ring thread at 75 ms a frame (64 µs a draw). In the exit view: 14.6 frames a second, 68.1 ms
of GPU, the ring at 47.5 ms. The shadows have not been checked by the player yet.

## A system crash during a session: pcv out of session memory (2026-10-10)

2.5 minutes into session `ds2_016` the console showed Atmosphère's error screen. Reports from the same second (copies
in `out/switch_logs/010/`):

| process | result | meaning |
|---|---|---|
| `pcv` (010000000000001A), fatal | 2011-0102 | sf/hipc: out of session memory |
| `ptm`, `Bus` | 2011-0301 | session closed (a consequence of pcv's crash) |
| `hoc:clk` (00FF0000636C6BFF) | 2345-0048 | the console's clock sysmodule (Horizon OC), right after |

The game's log goes on normally until 16:55:19.7 (the console's clock); the failure is at 16:55:20. The clocks were at
default (CPU 1020, GPU 460.8, memory 1331.2 MHz), SoC at 37 °C. There is no report from our process.

What is known: `pcv` (the clock and voltage service) ran out of memory for sessions. Its clients on that console: the
clock sysmodule, the status bar (overlay) and our process, which through code inherited from sa2-nx kept **three
`clkrst` sessions open** for the whole session (CPU, GPU and memory, read every second for the `clocks:` line of
`rex_profile.log`). What is not known: who actually exhausted the sessions. It happened once in ~16 sessions with the
same code.

Change (`sdk/src/ui/switch_perf.cpp`, marked `ds2-nx`): the process no longer keeps any `clkrst` session; the clocks are
read in the first report and once a minute, opening and closing the sessions on the spot. If the crash comes back with
this build, the next step is testing without the overlay and without the clock sysmodule to isolate it.

## The eleventh build on the console: right shadows, no crash, and the CPU as the limit (2026-10-10)

`out/switch_logs/011/ds2_017.log` (7 min, 540p, no overclock): a few minutes at the bonfire, back to the firekeepers'
house, the first fog wall, enemies, up to the trade nest. The player compared the shadows side by side with a video of
the original game and found no difference. No system crash, no rejected draw or copy, 386 pipelines. The console's save
(`logs/ds2`) was copied to the PC (the earlier one is in `out/pc_save_backup2`).

| stretch | frames a second | GPU a frame (with `gap`) | `gap` | ring thread | draws |
|---|---|---|---|---|---|
| bonfire, standing still | 18.6–19.2 | 52–54 ms | 5.7 | 36.5 ms | 625 |
| tutorial (fog wall, enemies) | 12.3–16.5 | 61–81 ms | 13–34 | 32–50 ms | 600–960 |

`edram_alias` stays fixed at 17.2 ms a frame and `edram_import` at 0.1–0.2.

**The limit is now the whole CPU, not one thread.** In the tutorial the GPU sits idle 20 to 34 ms a frame (`gap`) with
the ring thread at 32–50 ms: neither the GPU nor the ring accounts for it alone. The `rex_profile.log` of the same
stretch: total CPU of 266–287 % with the process limited to 3 cores (mask 0x7), that is, almost everything busy:

| thread | CPU |
|---|---|
| GPU ring native (the renderer) | 66 % |
| Main XThread (the game) | 62 % |
| XThread648EB540 (game) | 48 % |
| an unnamed thread on core 0, priority 0x2C | 20 % |
| MtTaskWorkerThread x3 (game) | 18 + 13 + 10 % |
| DrawCommandThread (game) | 15 % |
| XMA Decoder | 13 % |

The stacks were not sampled in that session (`switch_profile_stacks` off), so it is not known yet in which functions
that time goes (useful recompiled code, the game's busy waits, or the renderer).

**The next CPU steps, in order:**

1. a profile with stacks on the console (only `ds2.toml` changes; the ELF of the build in use is in
   `out/switch_elf/b08b0f7/`);
2. whatever the profile shows in the renderer (60 µs a draw) and in the game's threads;
3. LTO and PGO, which this project's Switch build does not have yet (`DS2_SWITCH_LTO_JOBS` is passed by the script and
   ignored by CMake). In sa2-nx PGO gave ~15 % over LTO. The cost on this machine: the full compile of the recompiled
   code took ~100 minutes; an LTO link of the whole program redoes that work at every change in the renderer, which
   does not fit the cycle of rounds. The planned way: optimise the recompiled code once (it does not change between
   rounds) and keep the renderer out of LTO.

## A CPU profile with stacks on the console and the deko3d question (2026-10-10)

`out/switch_logs/012/` (session `ds2_018`, 540p, no overclock, with `switch_profile_stacks`; the build's ELF in
`out/switch_elf/b08b0f7/`): 1 minute at the bonfire, the rest of the tutorial up to the Majula bonfire. With sampling on
the CPU sits at 297 % of 300 % even standing still. Report: `python tools/switch_profile_report.py
out/switch_logs/012/rex_profile.log --elf out/switch_elf/b08b0f7/ds2.elf --max-fps 30` (output in `report.txt`).

**Where the CPU goes** (16,462 samples while running, 35 intervals below 30 frames a second):

| thread | samples | share |
|---|---|---|
| Main XThread (game) | 4,024 | 24.4 % |
| GPU ring native (the renderer) | 3,452 | 21.0 % |
| MtTaskWorkerThread (game) | 2,875 | 17.5 % |
| the game's XThread (B11F4AC0) | 2,510 | 15.2 % |
| DrawCommandThread (game) | 917 | 5.6 % |
| the "?" thread on core 0: the deferred recording, where the Vulkan (NVK) calls run | 603 | 3.7 % |
| XMA Decoder | 403 | 2.4 % |
| the rest (16 threads) | ~1,680 | ~10 % |

The game's recompiled code adds up to about two thirds. On the main thread the profile is flat (the hottest function has
1.5 %; by the stacks, `sub_82DACC38` 15 % and `sub_82DBB028` 9 % inclusive): no busy wait dominates.

**The ring thread** (3,452 samples):

| function | share |
|---|---|
| `DrawsVulkanImpl::Draw` (self time) | 36.4 % |
| `CopyVertices` | 8.1 % |
| `memmove` | 5.9 % |
| `WriteConstantRunRaw`, `armDCacheClean_L0` | 3.0 % each |
| `Type3`, `_free_r`, fingerprints, `RexGmQueryProtect`, shader identity, `PublishEDRAM4`... | 2.5 % or less each |

Inside `Draw`, 969 of the 1,257 samples (77 %) fall in 0x90 bytes of code: the general index conversion loop (`rev16`,
adding the base, minimum/maximum, testing for the restart value, one index at a time, 32-bit output). The game draws
almost everything as 16-bit triangle strips with primitive restart at 0xFFFF (`index layout seen: 16 bit, endian 1,
restart true, type 6`), and the inherited fast path (NEON, a per-frame cache, 16-bit submission) only accepted indices
without restart. That is 28 % of the ring thread, ~6 % of all the CPU.

**Fix** (`ds2_native_indices16_restart`, default `true`): 0xFFFF is also Vulkan's restart value for 16-bit indices, so
those strips go through the fast path as they are; only the largest index is recomputed without the restart values. On
the PC the geometry comes out the same (a capture at the trade nest), with no new validation error.

**deko3d** (nfsmw-nx's documents, branch `deko3d-implementation`: `docs/HowToDeko3d.md` and `docs/deko3d-devlog.md`).
What they report: in NFS MW the ring thread spent ~9 µs a draw, ~3.4 µs inside NVK (pipeline, descriptor sets, draw),
and deko3d removes that cost and the pipeline creation stalls; it reached 35.2 frames a second at 960x540 against 26–29
for NVK in the same stretch. The cost: a new backend (~13 thousand lines there), a shader path HLSL → SPIR-V →
SPIRV-Cross → GLSL → uam → DKSH with changes to the translator, none of it runs on the PC (only on the console), and in
the first comparison deko3d needed almost twice the GPU until the NVK path's savings were ported (35–43 ms against
19–24).

Here the arithmetic is different:

- the driver (NVK and Vulkan's common code) runs on the deferred recording thread and adds up to **3.7 % of the CPU**;
  that is the ceiling of what deko3d removes directly, plus a small part of the ring thread (`PipelineFor` 1.0 %,
  queuing ~1 %);
- the ring thread spends ~60 µs a draw, six times NFS MW's total, and most of it is our own code independent of the API
  (indices, vertices, constants, EDRAM);
- two thirds of the CPU are the recompiled game, which deko3d does not touch;
- this game's renderer depends on the EDRAM logic (aliased views, conversions by fragment and compute shaders, ZCULL
  with its own patch), all written against Vulkan and checked on the PC with lavapipe.

Conclusion recorded: deko3d is not the next step for this game; it stays as an option for when the renderer and the
game's code are lean and the driver starts to weigh. The current order: indices (done), vertices/`memmove`, constants,
then PGO/LTO of the recompiled code.

## The twelfth build on the console: cheaper indices and the first unregistered function (2026-10-10)

`out/switch_logs/013/ds2_019.log` (4.5 min, 540p, no overclock): from Majula to the Forest of Fallen Giants, where the
game closed and went back to the console's menu shortly before the first bonfire. No wrong geometry reported, no
rejected draw.

**Round 5 measured.** The ring thread fell from 58–60 to 39–45 µs a draw (the log's `ring thread:`); the index cache
now hits: `ME index cache: 600000 hits, 1845 MB neither converted nor copied` in the session.

**The crash.** `logs/rex/rex_crash.log` is an `abort()` of ours, and the last line of the game's log says why:

    [FATAL] Call to invalid or unregistered function at guest address 0x82C883C8

It is a game function the recompiler did not discover: `tools/find_gaps.py --check 0x82C883C8` shows the address in a
48-byte gap between `sub_82C883A0` and `sub_82C883F8`. The gap has two 6-instruction thunks (`lwz r3,0(r4); mr r4,r5;
lwz r11,0(r3); lwz r10,36(r11); mtctr r10; bctr` at 0x82C883C8 and its sibling at 0x82C883E0, vtable slot 40), of the
same family as the neighbours already registered. Nobody calls them with `bl` or stores them in data: the game builds
the addresses in code, at 0x82C88578–0x82C885C0, with six `lis` in a row and the `addi` up to 12 instructions later.
`tools/pointer_fixpoint.py` looks for that pattern, but only looked 8 instructions after the `lis`: it caught the two
neighbours and missed these.

**Fix.** The window became 64 instructions (`BUILT_WINDOW`; the sequence still ends at a branch or when the register is
rewritten). The redone scan found 20 candidates and accepted exactly those two (the other 18 are the same ones rejected
before: fragments that read an undefined register, branches into another function, no code or no terminator). They went
into `app/overrides.toml`; the regenerated code changed only in `ds2_funcs.h`, `ds2_init.cpp`, `ds2_register.cpp` and
the two files that received the functions (101 and 236), so the recompile took minutes.

A gotcha of this machine: `tools/codegen.ps1` calls `python`, which here is the Windows Store shortcut, and the
`tools/post_codegen.py` step fails; run it by hand with the real Python after every codegen.

**What else of the same kind there may be.** The gap report lists 902 stretches of 8 bytes or more with no function (343
of 8, 118 of 12, 214 of 16–63, 227 of 64–1023). Most are padding, data in the middle of the code or exception
funclets; the ones that are functions called only through a pointer built in a way the scan does not see will show up
like this crash, one at a time. The structural scan (`tools/gap_fixpoint.py`, which declares every gap that is a
complete leaf, with or without a reference) has not been run on this game yet; it may touch many generated files, so it
is left for the full recompile of PGO/LTO.

## A structural scan for unowned functions and the PGO/LTO preparation (2026-10-10/11)

The thirteenth build went past the point of the crash (`out/switch_logs/014/ds2_020.log`, no `FATAL`).

**The structural scan** (`tools/gap_fixpoint.py`, ported to Windows: it finds `rexglue.exe`, builds the pass's manifest
with relative paths, calls `tools/codegen.ps1` and then `tools/post_codegen.py` with its own interpreter; `--dry-run`
only counts). It declares as a function every gap of 8 to 1,024 bytes whose generated code is a complete leaf (it fits
in the gap, ends in `blr`/`bctr` or a tail branch, uses no stack or LR, reads no undefined register on entry), with or
without a known reference.

Two kinds of false gap showed up and are now discarded:

- `tools/find_gaps.py` counts one instruction per comment line; a function with a jump table inside the code (the
  table's words have no comment) looks shorter than it is, and its last bytes, the code of the cases, show as a gap.
  Declaring a function there would cut the switch off from its cases. Criterion: the gap contains a `loc_` label of the
  generated code. 33 candidates discarded (an example: `sub_8328FA30`, a 17-entry table at 0x8328FA58);
- an address table read as instructions: half or more of the words are aligned addresses inside the code.

Result: pass 1, 901 new gaps, 365 accepted; pass 2, 4 new, 3 accepted; pass 3, fixed point. **368 new functions** in
`app/overrides.toml` (710 entries in all): 226 are a lone `blr` (empty functions), 111 end in `bctr` (forwarding through
a vtable, like the two of the crash), the rest are getters, setters and thunks. `tools/switch_check.py` after the
regeneration: 0 truncated tables, 0 wrong targets, 0 lost. 220 generated files changed (about 110 compile units).

A known limit: the scan only accepts leaves. An unowned function that uses the stack, or whose address is computed in a
way none of the scans sees, would still show up as `Call to invalid or unregistered function`.

**PGO/LTO, what was put together** (from `sa2-nx`):

- `app/src/ds2_pgo.cpp`: in the instrumented build it writes the counts to `pgo/ds2_pgo_<N>.stream` next to the NRO,
  every 2 minutes, on losing focus (the HOME menu) and on exit; `Ds2App` calls `Start`/`DumpNow`/`Stop`;
- `app/CMakeLists.txt`: `DS2_SWITCH_LTO` (off by default), `DS2_SWITCH_LTO_JOBS`, `DS2_SWITCH_IPA_PTA`, `DS2_PGO`
  (`generate`/`use`), `DS2_PGO_DIR`. Before, the script passed `DS2_SWITCH_LTO_JOBS` and CMake ignored it;
- `tools/win/build_switch.ps1`: `-BuildDir` and `-Lto`. Each kind of build has its own folder, because changing a
  folder's flags recompiles the whole game: `out\nx` (the everyday one), `out\nx_pgo` (instrumented), `out\nx_rel`
  (optimised). The profile matches across folders thanks to `-fprofile-prefix-path`;
- `tools/win/pgo_merge.ps1`: finds devkitPro like the build script and accepts `-BuildDir` and `-Python`.

About LTO in this game: in the classic codegen the calls between the game's functions go through weak symbols (`sub_X`
is a weak alias of `__imp__sub_X`), so the gain from cross-file inlining may be small; in sa2-nx it comes together with
the "optimised" codegen (direct calls), which has not been done here yet. It gets measured, not presumed.

**Long builds on this machine** (i5-3210M, 2 cores / 4 threads, `-j4`; `out/build_switch_all.log`):

| build | folder | time | NRO |
|---|---|---|---|
| the everyday one after the scan (210 steps) | `out\nx` | 39.7 min | 105.6 MB |
| instrumented (`-Pgo generate`), from scratch, with the SDK | `out\nx_pgo` | 115.8 min | 132.1 MB |
| LTO without PGO, from scratch | `out\nx_rel` | 122.5 min (compiling ~48, link ~74 with 2 LTRANS) | 117.8 MB |

The LTO link uses 512 partitions and ~120 MB per `lto1`; 4 in parallel fit. `tools/check_switch_build.py` passes on all
three (in the instrumented one two endless loops of the game itself showed up that the normal build compiles another
way: `sub_8285E1D0` and `sub_82B482D8`, noted in `KNOWN_HALTS`).

**The fourteenth build on the console** (`out/switch_logs/015/`): `ds2_021` with `ds2.nro` (7.5 min, Majula and the
forest) and `ds2_022` with `ds2-pgo.nro` (6.5 min). No `FATAL`, no rejected draw. With `ds2.nro`: a mean of 11.7 frames
a second over 39 windows, with 42.7 ms of idle GPU a frame (`gap`) and total CPU of 279–280 %: the game is CPU-bound.
The profile: `ds2_pgo_1.stream`, 6.4 MB → 354 `.gcda` in `pgo\` (outside git; `sum_max` 1,629,423,073).

The "white aura" in the photo sent is the game's lock-on reticle (the bright dot over the locked enemy).

**Switching resolution live** (`ds2_render_height_combo`, on): on the Switch, L3 + R3 for a second toggles the render
height of the current mode between 540 and 720 (`ds2::render::ToggleHeight`; the pad is read through its own
`PadState` in `OnPresented`). The renderer already rebuilt the images when the scale changes (sa2-nx's dock/undock);
here it had never been exercised. On the PC (`ds2_render_test_height_at`): 540 → 720 in the menus and 720 → 540 in the
middle of the forest, the picture right in both (`out/pc/captures/t720.png`, `t540.png`), no new validation error.

**The PGO + LTO build** (`out\nx_rel`, `-Lto -Pgo use -LtoJobs 4`, with the profile of a 6.5-min session): 84.6 min (the
SDK was already compiled; the link with 4 LTRANS is much shorter than with 2), an NRO of 121.3 MB, `.text` 119.2 MB
against 115.7 for LTO alone and 103.5 for the normal build. Checked that the profile is read: the command for
`ds2_recomp.7.cpp` redone with `-Wmissing-profile -Wcoverage-mismatch` warns about nothing. `tools/check_switch_build.py`
passes (3 known endless loops, no vector fmla; the scalar fmadd go up from 10,499 to 13,650, copies of the game's own
`std::fma` through inlining). The ELF in `out/switch_elf/pgo_lto_1/`; the LTO-only one in `out/switch_elf/lto_only/`.
Neither has run on the console yet.

## PGO + LTO measured on the console, and 540p against 720p (2026-10-11)

`out/switch_logs/016/`: `ds2_025` with `ds2-opt.nro` (20 min, a new save) and `ds2_026` with `ds2.nro` (8 min), no
overclock, no crash and no anomaly reported. At three still points (the thicket at the start, inside the firekeepers'
house, the view of Majula on leaving Things Betwixt) the player stayed ~30 s at 540p, switched with L3 + R3 and stayed
~50 s at 720p. The live switch worked on the console (6 switches per session in the log). Whole 10-s windows inside each
stretch:

| point | resolution | `ds2.nro` | `ds2-opt.nro` | gain |
|---|---|---|---|---|
| 1 thicket | 540p | 15.5 | 16.2 | +5 % |
| 1 thicket | 720p | 11.8 | 11.8 | 0 |
| 2 house | 540p | 15.8 | 19.0 | +20 % |
| 2 house | 720p | 12.2 | 13.0 | +7 % |
| 3 Majula view | 540p | 15.3 | 17.1 | +12 % |
| 3 Majula view | 720p | 12.5 | 12.7 | +2 % |

(frames a second). Where the ring thread is not waiting for the GPU, the cost per draw fell from 44.8–48.6 to 41.7–41.8
µs.

**What limits in each case.** At 720p the GPU has no slack (2–3 ms idle a frame) in both builds: the CPU gain does not
show. At 540p `ds2-opt.nro` at point 1 also hits the GPU (1.8 ms idle; the scene costs 32 ms there, with the brush). At
points 2 and 3 the normal build left the GPU idle 7.5 and 11.6 ms, and the optimised one makes use of it.

**540p against 720p** (the same build, the same point): +31 % to +46 % frames a second at 540p. By GPU category, 720p →
540p: `edram_alias` 24.6–30.4 → 14.1–17.4 ms, scene 30–37 → 24–32 ms.

**The next GPU target: `edram_alias`.** It is 14–17 ms a frame at 540p (a quarter of the frame) and 25–30 at 720p, fixed
at every point. By round 4's per-pass times, the category is dominated by passes whose first draw uses `VS n3` with
`PS n720` and `PS n184` on the main target's aliased 4x surface: the game's full-screen draws done per sample (they are
not Clears), followed by the conversion of the whole target to the single-sample view. Not investigated yet.

## Mesa 26.2.4 (2026-10-11)

The player left `mesa-26.2.4-switch-unified-horizon-sdk.zip` in the project's root (an already compiled SDK, the same
layout as ours: `opt/devkitpro/portlibs/switch`). Extracted in `_tools/mesa-26.2.4-sdk/`. Comparing the text strings of
the two `libvulkan.a`:

| | current (`_tools/mesa-switch`) | zip |
|---|---|---|
| version | Mesa 26.2.1 (git-1a8c1a66d6) + nfsmw-nx's patch | Mesa 26.2.4 (git-606441c90e) |
| ZCULL (a plane per depth image) | yes (`ZCULL: plano de ...`) | no (only the register names) |
| `NVK_SWITCH_DIBUJO` (a cheaper draw path, per-draw measurement) | yes | no |
| `NVK_SWITCH_DYN_UBO_DELTA` (set 4 by differences) | yes | no |
| `NVK_COPY_ENGINE`, `NVK_SUBTILING_KNOB`, `NVK_SHADER_STATS` | yes | no |
| `NVK_SWITCH_CPU_WRITE_MEM_UNCACHED`, `_MAPPED_COMPLETION`, `_NO_UBO_CBUF`, `_PERF_LOG` | yes | yes |

That is, it is the newer mesa-switch without StevensND's driver changes, which are the ones this renderer (via sa2-nx)
uses; nfsmw-nx's `mesa/` stays at 26.2.1. ZCULL alone was worth ~8 % of GPU here (137.9 → 127.2 ms). **Linking against
the zip's SDK fails** (`out/nx_build_mesa264.log`): thousands of undefined references to functions of Rust's base
library (`core::panicking::panic`, `panic_fmt`, `panic_bounds_check`, `core::fmt::write`,
`alloc::alloc::handle_alloc_error`...), coming from `nil` and `nak_rs`. The zip's `libvulkan.a` only carries the generic
instances of `core` (crate `CsR2NHd66MWo`), not the library itself; ours (crate `CslpfLnFkgLmC`, from another Rust
compiler) does. Without the `libcore` of the same `rustc` that produced the zip there is no way to link, and this PC's
is of another version. The everyday build went back to the current SDK (`-Reconfigure` with the old `NVK_SDK`; it only
relinks).

To really have 26.2.4 the way is compiling mesa-switch here at commit `606441c90e` with nfsmw-nx's patch ported (it is
against `1a8c1a66d6f`, 35 files). Not done: there is no measurement that 26.2.4 brings a gain for this game, and without
the patch it loses ZCULL.

## UnleashedRecomp-NX and MarathonRecomp-NX: what is useful for this port (2026-10-11)

Cloned in `_ref/` (outside git). The documents read: `UnleashedRecomp-NX/docs/SWITCH-PERFORMANCE.md` (10 rounds, with
the result of each test set) and `MarathonRecomp-NX/docs/SWITCH-OPTIMISATIONS.md` (the full catalogue, with why each
change is exact). They are XenonRecomp ports with their own renderer (plume), so the renderer part does not transplant;
the **game code generation** and **runtime** parts do, and that is where our CPU goes (two thirds in the recompiled
code).

**The central point.** The generator emits every game function as a weak, `noinline` alias (so the hooks can replace
it), and every call goes through that alias: GCC cannot inline any of them, with or without LTO. Their "direct calls"
step makes the calls between functions without a hook go straight to `__imp__sub_X` (Unleashed: 365,513 calls to 41,315
functions). It is the same step sa2-nx has in `tools/direct_calls.py`, inside the "optimised" codegen, which this port
does not use yet (we are on "classic"). That explains our PGO + LTO's modest gain.

**We already have, the same as them:** LTO, PGO (collection in a session, `-fprofile-update=single`,
`-fprofile-partial-training`), `-fipa-pta`, depth targets without `TRANSFER_DST` for ZCULL, a pipeline cache, the render
thread at a non-sliced priority, constants through a UBO, depth-only draws with no fragment stage, the GPU clock
corrected (1.627), CPU/GPU profiles.

**Going into this build (it does not invalidate the collected profile):** `-fno-math-errno -fno-trapping-math` on the
recompiled code of the optimised builds (`DS2_SWITCH_MATH_FLAGS`; 464 `sqrt()` in the generated code, each with a test
and a branch to libm). Their `-fomit-frame-pointer` was left out for now: `rex_crash.log` and the stack sampler walk the
frame chain through the game's code, and we are still hunting errors.

**The next CPU stage, in the order of what they measured** (all of this changes the generated code, so it asks for a
full recompile and **a new PGO collection**; sa2-nx already brings the generator's options and the audit tools):

| change | theirs | here |
|---|---|---|
| direct calls | Unleashed R1-3; Marathon 151,618 calls | `tools/direct_calls.py` (DS2's list of hooks to check) |
| the game's memory without `volatile` + loop barriers | Unleashed R8; masseffect-nx +15-20 % | `pch_no_volatile.py` + `loop_barriers.py` (mandatory together) |
| CR, CTR, XER and the reservation as locals; no MSR | both | `cr_as_local`, `ctr_as_local`, `xer_as_local`, `reserved_as_local`, `skip_msr` |
| non-volatile registers as locals, without the `__savegprlr`/`__restgprlr` calls | masseffect-nx -10 % on the main thread | `non_volatile_as_local`, `elide_lr_stores` (here the helpers are 2-3 % of the samples) |
| floating-point moves without switching the FPCR's mode | sa2-nx, Marathon | `fp_moves_modeless` |
| scratch registers as locals | sa2-nx | `non_argument_as_local` + a per-game list (`tools/non_argument_audit.py`) |
| the game's atomics without a full barrier | both (`SwitchRelaxedAtomics`) | to be checked in ReXGlue |
| critical sections and events with no system call when nobody waits; a short spin before sleeping | both (turned on in Marathon after an audio test) | in the SDK's kernel; it touches audio, A/B mandatory |
| the engine's threads off core 0 | Marathon (not the sound one: it caused audio glitches) | A/B |
| native functions in place of the hottest ones (RTTI, `dynamic_cast`, CRT, matrices) | both | depends on DS2's per-function profile |

Per-game requirements before turning on the "optimised" codegen (from sa2-nx's `perf_overrides.toml`):
`tools/read_before_write.py` and `tools/non_argument_audit.py` on the generated code, the setjmp/longjmp callers, the
exception funclets, and the check on the PC (verify mode and a lockstep capture against classic).

**On the GPU**, what they did in the shader translator that ours (the same origin, XenosRecomp) does not have yet: an
early exit for the alpha test (the discarded pixels skip the arithmetic between the alpha output and the test; 646 of
1,096 shaders in Unleashed) and "sinking" the shadow reads to after the test; `gather` in the shadow filters; vertex
outputs the pixel shader does not read not computed. Here the scene costs 24–32 ms at 540p and the thicket at the start
is the worst point, so the alpha test's early exit is the first candidate. It requires regenerating the shader library
and checking the picture.

**`edram_alias`, what it is.** The log's count (`color-resolve/color->color`, whole-target runs) and the PC's pair
report point to whole-target format conversions: the game draws the scene into a 7e3 target (format 3, RGBA16F on the
host) and copies it declaring the source as format 2 (2_10_10_10), that is, it reads the HDR's raw bits; the renderer
converts the whole target from float to the packed 7e3 bits at every such copy, and back when the game draws into it
again (`C000/f3:1280x1280 -> C000/f2` and the reverse, 720 blocks each). That is 4–6 a frame. To investigate: if the
resulting texture is read as 7e3 again, the two conversions cancel out and the float image could be handed over
directly.

## Round 6: a full PGO profile and format conversions as a fragment pass (2026-10-11)

**The profile.** `out/switch_logs/017/pgo/`: `ds2_pgo_1.stream` (6.5 min) and `ds2_pgo_2.stream` (27 min, from the start
to the Last Giant, session `ds2_027`, no `FATAL`). Merged from scratch in `pgo\` (session 1's is in `out/pgo_session1`):
354 `.gcda`, `runs = 2`, the largest arc count 8,220,895,832. Build `out\nx_rel` (`-Lto -Pgo use`, plus `-fno-math-errno
-fno-trapping-math`): 62.9 min after resuming (the notebook froze in the middle of the first: I was running a game
session on the PC together with the compile; one thing at a time from here on). An NRO of 120.8 MB,
`tools/check_switch_build.py` passes. The ELF in `out/switch_elf/pgo_lto_2/`.

**`edram_alias`, what it really was.** The "GPU time by pass" list showed `[cat 12 VS3 PS720]` and `[cat 12 VS3 PS184]`:
they are not game shaders, they are the conversions (the "PS" there is the number of blocks: 720 = the 1280x720
target, 184 = the 640x360 one). Per frame, on the console: 1 whole-target conversion (720 blocks) in a colour copy and
4–5 small ones in draws; on the PC, the pair report shows `C000/f3 -> C000/f2` (the 7e3 target read as 10 bits, the
HDR's raw bits) twice a frame and `C000/f0 -> C000/f3` once. They all ran as a **compute shader**: sa2-nx has the same
work as a fragment pass (`ds2_native_conversion_frag`, "same words bit for bit", with no storage image traffic or 3D
<-> compute engine switch), off by default.

Turned on on the PC (540p, Majula, with the console's save): `color conversions as fragment passes: 6 done, 0
declined` per window, no new validation error, the picture right (`out/pc/captures/frag1.png`) and the HDR chain with
coherent values in the frame dump (`12366000`, `117AC000`). It goes on in the package's `ds2.toml`;
`configs/ds2_teste_conversao_compute.toml` goes back to the old path. Only the console measures the gain (the log's
`edram_alias` category).

**Wrong thumbnails in the save list (open).** A photo from the console: of the three portraits on the "Data List"
screen, the middle one comes out right and the other two are noise. The slot files (`USER_DATA01`..`0a`, 111,308 bytes)
hold no image: those of the first two saves have the same structure (data, zeros, data) and the third has high-entropy
content with pointers and floats, which needs checking (the game loads all three normally). In the console's log, on
opening the menu the game creates four 1280x720 copy textures (`158B4000`, `15C4C000`, `15FE4000`, `1551C000`):
portrait candidates. On the PC the list shows for a few seconds before the game loads the save and the portraits are
not drawn in that time, so it was not reproduced. The cheap next step: open the list on the console with
`configs/ds2_melhorias_desligadas.toml`; if the portraits come out right, it is one of the inherited copy shortcuts.

## Round 6 measured on the console; round 7: the save portraits and the texel-for-texel conversion (2026-10-11)

**The sixteenth build on the console** (`out/switch_logs/019/ds2_029.log`, `ds2-opt.nro` with the profile of both
collections and `ds2_native_conversion_frag`; no `FATAL`; `302 done, 0 declined` per window; 674 rejected draws, all in
the save list, which was only seen in the next round: see "Round 7 measured"). At the three points, against session
`ds2_025` (the previous optimised build, conversions by compute):

| point | resolution | frames a second before → now | `edram_alias` before → now | ring µs/draw |
|---|---|---|---|---|
| 1 thicket | 540p | 16.2 → 16.5 | 14.1 → 10.7 ms | 37.3 |
| 1 thicket | 720p | 11.8 → 12.6 | 24.6 → 18.6 ms | |
| 2 house (another position: 1,209 draws against 824) | 540p | 19.0 → 15.1 | 14.4 → 10.9 ms | 38.5 |
| 2 house | 720p | 13.0 → 11.8 | 25.1 → 18.8 ms | |
| 3 Majula view | 540p | 17.1 → 16.9 | 17.4 → 13.2 ms | 38.8 |
| 3 Majula view | 720p | 12.7 → 13.7 | 30.4 → 22.9 ms | |

Point 2 is not comparable (the camera was elsewhere). The fragment conversion takes off 3–4 ms at 540p and 6–7.5 ms at
720p; the bigger profile takes the ring thread from ~41.7 to ~38 µs a draw. What is left in `edram_alias` is almost all
one whole-target conversion a frame (`[cat 12 VS19 PS720]`, ~5 ms a frame at 540p): the shader
`me_edram_color_to_color.frag` computes each pixel's block with five integer divisions by values that are not
constants, and the GPU has no instruction for that.

**The texel-for-texel conversion** (`ds2_native_conversion_frag_identity`, default `true`): when the two views have the
same pitch in blocks, the same sample arrangement and the same first block, and the run is of whole rows (the scissor is
already exactly the run), each destination texel reads its own position; the shader skips the block arithmetic (the
constants' `source_64bpp = 2` field). On the PC, in Majula: `12 done (12 texel for texel)` per window, the picture and
the HDR chain right.

In the firekeepers' house `edram_import` showed again (6.6 ms a frame, `draw/depth-depth=10827ops` in 152 frames): they
are the edges of the shadow atlas clears with a partial block owned by another format, the known leftover of round 4.
It is left for later.

**The save list's portraits.** Photos from the console with four saves: with the normal `ds2.toml` only the portrait of
the save just played comes out right, the others stay empty and one is noise; with `ds2_melhorias_desligadas` none comes
out right. The log (`ds2_030`, `ds2_031`) shows what the game does on opening the list: for each save, ~33 draws into a
target copied to a 256x152 texture and one more draw copied to a **128x76** one; those 128x76 ones show in the inventory
as `0 reads *** NOBODY READS IT ***`, that is, no draw samples them as a copied texture: the game reads the pixels with
the CPU (for the interface). The renderer only writes copied textures back to the game's memory up to 4,096 texels
(`ds2_native_resolved_read_texels`, the exposure ones), and 128x76 is 9,728: the game read whatever was in memory
(zeros: an empty portrait; garbage: noise).

Fix (`ds2_native_resolved_read_rare_texels`, default 65,536): a copied texture bigger than the normal limit and up to
that size is also written to the game's memory when the copy is rare (the first, or none in the last 30 frames), and in
that case the work is submitted and waited for right there, so the texels are in memory before the ring goes on (on the
original console a texture lock waits for the GPU). The post-processing targets, copied every frame, only go through
the rule on the first copy (on the PC: 12 reads on entering the 3D, none after). The save list was not reproduced on the
PC ("Continue" loads the latest save directly), so the confirmation is the console's.

## Round 7 measured on the console; the save list's rejected draw (2026-10-11)

**The seventeenth build on the console** (`out/switch_logs/020/ds2_032.log`, `ds2-opt.nro`, 17:28–17:39, six resolution
switches, no `FATAL`; the four portraits show in the save list, a photo from the console). At the three points, against
session `ds2_029` (the sixteenth build):

| point | resolution | frames a second before → now | `edram_alias` before → now | idle GPU a frame |
|---|---|---|---|---|
| 1 thicket | 540p | 16.5 → 16.6 | 10.7 → 3.1 ms | 11.7 ms |
| 1 thicket | 720p | 12.6 → 15.4 | 18.6 → 5.1 ms | |
| 2 house | 540p | 15.1 → 17.0 | 10.9 → 3.5 ms | 9.5 ms |
| 2 house | 720p | 11.8 → 15.4 | 18.8 → 5.6 ms | |
| 3 Majula view | 540p | 16.9 → 18.1 | 13.2 → 3.5 ms | 16.9 ms |
| 3 Majula view | 720p | 13.7 → 17.2 | 22.9 → 5.9 ms | |

The texel-for-texel conversion took 7–10 ms of GPU a frame off at 540p and 13–17 ms at 720p. At 720p that became 2.8 to
3.6 frames a second; at 540p almost nothing, because there the GPU already sits idle 9 to 17 ms a frame waiting for the
CPU (the ring: 36.6 to 42.3 µs a draw). The limit at 540p is the CPU again; the generated code is the next step.

**5,873 rejected draws in that session** (`resolved logical crop rejected 14AE9000 128x76: unproven logical fetch
layout`, cause 109), one a frame while the save list stays open. It is not new: sessions `ds2_028` to `ds2_031` have the
same (1,175, 674, 962 and 609), and I had written "no rejected draw" looking only at the game's windows. What happens:
the game copies each save's portrait to a 256x152 texture, reduces it to 128x76, reads the pixels with the CPU, frees
both and creates the texture the interface draws (128x76, filled by the CPU). The first save's one lands at address
`14AE9000`, where the 256x152 copy had been; the renderer still had the copy's entry at that address, saw a size that
is not a crop of it (a row pitch of 128 against 256) and dropped the draw. It is the third case of reused memory (the
others: linear fetch, compressed fetch).

Fix (`ds2_native_other_pitch_fetch_not_resolved`, default `true`): when all the other conditions of the contract hold
(one level, 2D, tiled, at the copy's base, a compatible format) and only the row pitch differs, the fetch's rows cannot
be the ones the copy wrote: the fetch is read from the game's memory, as an ordinary texture. One log line per address
(`another row pitch, so the memory was reused`).

**The save list on the PC.** The title menu is `New Game / Continue / Information / Go Online`; "Continue" opens the
list. The button script of `guided_session.ps1` presses START three times and A every 6 s, and the screens' timings vary
from one run to another, which is why the list "loaded directly": it was the script confirming. With `-Pad
"16-16.6:a,30-30.6:start,42-42.6:start"` and `-Extra "REX_DS2_DIAG_DRAW_EVERY=1"`, and from there by hand with
`tools/live.py` (START at the title, A at the "No Ethernet cable" and "Starting in offline mode" notices, A on
"Continue"), the list opens and stays. On lavapipe each portrait takes ~40 s (a single 37-s pass). Result
(`out/pc/captures/x6.png`): the four portraits, 8 rare reads, the different-row-pitch line for `14AE9000`, no rejected
draw.

**The slack of the rare copies rule** (`ds2_native_resolved_read_rare_frames`, default 8; it was a fixed 30). The game
draws the portraits 23 to 29 frames apart and reuses the addresses of the ones already freed (`14AE9000`, `15436000`,
`14AF5000`, `15436000`: on the console and on the PC the reuse was two portraits apart, 58 frames). With the limit at
30, a reuse one portrait apart would be left out and the portrait would come out empty.

## Optimised generated code (2026-10-11 and 12)

Stage 2 of the agreed order (the GPU's fixed cost, the generated code, alpha cut in the shaders). The "classic"
generated code keeps all of the PowerPC's state in the `ctx` structure, in memory; since any write to the game's memory
may, for the compiler, be a write to `ctx`, almost every register use becomes a memory read or write. sa2-nx's
"optimised" flavour changes how the C++ is written, without changing what the game computes. The new file
`app/perf_overrides.toml` turns on, for DS2:

- `cr_as_local`, `ctr_as_local`, `xer_as_local`, `reserved_as_local`: cr0-cr7, ctr, xer and the `lwarx` reservation as
  local variables;
- `skip_msr`, `dform_disp_split`, `fctiwz_inline`;
- `non_volatile_as_local`: r14-r31, f14-f31, v14-v31 and v64-v127 as locals (the `__savegprlr`/`__restgprlr` calls and
  those registers' stack traffic go away);
- `elide_lr_stores` and `fp_moves_modeless`;
- after the generator (`tools/post_codegen.py`): the game's memory without `volatile` (16 macros of `ds2_pch.h`),
  barriers at 51,794 loop heads (mandatory with the memory without `volatile`) and **217,593 direct calls** between
  functions without a hook (`tools/direct_calls.py`), which GCC can now inline.

A codegen takes 10 min here, plus 3 min for the following steps. `tools/codegen.ps1` now finds the real Python by itself
(this machine's `python` on the PATH is the Microsoft Store shortcut).

**`non_argument_as_local` was left out.** `tools/non_argument_audit.py` on DS2's classic code: 340 functions receive a
value in r0/r11/r12/f0/v32-v63 on entry (250 in r12; chains of functions that pass v33-v63 and f0 along), besides the
save/restore helpers, and 329 reads after a call of a value the called function may have written (277 in r11, after
`sub_82FB2578`, `sub_82FB2540` and `sub_8266B030`). In SA2 there were half a dozen. It needs per-function work first;
it is left for a round of its own.

**setjmp / longjmp.** DS2 has the CRT's pair: `sub_82FB8A30` (the program's only function with `mfcr`; 68 calls) and
`sub_82FB78D0` (433 calls: some library's error path). Translated as an ordinary call, a longjmp restores r1 from the
`jmp_buf` and then returns to its caller, with another function's stack. With `setjmp_address` and `longjmp_address` in
`overrides.toml` the generator emits the host's setjmp/longjmp, indexed by the `jmp_buf`'s address. The PGO profile of
the 33 minutes of play (I recompiled `ds2_recomp.46.cpp` and `.20` with `-fdump-ipa-profile-blocks` and the profile of
both collections) has the setjmp with 3,920 entries and all of the longjmp's arcs at zero: no longjmp has run on the
console so far. With the registers as locals, after a longjmp the setjmp caller's r14-r31 hold the value from the time
of the failure, not the one from the time of the setjmp (the same risk accepted in SA2).

**Reads before writes** (`tools/read_before_write.py`, 4.6 min): a function that reads a non-volatile register before
writing it expected another function's value, and with locals reads zero.

- With no seeds beyond C++'s `throw` (`sub_82FBF820`, 35 calls, calls `RaiseException`): 70 functions. 43 of them were
  paths after a call to `sub_8266B030` or `sub_82AB5F50`, two variadic "fatal error" printers: 23 functions *end* with a
  `bl` to one of them, with nothing after, so the original compiler knew they do not return. With the three as seeds:
  **27**.
- 8 are real, plus two more the tool lists as "write only":
  - **the WMV decoder** (`movie/prologue.wmv`, the opening movie) keeps constants in v64-v127 across C functions:
    `sub_8338E090` writes v64-v127 to a buffer and loads the 64 of another, `sub_8338DD48` does the same and reloads
    v64-v72, and seven hand-written leaves (`sub_831452E0`, `sub_83145918`, `sub_833A91A0`, `sub_833A9548`,
    `sub_833A9808`, `sub_833A9A70`, `sub_833AAB98`) read v64, v65, v69 or v72 without ever writing them. With locals
    the first two would load variables that vanish at `return` and the leaves would read zero. The nine stay
    `share_registers` (they use `ctx`); none calls a save/restore helper or writes r14-r31, and none of the functions
    that call them hands over a vector register (checked in the generated code), so `ctx.v64`-`v127` keep the
    decoder's constants as the real registers did;
  - `sub_829A3604`: ten instructions at the end of the heap function `sub_829A2DF0`, reached by its `beq`/`bne` (and by
    a pointer, from the old scan); it reads its owner's r31. `share_registers`.
- The other 19 stay as they are: normal functions, with a prologue that saves the register before using it, where some
  path reads a variable the original code had not initialised either (`sub_8235E600`, `sub_823C2660`, `sub_824CB840`,
  `sub_8259D220`, `sub_8259DA80`, `sub_82B63548`, `sub_82C02E10`, `sub_82C9F088`, `sub_82EF1290`, `sub_82F73DF0`,
  `sub_82F9B720`); idioms that do not depend on the value (`xor r30,r30,r30` in `sub_829982F0` and `sub_82998680`,
  `subfe rX,rY,rY` in `sub_83009D40` and `sub_830B12C8`); the game's setjmp, which is no longer called; `sub_82FD0C54`,
  a loose instruction nobody calls; and two motion compensation functions of the decoder (`sub_83146E08`,
  `sub_831477A8`) where v14-v24 are scratch and the reads are on paths the entry conditions exclude (the result is only
  used when `r9 != 0`, and the path only exists with `r9 == 0`).
- Who reads `ctx.lr`: only the Direct3D tracer (off) and the console profile's caller attribution. No hook of the app
  reads cr, xer, ctr or a non-volatile register.

**Checked on the PC** (`ds2_082`, `ds2_086`; the `[enh]` line says `optimised codegen (non-volatile locals, no lr
stores, modeless fp moves)`): the title, the notices, the save list with the four portraits, Majula in 3D (walking,
turning the camera, attacking, opening the menu; `out/pc/captures/p3.png` to `p7.png`), "New Game" and the opening
movie with a clean picture and subtitles (`z1.png` to `z4.png`: it is the test of the decoder's markings). No `FATAL`;
the self-checks' only difference is the one from before (`VS identity mismatch`, the same as on classic).

### Three stumbles of the PC tools, found on the way

1. **65,535 `MEM*.tmp` files in the temp folder.** The first session with the new code went 19 minutes without
   presenting a 3D frame, with three threads at 85 %. It was not the code: every new pipeline took exactly 46 s
   (`PipelineFor 45898152 us`), and the prewarm ones went from 0.35 s (at 19:00) to 1.6 s (20:50) and 48 s (23:20).
   `tools/win/sample_threads.py` (new, below) showed the driver's thread inside `NtCreateFile`, and its stack pointed at
   `%TEMP%\MEMxxxx.tmp`: Mesa on Windows creates a temporary file for every memory stream (here with `spec[0] =
   0x0000a320`), `GetTempFileName` only has 65,535 names, and the test sessions, stopped from outside, had left them all
   there over three days. Deleted (they were 2 MB), and `tools/win/boot_test.ps1` now gives each session its own temp
   folder (`out\pc\tmp`), emptied at the start: 456 pipelines prewarmed in 30 s. The PC timings measured on the
   afternoon of 10/11 (the 37 s per portrait on lavapipe, for example) were inflated by this.
2. **`guided_session.ps1 -Pad` never worked.** The internal variable `$pad` is the same as the `-Pad` parameter
   (PowerShell names are case-insensitive) and overwrote it; the default script (START three times, A every 6 s) always
   ran. It was what "loaded the save by itself" when I tried to stop at the list. Fixed.
3. **`tools/win/sample_threads.py`**: for a PC session that stopped moving, with no debugger installed (LLVM's
   `lldb.exe` here does not open: the Python 3.10 DLL is missing). It suspends each of the busiest threads for an
   instant, reads the instruction pointer and the return addresses at the top of the stack and names them with
   `llvm-symbolizer` and `ds2.pdb` (`--all-modules` also shows the DLLs).

**Switch builds with the optimised code** (2026-10-12, `out/build_switch_all.log`): `out\nx` in 58.1 min, an NRO of 80.8
MB (the classic one was 105.6 MB); `out\nx_pgo` (instrumented) in 83.8 min, 105.7 MB. `tools/check_switch_build.py`
passes on both: no branch to self in the recompiled code (the five known endless loops of classic went away too: with
the barriers GCC does not prove them endless), no vector multiply-add. The ELFs in `out/switch_elf/optimised_1/`. The
profile of the old collections (classic code) went to `out/pgo_classic`; `out\nx_rel` still has the old program
(classic, LTO + PGO), which goes in the package as `ds2-opt.nro` only for comparison until the new collection.

## The eighteenth build on the console: the new code works; the memory ran out at 28 minutes (2026-10-12)

**A normal session** (`out/switch_logs/021/ds2_033.log`, `ds2.nro` with the optimised code, no LTO/PGO; 20 min, 11.8
million draws, none rejected, no `FATAL`, the portraits right). At the three points, against the old `ds2-opt.nro`
(classic code **with** LTO + PGO, session `ds2_032`):

| point | 540p old → new | 720p old → new | ring µs/draw (540p) |
|---|---|---|---|
| 1 thicket | 16.6 → 19.5 | 15.4 → 15.3 | 36.6 → 51.1 |
| 2 house | 17.0 → 17.6 | 15.4 → 15.9 | 42.3 → 50.9 |
| 3 Majula view | 18.1 → 19.4 | 17.2 → 17.8 | 38.9 → 50.5 |

The optimised game code, on its own, already beats classic with LTO + PGO at 540p. The ring thread got slower because
this program has no LTO/PGO (the renderer is the same code as before); the next `ds2-opt.nro` puts the two together. At
720p the limit is still the GPU.

**The collection session** (`ds2_034.log`, `ds2-pgo.nro`, 12:31 to 12:59 on the PC's clock): at 28 minutes a "ghost"
picture, an orange screen on switching to 720p and the program closed (`rex_crash.log`: `abort()` coming from
`Memory::AccessViolationCallback`, an invalid access by the game). The order in the log:

- 12:55:01, :15, :28 (at 540p, without switching anything): `nvkmd-switch: allocate memory failed: out of host memory`;
- 12:59:13, L3 + R3 to 720p: `targets: out of GPU memory for a 1040x1040 target ... dropping the cache was not enough`
  (and 1600x1600, 1280x720): targets that do not exist, the orange screen;
- 12:59:26, back to 540p: the 540p targets do not fit either, targets at different scales are left over (`targets at
  different render scales (cause 41)`, 11,516 rejected draws): the ghost picture;
- 12:59:31: the game accesses invalid memory and the program closes.

It is not the generated code: it is **memory**. On the Switch NVK's GPU memory comes out of the process heap, and the
`C3 GPU memory` line gives what the driver thinks is still there: used 664 → 759 → 852 MB, "budget" 1,092 → 1,052 →
1,133 MB, that is, only ~280 MB free in the whole process at 12:58 (in the normal session, 299 MB at 13 min; in the
27-min classic collection, 386 MB at 15 min). What grows is the texture cache (151 → 252 → 330 MB, limit 512), as one
walks through the game; switching resolution needs ~100 MB more at once (targets of 172.6 MB at 540p, 273.4 MB at
720p). Any long session would get there, with either program.

Where the memory was: on Horizon the whole heap is committed memory, and the host stack of each game thread is a
**16 MB** heap block (`params.stack_size = 16_MiB` in `sdk/src/system/xthread.cpp`). DS2 keeps some 50 threads alive
(`stacks: 50 from the heap, 12 reused`; 61 threads in the profile report): ~800 MB of the process's ~3.2 GB in stacks
that use a few hundred KB each. In SA2, with few threads, this did not show.

Fix (`switch_guest_host_stack_mb`, the SDK's fourth `ds2-nx` exception; default 0 = automatic): the host stack becomes 16
times the thread's own guest stack, at least 4 MB and at most 16 MB (the main thread keeps 16 MB). The first 96 threads
created write the guest and host stacks to the log (`[threads] guest thread N: guest stack X KB, host stack Y MB`), to
tune later with numbers. The log also gains the process's memory (`process memory A of B MB; heap: ...`, from
`svcGetInfo` and `mallinfo`) next to the `C3 GPU memory` line and in the GPU out-of-memory message.

**The profile was saved.** The card's `pgo/` folder had three files: `ds2_pgo_1` and `_2` are the old collection's
(identical byte for byte to those in `out/switch_logs/017/pgo`, of the classic code: no use) and `ds2_pgo_3.stream` is
this session's last complete dump, written two minutes before the crash (the 5 MB `.tmp` is the dump that was being
written). Merged alone in `pgo\`: 354 `.gcda`, the largest arc count 9,065,595,789.

**A build stumble:** touching an SDK file makes the PC build relink the generator and run the codegen again, which
rewrites the 331 generated files (the final content is the same after the post-codegen steps, but the date changes and
every build recompiles everything; and the PC build itself fails once, because `ds2_pch.h` changes after the
precompiled header is made). I checked that two of the files compile to objects identical to those of `out\nx` and gave
the 331 their old dates back, so that `out\nx` only recompiles what changed.

**The nineteenth build** (2026-10-12): `out\nx` with the memory fix in 3.7 min (80.9 MB); `out\nx_rel` with the
optimised code, LTO and the new profile in 68.7 min (90.5 MB). `tools/check_switch_build.py` passes on both. The profile
matches the code (one file recompiled with `-fdump-ipa-profile-blocks`: counts read, no mismatch warning). The ELFs in
`out/switch_elf/optimised_2/`. A fact from the profile for later: the most executed arc of the whole program
(9,065,595,789 times in 26 min) is the wait loop of `sub_829B9C60`, the Direct3D function that spins waiting for the
GPU ring to move: the main thread burns CPU there while the renderer is behind.

## The nineteenth build on the console: memory solved; the achievement notice brings down the presenter (2026-10-12)

A 46-minute session with the new `ds2-opt.nro` (optimised code + LTO + PGO; `out/switch_logs/022/ds2_035.1.log` and
`ds2_035.log`, 15:06 to 15:56 on the PC's clock). No `FATAL`, no memory failure, 12 resolution switches.

**The three points** (before 21 minutes), against the old `ds2-opt.nro` (classic + LTO + PGO) and the eighteenth build's
`ds2.nro` (optimised without LTO/PGO):

| point | 540p: old → no LTO/PGO → new | 720p: old → new | idle GPU at 540p |
|---|---|---|---|
| 1 thicket | 16.6 → 19.5 → **20.3** | 15.4 → 15.3 | 1.9 ms |
| 2 house | 17.0 → 17.6 → **20.1** | 15.4 → 16.1 | 1.4 ms |
| 3 Majula view | 18.1 → 19.4 → **22.0** | 17.2 → 18.8 | 6.3 ms |

At 540p that is +18 to +22 % over the old program, and the GPU stopped sitting idle: with 45 to 50 ms of GPU a frame
(scene 25 to 32 ms), the limit at 540p is the GPU again. Stage 3 (alpha cut in the shaders) is next.

**Memory.** The stacks: 68 threads with a 16 KB guest stack, 8 of 128 KB, 7 of 64 KB, 6 of 256 KB, 6 smaller, all with
4 MB of host; the main one (256 KB) with 16 MB. None overflowed in 46 minutes. The new log:

| time | heap taken from the system | allocated | free inside | free chunks | GPU | texture cache |
|---|---|---|---|---|---|---|
| 15:14 | 2,154 MB | 1,948 | 205 | 43,656 | 664 MB | 149 MB |
| 15:24 | 2,518 | 2,257 | 260 | 94,148 | 931 | 382 |
| 15:44 | 2,858 | 2,345 | 512 | 169,932 | 927 | 382 |

The texture cache stopped at 382–386 MB (the trickle of cold ones starts at 75 % of the 512 limit) and the GPU at ~930
MB. Apart from the GPU, the process uses 1.3 to 1.4 GB and grows slowly (+134 MB in 30 min); the heap fragments (170
thousand free chunks) and what it takes from the system keeps rising (2,858 of ~3,190 MB at 38 min). There was margin
left in this session, but a session of several hours may still reach the ceiling: it stays on the list (the texture
pool is in 16 blocks of 32 MB, 86 % fragmented, and 1,273 of the cache's 2,891 textures have the same content as
another).

**The performance loss "over time" starts at an exact instant: 15:28:29.**

    15:28:29.497 Achievement unlocked: 00000026
    15:28:29.552 Presenter: paint mode -> on the UI thread, on request (implicit vsync no, UI drawers 1)

The SDK shows the achievement in an ImGui dialog. A dialog registers a "UI drawer" while it exists, and with a drawer
registered the presenter stops presenting from the game's thread: the UI thread paints, on request. sa2-nx had already
measured this (from ~18 to ~9 pictures a second) and so only creates the dialog on the first achievement; but, once
created, it stays until the program closes. From that second on, in this session:

| | before | after |
|---|---|---|
| GPU submissions a frame (kickoffs) | 2 | 8 to 14 |
| fence queries a second | 25 to 40 | 450 to 790 |
| time waiting for fences, per second | 2 to 70 ms | 550 to 980 ms |
| presenter: wait for the submission of 3 frames back | 0.03 ms | 6 to 8 ms (worst: 40) |
| ring thread, µs a draw | 45 to 50 | 90 to 145 |
| frames a second (10-min mean) | 19.5 to 21.3 | 12.3 to 13.8 |

and the windows of 1 to 4 frames a second the user saw (15:32:37, 15:34:34, 15:44:07: 190 to 210 ms of "GPU a frame"
with only ~20 ms of scene) belong to the same regime. The 27-minute classic session (`ds2_027`) also had an achievement,
but it was the instrumented program and nobody measured it.

Fix (`ds2_achievement_toast`, default `false`): the app overrides `CreateAchievementNotificationDialog()`, the extension
point the SDK documents ("returning nullptr disables notifications"), and does not create the dialog. The achievement
is recorded just the same; only the on-screen notice does not show.

Another fact from the thread profile at the end of the session: the ring thread at 78–83 % of a core and the game's
main thread at 62 %, with 266–280 % of total CPU on the three cores.

**The twentieth build** (2026-10-12): `out\nx` in 0.8 min (80.9 MB) and `out\nx_rel` in 41.8 min (90.5 MB), both with
`ds2_achievement_toast`; `tools/check_switch_build.py` passes on both. The ELFs in `out/switch_elf/optimised_3/`.

## The twentieth build on the console: steady performance; "robotic" sound and the FMOD mixer's priority (2026-10-13)

A 48-minute session with `ds2-opt.nro` (`out/switch_logs/023/ds2_036.1.log` and `ds2_036.log`), with no new
achievement. The presenter stayed in the game-thread mode the whole time and the performance did not drop: 19.8 / 22.4
/ 19.1 / 22.3 / 18.5 frames a second in ten-minute bands (in the previous session, 12 to 14 after the achievement). The
ring thread still gets more expensive with the texture cache near its limit (42 → 54 → 75 µs a draw). No `FATAL`.

The user reports two things: **somewhat robotic, dragging sound**, and **hitches at key points** (area transitions).

**Sound.** FMOD asks the SDK's XMA decoder for data at every block. Per 10-s window:

| session | requests (kicks) | decoding on the XMA thread | windows with the data ready more than 10.6 ms after the request | worst |
|---|---|---|---|---|
| `ds2_032` (10 min, the start of the game) | 9,391 | 0.58 s | 0 of 60 | 9.6 ms |
| `ds2_035.1` (46 min) | 19,208 | 1.78 s | 49 of 161 | 52 ms |
| `ds2_036.1` (first half) | 19,641 | 1.55 s | 32 of 162 | 36 ms |
| `ds2_036` (second half) | 25,625 | 2.48 s | 64 of 122 | 51 ms |

Further into the game there are twice the voices or more, and the audio starts arriving late in 20 to 50 % of the
windows. nfsmw-nx documents the same symptom (`docs/audio-and-video.md`, "Robotic audio"): on the Switch all of the
game's threads run at priority 0x3B, the only one Horizon time-slices (10 ms slices); the game's audio thread waits its
turn behind the main one and the other busy ones, the 256-sample block (5.33 ms) comes out late and XAudio2 plays a
gap. The fix that helped most there was raising that thread's priority to 0x2D.

In DS2 the thread is FMOD Ex 4.40's "FMOD mixer thread". Found through the string in the image (`0x82047D1C`):

- `sub_828478D0` calls `FMOD::Thread::initThread` (`sub_8282C1B0`) with that name; `initThread` copies the name to
  offset 5 of the `Thread` object and calls `FMOD_OS_Thread_Create` (`sub_82822F58`) with the routine
  `FMOD::Thread::callback` = `sub_8282BF30` (`lis r11,-32125; addi r4,r11,-16592`);
- `FMOD_OS_Thread_Create` creates the thread suspended, compares the name with FMOD's six thread names to choose the
  core (`XSetThreadProcessor`) and calls `SetThreadPriority` with the priority FMOD asked for: the game itself asks for
  a high priority for the mixer, and the SDK ignores it (all guest threads at 0x3B).

`app/src/ds2_audio_threads.cpp`: a hook on `sub_8282BF30` reads the name on the new thread itself and, if it is the
mixer, raises its priority (`ds2_audio_mixer_priority`, 0x2D on the Switch: below the ring, 0x2C, and the SDK's audio
threads, 0x2B). The mixer blocks between one block and the next, so it does not hog the core.
`ds2_audio_stream_priority` does the same for the "FMOD stream thread" and stays at 0 (off) until there is a
measurement. The log names each FMOD thread that is born.

**Hitches.** The log's 10-s windows do not show hitches of a few seconds, and the per-frame detail (`[hitch]`) had a cap
of 1,000 lines with a 60 ms threshold: at 20 frames a second almost every frame of a heavy area goes past 60 ms and the
cap ran out in the first minutes. Now a frame of 400 ms or more is always written (with the same breakdown: GPU, waits,
textures, submissions). What is known: in the transitions the game reads 60 to 80 MB from the card in ~3 s (1,000 to
2,000 reads of 128 KB, ~21 MB/s) and creates 250 to 270 textures in 10 s. The next log says whether the time goes in
the renderer or whether it is the game that stops sending frames.

**A session with overclock** (`ds2_037`, 6 min, `ds2.nro`): the profile report read CPU 1,020 MHz / GPU 460.8 MHz /
memory 1,331 MHz in all 35 reports, that is, the factory clocks: the overclock profile did not take effect for this
program (or does not apply to the title it is opened through).

**`-fno-trapping-math` came out** of the optimised builds (`-fno-math-errno` stays). sa2-nx's audit (`docs/cpu-rounds.md`):
the option lets the compiler move a floating-point operation ahead of a change of the host's flush mode, which it does
not know matters; it gave at most 0.5 % on one file. Here there was one more reason: the collection build did not have
the option and the use build did, a difference ahead of reading the profile.

## The reference repositories on 2026-10-13: what is new

`_ref/sa2-nx` went from `fdd6f14` (2026-10-08) to `d2c6e15` (the 1.0.2 pre-release): 11 commits, 279 files, 14
thousand lines. `_ref/nfsmw-nx`: 4 commits, only dubbing tools. `_ref/torchlight-nx` (new): another ReXGlue port
(Torchlight from XBLA), with the native renderer on Vulkan.

What is useful for DS2, in order of value:

1. **Long sessions** (`sa2_native_resolved_retire_idle`, sa2-nx 1.0.2): the copied textures (`resolved_`) were never
   freed; each scene leaves its own and after one to two hours the GPU memory runs out (wrong shadows, then black
   textures and missing geometry; nfsmw-nx has the same report). They are now freed after a minute with no copy or
   read. DS2 uses the same renderer and memory here is already tight: it is the first thing to port.
2. **The three CPU rounds of 2026-10-12** (`docs/cpu-rounds.md`; the author estimates 15 % on the console): fewer
   switches of the host's floating-point mode (`fp_compares_modeless`, `fp_mode_lookahead`, proofs through control
   flow), the guest's compare-and-swap without a barrier, no frame pointer, `.rdata` constants folded (16 thousand
   loads), indirect calls with a usual target turned into guarded direct calls (81 to 85 % of those that run),
   registers as locals in loops and leaf functions, NEON forms of the VMX helpers, critical sections without the thread
   lookup, `fctid`/`fctiw` rounding like the PowerPC (a correctness fix, not only speed), and the link's inlining limit
   from 30 to 64. Almost all of it depends on the new SDK and generator: it means updating `sdk/` to sa2-nx's (keeping
   the `ds2-nx` exceptions), porting the tools, generating again, auditing and collecting a profile once more.
3. **Audio** (nfsmw-nx): besides the priority, native versions of the hottest audio functions (resamplers, filter, sum
   with gain: -17.5 % of CPU on the audio thread there). In DS2 the FMOD mixer thread spends 23 to 48 % of a core, so
   there is something to gain; it needs the profile with stacks to find the functions.
4. What does **not** apply or was discarded there: `-fno-trapping-math` (above), removing the dead stores of
   non-volatile registers in the prologue (it changed the picture in SA2: the game reads a never-initialised stack word
   and depends on the zeros) and of the link register.

**The twenty-first build** (2026-10-13): `out\nx` in 3.3 min (80.9 MB) and `out\nx_rel` in 68.1 min (90.3 MB, all the
game code recompiled without `-fno-trapping-math`); `tools/check_switch_build.py` passes on both. On the PC the hook
recognises the four FMOD threads (`FMOD mixer thread`, `FMOD stream thread`, `FMOD nonblocking thread (0)`, `FMOD file
thread`), and a sampling of the mixing thread confirmed the chain `FMOD::Thread::callback` → `sub_82847B18` (the mixer's
thread function, a neighbour of the one that creates it) → ... → `sub_8283AD00` (the DSP network). The ELFs in
`out/switch_elf/optimised_4/`.

## The twenty-first build on the console; long sessions and thread creation (2026-10-15)

A 9-minute session with `ds2-opt.nro` (`out/switch_logs/024/ds2_040.log`). The user: less robotic sound and fewer sudden
hitches; the area transition hitches remain; in the water area with the knights, 13–15 frames a second.

- **Mixer:** `[audio] FMOD mixer thread: Horizon priority 0x2d set`.
- **Long frames** (the new record, always at 400 ms or more, and the 60 ms one with a cap that did not run out this
  time): only 7 frames above 250 ms in 9 minutes, the biggest of 652 and 446 ms on entering the game. The transition
  ones (15:26:15, 278 ms; 15:27:14, 268 ms; also 120 and 203 ms just before) have **GPU 1.4 ms, one copy, no Swap**:
  the renderer was waiting for the game, which stopped sending frames. And inside each of them the game creates **five
  threads in a row** (`[threads] guest thread 49` to `53` at 15:26:15.33–15.57; `54` to `58` at 15:27:14.39–14.61).
- **The knights' area:** GPU 58–62 ms a frame, of which 26 ms idle (`gap`), scene 20 ms; the ring 35–37 ms. The limit
  there is the CPU (the game), not the GPU.

**What changes in this build:**

- **Long sessions** (ported from sa2-nx 1.0.2, `3aff36c`, `ds2_native_resolved_retire_idle`): the copied textures
  (`resolved_`) were kept the whole session, one per address that ever received a copy; the game frees a target's
  texture when the scene ends and the next one is born at another address, and they pile up until the GPU memory runs
  out (in SA2 and NFSMW: wrong shadows, then black textures and missing geometry after one to two hours). Now one that
  receives no copy and is not read for `ds2_native_resolved_idle_frames` frames (3,600) is freed, and if the GPU runs
  out of memory the ones idle for 120 frames go at the next Swap. A report every 600 frames (`targets: N resolved
  textures and P parked hold X MB; R released ...`). On the PC, with 120 frames to force it: 24 freed (43 MB) in 5
  minutes, no fetch at a freed address, no `FATAL`.
- **Thread creation** (SDK, `threading_posix.cpp`, a `ds2-nx` exception): the 4 MB stacks of the game's threads now use
  sa2-nx's stack pool (it was only for the 16 MB ones: minimum 8 MB, four blocks), with up to sixteen blocks; before,
  each new thread asked a heap with 170 thousand free chunks for 4 MB and gave them back a second after finishing. And
  every creation of 4 ms or more goes to the log (`[threads] creating a thread ... took X ms`): the next log says
  whether the hitch is the cost of the five creations or something else the game does at the same moment.

**The references, 2026-10-15.** `torchlight-nx` released 1.0.1 and 1.0.2:

- 1.0.1: a "device lost" hang in busy scenes (NVK's command queue full at submission), fixed with a Mesa patch
  (`switch/patches/mesa-switch-gpfifo-reserve.patch`). DS2 uses the same driver: worth porting if a `device lost` ever
  shows up.
- 1.0.2: it ported the tools of sa2-nx's second and third CPU rounds as passes over the generated code, without
  replacing the whole SDK: indirect calls promoted to guarded direct ones (from a census made on the PC), read-only
  constants folded, faster floating-point loads, registers as locals in leaf functions and in loops with no call. It is
  the way for DS2 too: sa2-nx's tools (`tools/ro_constants.py`, `float_loads.py`, `leaf_locals.py`, `loop_locals.py`,
  `indirect_calls.py`, `indirect_promotions.py`) need the new macros in `ds2_pch.h` (`REX_RO_U32`, `REX_LOAD_D_F32`,
  `REX_LEAF_LOCAL`, `REX_LOOP_IN`, `REX_CALL_INDIRECT_FUNC_AT`, `REX_PROMOTED_IS`...) and the check on the PC side
  (`RexRoMismatch`, the indirect call census). As the knights' area shows, it is CPU that is lacking.

**The twenty-second build** (2026-10-15): `out\nx` (80.9 MB) and `out\nx_rel` in 43.4 min (90.3 MB);
`tools/check_switch_build.py` passes on both. The ELFs in `out/switch_elf/optimised_5/`.

## The CPU rounds of sa2-nx 1.0.2, ported (2026-10-17)

The goal set by the user: 30 frames a second at stock clocks. Where the frame goes now (twenty-first and
twenty-second builds): in the knights' water area the GPU waits 26 ms of a 60 ms frame, so the recompiled game is the
limit there; in the area transitions the game stops sending frames for 200-650 ms. The CPU rounds sa2-nx shipped in
1.0.1/1.0.2 (its `docs/cpu-rounds.md`; the author estimates 15 % on the console) and torchlight-nx took into its 1.0.2
(promoted indirect calls alone: -13 to -15 % there) are the first step.

**What was ported.**

- **The SDK** (`sdk/`), from sa2-nx `fdd6f14` to `d2c6e15`: 42 files, 3,000 lines (the generator's new options, the
  NEON forms of the VMX helpers, `fctid`/`fctiw` rounding halves to even as the PowerPC does, the guest
  compare-and-swap without the trailing barrier, critical sections without the thread lookup, inline reference
  counts, pointer translation without the heap lookup, events and semaphores that skip the notify nobody waits for).
  Our `sdk/` was sa2-nx's at `fdd6f14` plus the five `ds2-nx` exceptions; the patch applied without a conflict.
- **The generator options** (`app/perf_overrides.toml`): `fp_compares_modeless`, `fp_mode_lookahead`
  (`fp_mode_lookahead_past_branch = false`), `fcti_inline`, `fp_copy_pairs`, `div_inline`.
- **The post-codegen steps** (`tools/post_codegen.py`, sa2-nx's order; each tool copied with this port's names), on
  Dark Souls II's code:

| Step | Result |
|---|---|
| `indirect_calls.py` | 43,851 indirect call sites tagged; 460 promoted to a guarded direct call (below) |
| `loop_barriers.py` (new version) | 51,794 loop headers, 37,151 keep a barrier (14,643 have nothing to protect) |
| `pch_cold_traps.py` | the trap helper out of line and cold |
| `guest_cas.py` | 255 compare-and-swap sites without the trailing `dmb` |
| `flush_mode_flow.py` | 17,408 flush-mode checks proven by the control flow (23,727 kept) |
| `vec_byteswap.py` | 115,828 vector byte reversals as REV64 + EXT; 9,322 shuffles by table |
| `pch_neon_shuffles.py` | constant word shuffles as one permutation |
| `ro_constants.py` | 14,672 float, 1,350 double, 820 integer and 7,837 vector loads from read-only data folded |
| `float_loads.py` | 41,533 single-precision loads straight into a float register |
| `frame_traffic.py` | marks only (the elisions stay off: not exact, sa2-nx's audit) |
| `loop_locals.py` | 12,809 call-free loop regions in 6,234 functions keep 81,563 registers in locals |
| `leaf_locals.py` | 2,625 leaf functions with loops, 36,667 registers in locals |

- **The PC side** (`app/src/cpu/ds2_codegen_checks.cpp`, from sa2-nx's `sa2_codegen_checks.cpp` and its enhancements
  file): every folded constant, promoted call and flush-mode proof the PC build executes is checked against the old
  operation (`[verify]` lines), the indirect call census (`ds2_diag_indirect_census`), and the run-time key
  `ds2_cpu_guest_cas_no_barrier`.
- **The build** (`app/CMakeLists.txt`): the game code without the frame pointer on the Switch (`-fomit-frame-pointer`:
  the crash log then lists the possible return addresses on the stack) and, with a profile, the inliner's limit at
  64 and the unit growth at 12 %.

**Checked on the PC** (`ds2_098`, 9 minutes from the title screen through the prologue video and the opening cutscene
to Things Betwixt): 153 million flush-mode proofs executed, 7.6 million folded constants and 3.1 million critical
section owners compared, **0 differences**; the prologue video, the cutscene and Things Betwixt draw right.

**The promoted indirect calls.** The census of that session: 9,722 call sites, 317 million calls. A site is
promoted when one target (or two or three) takes 99.5 % of its calls, or one takes 70 % (sa2-nx's thresholds): 460
sites, 51.8 % of all indirect calls, 44 of them with two or three targets (`app/indirect_promotions.txt`). The guard
compares the target, so a scene the census did not see still goes the right way, one compare later. The largest
site with many targets is the WMV decoder's dispatch (`sub_833A70A0`, 30.5 million calls, eight targets): not
promoted.

**What this changes for the PGO profile.** Every function of the game is different text now, so the profile of
2026-10-12 does not fit: this round ships the plain build (`ds2.nro`) and the instrumented one (`ds2-pgo.nro`) for a
new collection; the LTO + PGO build comes after it.

## Area transitions: what the other ports do

- **sa2-nx**: the guest thread stacks from a pool and finished threads joined early when a creation needs their
  slot (a one-shot thread every ~35 ms on one stage had made 1,000 ms frames); here the pool now takes the 4 MB
  stacks too (twenty-second build), and each creation of 4 ms or more is logged. It also moved the log writing to its
  own thread (an SD write had stalled the ring thread for 50-150 ms), which this port already has.
- **nfsmw-nx**: a cache of file reads by exact range, because the game read the same zone packs again on every lap.
  Dark Souls II does the same: in the last console session half of what it read (187 of 395 MB) were ranges it had
  read before, all in pieces of 128 KB or less, which the cache's floor of 256 KB left out. The shipped `ds2.toml`
  now sets `sa2_io_ranges_min_kb = 128`.
- **torchlight-nx**: its loading reads go through a 64 KB block cache (64 MB), and its console profile is split by
  time slices because the first 30-40 % of the samples are loading.
- **MarathonRecomp-NX**: a stall watchdog (`[stall]` dumps of every thread when no frame comes for a second).
- **Here**: the console profiler already separates the samples taken during long frames ("during the hitches" in
  `rex_profile.log`, with `configs/ds2_profile.toml`), but it took every frame over 45 ms, which at 20 frames a
  second is nearly all of them, and keeps only 256 windows. The threshold is now `ds2_profile_hitch_ms` (150 ms), so a
  profiled session through a few transitions says what each thread was doing during the stalls.

## 30 frames a second

`ds2_swap_interval` (0 by default): 2 paces the presentation at 30 frames a second on the Switch (each image stays two
vblanks). sa2-nx measured that it only pays once the median frame is under ~31 ms (otherwise 22 % of the frames fall
to 66.7 ms), so it stays off until the game holds 30 most of the time.

## deko3d

The analysis of moving the renderer from Vulkan (NVK) to deko3d, from nfsmw-nx's branch and the other ports, is in
`docs/deko3d.md`. In short: not now; what limits this game is the recompiled code and our own ring work, not NVK.
