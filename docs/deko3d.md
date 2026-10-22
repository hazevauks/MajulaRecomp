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

## The census (2026-10-22)

`tools/d3d_callgraph.py` (the call graph of the generated code, with the kernel imports each function calls) and the
disassembly in the generated comments. Each function was identified by what it does, then checked against SA2's
distances where the linker kept the same neighbours.

**The device layout is SA2's.** `Direct3D_CreateDevice` allocates 24,704 bytes (SA2: 24,704, `0x6080`), and DS2's
Direct3D reads the device at SA2's offsets throughout: fences 11024/11036, flags 11068/11072, the vertex declaration
12240, the index buffer 12812, targets 12816, textures 12920, pixel/vertex shader 13068/13072, the inline ring end
13844, the command buffer 13920, the automatic buffers 15228, the present fence 15344, the swap count 16968, the
behaviour flags 24392. Renut's SA2 offsets are the starting point as they are; each one is still read once more in DS2's
code when its hook is ported.

| Function | DS2 | SA2 | How it was found |
|---|---|---|---|
| Direct3D_CreateDevice | `829BA480` | `8254FC10` | allocates 24,704; the game calls it at `82FD9F80`; calls the hardware path `829C20B0` |
| hardware path (SA2 `sub_8255CDA0`) | `829C20B0` | | `VdInitializeEngines`, `KeQueryPerformanceFrequency`, SetRingBufferParameters |
| RingBufferDeviceAllocate | `829A8008` | `8254D4B8` | BlockOnFence - 0x230, as in SA2 |
| CDevice::BlockOnFence | `829A8238` | `8254D6E8` | completed (11024) against next (11036); KickOff, then the CBlocker wait |
| AllocateInlineData | `829A8450` | `8254D9A8` | segment allocator - 0xE8, as in SA2 |
| ring segment allocator | `829A8538` | `8254DA90` | |
| CDevice::KickOffSegment | `829A92D8` | `8254E830` | segment allocator + 0xDA0, as in SA2; called by KickOff |
| CDevice::KickOff | `829A9618` | `8254EBB8` | 75 callers; closes the segment, returns the write position (+48) |
| IsFencePending | `829A9758` | `8254ECF8` | KickOff + 0x140, as in SA2; the global device, the fence compare |
| BeginRingBig | `829A9850` | `8254EDF8` | IsFencePending + 0xF8 |
| InsertCallback | `829A9940` | `8254EEE8` | BeginRingBig + 0xF0, as in SA2 |
| SetRingBufferParameters | `829A9998` | `8254EF40` | InsertCallback + 0x58, as in SA2; `VdInitializeRingBuffer` |
| CBlocker ctor / Check / dtor | `829B9B68` / `829B9C60` / `829B9B98` | `8254F400` (Check) | the wait loop of every fence wait |
| SynchronizeToPresentationInterval | `829BB060` | `825589D8` | Swap - 0x4C0, as in SA2 (to read again when ported) |
| Swap | `829BB520` | `82558E98` | calls `VdSwap`, reads 15344 and 16968 |
| ClearF | `829CCC28` | `82563978` | Clear (`829CCD60`, D3DCOLOR) calls it per rectangle: Z in f1, stencil in r8 |
| Resolve | `829B60E8` | `825656F8` | 19 instructions: reads the caller's +92 and +100, calls `829B4F90` |
| BeginVertices | `829C2340` | `82565758` | |
| DrawVerticesUP | `829C2800` | `82565C18` | |
| DrawVertices | `829C2E28` | `82565C60` | |
| DrawIndexedVertices | `829C3240` | `82566078` | DrawVertices + 0x418, as in SA2 |
| XGRegisterVertexShader | `829B10E0` | `8255E658` | (object, physical part): type 6, physical at +32, the 872-byte object |
| XGRegisterPixelShader | `829AF490` | `8255DBF8` | type 7, physical at +24 |
| CreateVertexDeclaration | `829AFA30` | `8255E198` | (elements) -> declaration: counts the 12-byte elements to the 0xFF stream, allocates, initialises (`829AF940`) |
| D3DResource_Destroy | `829ACF00` | `8254A200` | Release (`829AD1A8`) calls it at zero; a switch on the resource type |

Also DS2's own: `BeginIndexedVertices` `829C2848` and `DrawIndexedVerticesUP` `829C2D88` (SA2's linker dropped
them); the header copies of the shaders `829B10A0` (+872) and `829B0F78` (+40), Renut's container offsets. **The game
also initialises vertex declarations in its own memory** (`829AF940` called directly from `826A8F08`): that path needs
the declaration hook too, or those declarations never reach Renut.
