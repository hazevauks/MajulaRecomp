# deko3d: the plan (2026-10-22)

The user's decision (2026-10-22): move the renderer to deko3d, the Switch's own graphics API, as the sibling Xbox 360
ports did, for the long term even if the first gain is small. The goal stays 30 frames a second without drops at
stock clocks, at 540p; 720p once deko3d frees enough GPU. The earlier analysis on this page (2026-10-17, "not now")
weighed only swapping NVK under the current renderer; the route below replaces the renderer itself.

## Which route

| | nfsmw-nx `deko3d-implementation` | sa2-nx 1.0.3 (from banjo-nx) |
|---|---|---|
| What changes | a second back end under the PM4-ring renderer | the renderer: Renut's, at the Direct3D level, on plume |
| PM4 ring, EDRAM model | kept | **gone** |
| Status | never merged; NVK stays the release | released (1.0.3, 1.0.3.1), and banjo-nx, peggle2-nx, futari-nx |
| Guide | `docs/HowToDeko3d.md`, `docs/deko3d-devlog.md` | `docs/deko3d-renderer-guide.md`, `docs/renut-renderer.md` |

**sa2-nx's route**, because what limits this port is exactly what it removes. At the twenty-fourth build (540p, stock
clocks) the ring thread spent 41-54 ms a frame parsing PM4 and preparing draws (50-58 µs a draw, against 8-11 µs
on sa2-nx's Renut render thread), and the GPU spent 6-10 ms a frame on the EDRAM emulation passes. NVK was 3.7 % of
the CPU. Our current renderer is also sa2-nx's own family, so the order of work, the tools and the checks carry over.

**Dark Souls II's Direct3D is close to SA2's.** DS2 links Direct3D 2.0.21256 (docs/ds2-notes.md); SA2 links 21076 and
Sonic & All-Stars Racing Transformed 21119, which has SA2's device layout. Renut's offsets were mapped from N&B
(2.0.7645) to SA2 with a name for every source; DS2 should start from SA2's map and confirm each offset in its own
code.

## The steps, as they apply here

1. **Census.** The ~25 Direct3D functions Renut hooks, and the device layout. Already known from the native renderer
   work (docs/ds2-notes.md): `DrawVertices` `sub_829C2E28`, `DrawIndexedVertices` `sub_829C3240`, `DrawVerticesUP`
   `sub_829C2800`, `BeginVertices` `sub_829C2340`, Swap `sub_829BB060`, and the ring wait `sub_829B9C60`. Still to
   find: `Direct3D_CreateDevice`, `KickOff`/`KickOffSegment`, `BeginRingBig`, the ring allocators,
   `SetRingBufferParameters`, `BlockOnFence`, `CBlocker::Check`, `IsFencePending`, `InsertCallback`,
   `SynchronizeToPresentationInterval`, `ClearF`, `Resolve`, `XGRegisterVertexShader`/`PixelShader`,
   `CreateVertexDeclaration`, `D3DResource_Destroy`. `tools/d3d_match.py` needs a named reference image (N&B's or
   SA2's), which is not here: the matching is by shape against the call graph we have, each confirmed by its calls.
2. **Renut on the PC**, on plume's Vulkan back end, behind a boot key (`ds2_renderer = "renut"`), against the current
   renderer frame by frame. This PC has no hardware Vulkan: lavapipe, as for the current renderer.
3. **Shaders.** reblue-XenosRecomp -> DXC -> SPIR-V -> SPIRV-Cross -> GLSL -> banjo-nx's passes -> uam. DS2 has 1,409
   containers (SA2: 103), so the console's first-use compiles and the DKSH cache matter much more here: the cache has
   to be built on the PC from a pipeline list before a console test, or the first session stutters for a long time.
4. **deko3d.** The guide tests first light in Ryujinx (banjo-nx's build). Ryujinx needs a real GPU and this PC has an
   Intel HD 4000: either Ryujinx on lavapipe (to be tried; slow) or straight to the console.
5. **Drop NVK** (`REXGLUE_SWITCH_NO_NVK`, `PLUME_SWITCH_NO_NVK`): the NRO loses the driver.
6. **Console profile and the render thread work** (device deltas, store cards, chain reuse, wake batches, core layout).
7. **Pacing and input latency.** DS2 runs at 30 frames a second on the Xbox 360 (presentation interval two): Renut's
   pacing must keep the game's own interval.
8. **Game shaders outside the NRO.** The GLSL is made from the game's shaders, so it is game data: it cannot be in
   the repository or a public NRO. For the console tests it goes on the SD card with the package, as the game files do.

## What changes in the repository

Third-party code comes in as it is, with its licence and a diff in `patches/` for every change: plume with banjo-nx's
deko3d back end (MIT), uam (zlib/MIT/MPL-2.0), reblue-XenosRecomp (MIT), Renut's renderer (`app/src/renut_engine`,
see below). The SDK takes sa2-nx 1.0.3's changes (`patches/rexglue-sdk-v0.10.0.patch` there: the no-NVK switch, the
input read hook, the core layout, the emulator start). The current renderer (`app/src/me`) stays as the reference until
Renut draws everything, as in sa2-nx, and NVK (Mesa 26.2.1 now; the 26.2.4 SDK is in `_tools`) with it.

## Open points for the user

- **reNut's licence.** reNut (masterspike52) has no licence file; sa2-nx's notes say its author permits reuse by that
  developer's ports. This repository needs the same permission before Renut's code is committed here.
- **Ryujinx.** See step 4.
