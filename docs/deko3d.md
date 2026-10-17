# deko3d instead of Vulkan (NVK): what the other ports found, and what it would mean here

Written 2026-10-17, from nfsmw-nx's branch `deko3d-implementation` (`docs/HowToDeko3d.md`, 533 lines, and
`docs/deko3d-devlog.md`, 921 lines; last commit 2026-10-10, merged as pull request #27) and from what the other ports
in `_ref/` use. A first look is in `docs/ds2-notes.md` ("A CPU profile with stacks on the console and the deko3d
question", 2026-10-10); this page brings it up to date with the numbers of the twenty-second build.

## What deko3d is

devkitPro's low-level graphics API for the Switch's GPU (the one Nintendo's own titles reach through NVN). It looks
like Vulkan (memory blocks, command buffers, queues, fences, barriers, images, descriptors) but has no pipelines, no
render passes and no descriptor sets, and it writes GPU commands itself, with no driver in between. Its shaders are
DKSH binaries made by `uam`, a compiler that only takes GLSL.

## Who uses what

| Port | Renderer | Driver |
|---|---|---|
| sa2-nx, this port, torchlight-nx | the native PM4-ring renderer (`app/src/me`) on Vulkan | NVK (Mesa) with nfsmw-nx's patch |
| nfsmw-nx `main` (released) | the same family, on Vulkan | NVK with its own 35-file patch |
| nfsmw-nx `deko3d-implementation` | a second backend, `RenderDk`, next to the Vulkan one | deko3d + uam |
| MarathonRecomp-NX, UnleashedRecomp-NX | their own renderer through plume (a Vulkan layer) | NVK |
| WWHD (Wind Waker HD) | moved from Mesa (OpenGL) to deko3d | deko3d |

Only nfsmw-nx has tried deko3d among the ReXGlue ports, and it keeps the NVK build as the released one.

## What nfsmw-nx measured (stock clocks, handheld)

- **Before the port's own work:** deko3d was *slower*: GPU-bound at 35-43 ms a frame against NVK's 19-24 ms in the
  same race. The NVK path's GPU savings (copies, barriers, deferred clears, depth compression, ...) had to be redone
  for deko3d, in five batches, before it caught up.
- **After twelve console runs:** 26-32 fps in a race where NVK did 21-29; at its 960x540 scene option 35.2 fps on
  average (28.0-47.7), GPU 24.6 ms a frame, the ring thread at 8.5 µs a draw (2,200 draws a frame).
- **What it removed:** pipeline creation (about 59 ms each on NVK, the stutters of a first race) and NVK's CPU cost
  per draw (pipeline lookups, descriptor sets, the draw itself: ~3.4 µs of the ~9 µs a draw the ring thread spent).
  Also the driver build (MSYS2, Rust) and the Mesa patch.
- **What it did not remove:** the GPU cost of the shaders (uam is not NAK, and NAK's tuning does not carry over: the
  open items are individual shaders slower than on NVK), the game's own main thread, the NvMap handle limit.
- **Cost:** about 13,000 lines (a second renderer backend, 73 files changed) and a new shader path: HLSL -> DXC ->
  SPIR-V -> SPIRV-Cross -> GLSL -> uam -> DKSH, with changes to XenosRecomp's HLSL so SPIRV-Cross accepts it.
  deko3d does not run on a PC: every check of the new backend is a console run, while the Vulkan renderer stays the PC
  build and the reference. The branch is not merged into `main` and nothing newer has been published.

## The arithmetic for Dark Souls II (twenty-second build, 540p, stock clocks)

| What | Here | nfsmw-nx (NVK) | What deko3d acts on |
|---|---|---|---|
| Ring thread per draw | 40-55 µs (LTO + PGO) | ~9 µs | only NVK's share of it |
| NVK's own work | the deferred-recording thread, 3.7 % of the CPU (round-5 stack profile) | ~3.4 µs a draw | all of it |
| Pipeline creation stalls | prewarmed from `cache/ds2_native_pipelines.bin`; none in the hang reports | the first race of a session | all of them |
| GPU per frame | 45-62 ms (scene 20-32 ms) | 19-24 ms | possibly worse at first (uam) |
| Where the CPU goes | two thirds the recompiled game, about a fifth the ring thread | the ring thread first | neither |

What limits this game now is, in this order: the recompiled game's own code (the knights' water area: the GPU waits
26 ms a frame), the ring thread's own work (indices, vertices, constants, the EDRAM emulation: API-independent), and
the GPU at 720p. deko3d would take away the 3.7 % of NVK and a part of the ring thread's 40 µs, against a rewrite of
the renderer's backend, including the parts this game depends on most (the EDRAM mode 4 aliases and the conversions
by fragment and compute shaders, ZCULL with its patch, the render-scale rewrite of the SPIR-V), all of it written
against Vulkan and checked on the PC with lavapipe.

## Recommendation

Not now. The CPU rounds of sa2-nx 1.0.2 (ported on 2026-10-17: guarded direct calls, read-only constants, float loads,
leaf and loop locals, flush-mode proofs) go after the two thirds that deko3d does not touch, and the ring thread's
cost is our own code. Worth revisiting when:

1. the ring thread is under ~15 µs a draw and NVK's share becomes a large part of it, or
2. a console profile shows pipeline or descriptor work of NVK in the frame time, or
3. nfsmw-nx (or another ReXGlue port) publishes a deko3d backend that reached parity and could be shared: the
   backend is the renderer family's, not the game's, so most of it would carry over.

If it is ever done, the order nfsmw-nx used is the one to follow: the shader path first, alone (all variants of
`ds2_shaders.mesp` through uam, byte-identical DKSH on the PC and the console), then an empty renderer, then the first
frames, then parity, and the NVK NRO stays as the fallback until deko3d beats it on the same scenes at stock clocks.
