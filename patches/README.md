# Patches

> **ds2-nx (2026-10-22).** This folder was taken as it is from sa2-nx 1.0.3.1 (`adf3a4e`), together with the trees
> it describes (`app/src/renut_engine`, `app/thirdparty`). The patches below are sa2-nx's: `renut-banjo-nx-c319b29.patch`
> and `plume-banjo-nx-c319b29.patch` match the imported trees exactly until this port retargets Renut to Dark Souls
> II; `rexglue-sdk-v0.10.0.patch` is sa2-nx's SDK, of which this port's `sdk/` differs by the `ds2-nx` exceptions
> listed in the README; `xenosrecomp-990d03b.patch` is sa2-nx's translator, of which `shaders/XenosRecomp` differs
> by this port's translator fixes. They are regenerated for this port as the deko3d work lands (`docs/deko3d.md`).

Every change this port makes to third-party code, as diffs against the upstream sources it was taken from. The changed
files are also in the repository as they are, so it builds without applying anything: these patches show exactly
what changed, and let you apply the same changes to a fresh upstream checkout.

| Patch | Upstream base | What it changes |
|---|---|---|
| `rexglue-sdk-v0.10.0.patch` | [rexglue/rexglue-sdk](https://github.com/rexglue/rexglue-sdk) v0.10.0, commit `c94f5ebdcb3c9d1a460ca48e04f9758448f8d518` | The whole SDK fork (`sdk/`): the Horizon (Switch) layer, the code generator's options, the console tuning of the runtime, the store cards (banjo-nx's: the card table in `Memory::Initialize`, the CRT hooks and file reads marking them), the core layout for Renut (`RexSetGuestThreadCoreLayout`, each guest thread applying it itself at its next wait, the other game threads preferring core 2), the Switch build without Mesa's NVK driver (`REXGLUE_SWITCH_NO_NVK`: `cmake/rexglue_switch.cmake`, `src/ui/switch_no_vulkan.c`), the controller read hook Renut's late input uses (`include/rex/input/read_hook.h`, `InputSystem::GetState`), and the SDK's build of FFmpeg (`thirdparty/CMakeLists.txt`, `thirdparty/ffmpeg-overlay/codec_list.c`). 208 files, +26,735 / -897 lines |
| `ffmpeg-0604b46.patch` | [wmarti/FFmpeg](https://github.com/wmarti/FFmpeg) commit `0604b464c7cb4ebc94940cf1f324a3b26b87717c`, the FFmpeg submodule of rexglue-sdk v0.10.0 | `config.h` (the Switch configuration), `libavcodec/fft_template.c` and `libavcodec/mdct_template.c` (a table cache), and two new files: `config_switch_aarch64.h` and `config_masseffect_wmv3.h` |
| `renut-banjo-nx-c319b29.patch` | `app/src/renut_engine` of banjo-nx (ChanseyIsTheBest's Banjo-Kazooie: Nuts & Bolts port, which carries [reNut](https://github.com/masterspike52/reNut) by masterspike52) at commit `c319b297299a5701cb55c474995c060dbe787805` (2026-10-19) | Renut's renderer retargeted to SA2's Direct3D (`docs/renut-renderer.md`): SA2's device layout and hooks, the display gamma ramp at scan-out, the frame pacing, the lockstep captures, banjo-nx's working changes of 2026-10-20 taken ahead of its commit (the stale texture header fix, the present-stall fix, early-Z, texture-size and aw4 support, the pipeline list and shader cache synced to the card, the latency trim, their reports), the compatibility layer for what banjo-nx's SDK has and SA2's does not, the render-thread and game-thread work after the first console run (the device deltas, constants by the chain, the capture batches, the hooks' fast paths, the chain's reuse of the pipeline key, targets, viewport and textures, the store cards' memexport marks, the done signal and batch clock; after the third and fourth: the narrow and NEON device compares, the shared constants' skipped lines, the next draw's prefetch, the draws' batched wakes of the render thread, Renut forced on deko3d in a build without the Vulkan driver; banjo-nx's input latency set of 2026-10-20 merged in: eager kicks, display sync joined to SA2's frame pacing (late input from the GPU queue taken, then removed at the user's request); the double buffer key; peggle2-nx's GLSL package loader (deko3d_glsl.bin beside the NRO, for builds without translated game shaders); futari-nx's deferred chain: Begin*Vertices draws in the device delta chain; MSAA of the game's single-sample targets, `sa2_renut_msaa` 0/2/4/8, 2 by default; 1.0.3.1: the render resolution
following the console's mode, 1920x1080 docked and 1280x720 handheld (`sa2_renut_dock_resolution`), with the GLSL
package's check for packages made for 1x only), and the files SA2 adds (`sa2_compat/`, `sa2_renut_*`, `sa2_line_compare.h`, `guest/sa2_screen.cpp`). 52 files, +3,795 / -367 lines. The base's N&B-only files this port does not take (19) are left as they are. Line endings LF |
| `plume-banjo-nx-c319b29.patch` | `app/thirdparty/plume` of banjo-nx at the same commit (plume by renderbag, with UnleashedRecomp-NX's Switch patches and banjo-nx's deko3d back end) | Its top-level files: banjo-nx's deko3d back end changes of 2026-10-20 taken ahead of its commit (`plume_deko3d.cpp`/`.h`, `plume_deko3d_commands.cpp`) `CMakeLists.txt`'s `PLUME_SWITCH_NO_NVK` (the NVK driver not linked), and `plume_vulkan.cpp`'s guard for that case (no instance when volk finds no `vkCreateInstance`, from banjo-nx); 1.0.3.1: `Deko3DSetSwapChainSize`, the deko3d swapchain at the display's size (1920x1080 docked: the window takes it with `nwindowSetDimensions` once the old swapchain has released its buffers). Nothing under `contrib/` or `examples/` changes. 5 files, +158 / -2 lines. Line endings LF |
| `xenosrecomp-990d03b.patch` | [hedge-dev/XenosRecomp](https://github.com/hedge-dev/XenosRecomp) commit `990d03b28a27b50277ee5d8d942e1c5f873869d1` | The shader translator (`shaders/XenosRecomp`): `shader_recompiler.cpp`/`.h` and `shader_common.h`, and the new `pch_min.h`. What each change does is in `shaders/README.md` |

The NVK driver (Mesa) is not changed by this port: see [mesa/README.md](../mesa/README.md).

## Applying them

Each patch is a `git diff` from the root of its upstream repository:

```sh
git clone https://github.com/rexglue/rexglue-sdk.git && cd rexglue-sdk
git checkout c94f5ebdcb3c9d1a460ca48e04f9758448f8d518
git apply ../sa2-nx/patches/rexglue-sdk-v0.10.0.patch

git submodule update --init thirdparty/FFmpeg && cd thirdparty/FFmpeg
git apply ../../../sa2-nx/patches/ffmpeg-0604b46.patch

# banjo-nx is a local repository (D:\Banjo kazooie port\banjo-nx):
git -C banjo-nx -c core.autocrlf=false archive c319b297299a5701cb55c474995c060dbe787805 app/src/renut_engine | tar -x
git apply ../sa2-nx/patches/renut-banjo-nx-c319b29.patch
git -C banjo-nx -c core.autocrlf=false archive c319b297299a5701cb55c474995c060dbe787805 app/thirdparty/plume | tar -x
git apply ../sa2-nx/patches/plume-banjo-nx-c319b29.patch

git clone https://github.com/hedge-dev/XenosRecomp.git && cd XenosRecomp
git checkout 990d03b28a27b50277ee5d8d942e1c5f873869d1
git apply ../sa2-nx/patches/xenosrecomp-990d03b.patch
```

The SDK patch leaves upstream's folders that the fork does not carry (`docs/`, `scripts/`, `tools/`, `.github/` and
the editor settings, and upstream's own files in `tests/`) as they are; nothing in the port's build uses them. In
`tests/` it adds the fork's eight unit tests (`sdk/tests/`, run by `tools\win\run_sdk_tests.ps1`). Line endings are
LF, as git stores the files.

Each patch was checked by applying it to a clean checkout of its base: the result is byte for byte the files of this
repository.

After a change to `sdk/`, `python tools/make_sdk_patch.py <scratch repository>` writes the SDK patch again (its
header says how to make the scratch repository) and prints the new totals for the table above.
