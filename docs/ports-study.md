# Other Xbox 360 ports studied (2026-10-29)

The user's list of 36 recomp ports, read for what this port can reuse: performance (30 fps at 1020 / 460 / 1331 MHz,
handheld 720p), shader stutters, and how they ship. This port's situation when it was made: the thirty-first build
runs 21-27 fps at the four measuring points, and the GPU is the limit (43-44 ms a frame at the forest bonfire, the
game thread busy about half the frame); the beta tester's report (Mariko at 614 MHz GPU: 25-35 fps in play) agrees.

## Which ones matter

| Group | Ports | For this port |
|---|---|---|
| Renut on deko3d (sa2-nx's line) | sa2-nx, bejeweled3-nx, ketsui-nx, futari-nx, guwange-nx, zuma*, peggle*, feedingfrenzy* (Chansey), mgr-nx-deko, mk9-nx, usf4-nx | The same renderer as ours: their changes port almost as they are |
| Native PM4 / NVK renderers | nfsmw-nx, nfscarbon-nx, masseffect-nx, Bakugan-NX, sc5p2-nx, the-simpsons-game-nx, project8-sw | The route this port left on 2026-10-22; their CPU notes still apply |
| Own engines / other routes | VasilkovND's (Dead Space 2, Gears 2, Max Payne 3, Dante's Inferno: build tools only), LoS1-nx, torchlight-*, skate-3-rust-engine, reblue, LostOdysseyRecomp, mhnwis-nx, BattleBlock | Little to take |
| XenonRecomp ports | MarathonRecomp-NX, UnleashedRecomp-NX | Their CPU findings are summed up in ketsui-nx's `docs/research/` |

## Taken (thirty-second build)

- **sa2-nx 1.0.4's renderer round** (its `docs/renut-renderer.md` items 36-43), the diff of `app/src/renut_engine`
  between 1.0.3.1 (`adf3a4e`, what this port imported) and 1.0.4, renamed for DS2; every hunk applied:
  - `ds2_renut_colour_handover`: a resolve of a whole colour target into a texture of its size and format is a swap
    of images, not a copy (in SA2 the colour resolves were about a fifth of the GPU's time); the PC log of the start
    of Things Betwixt: 35-50 resolves a frame not drawn where about 5 (depth only) were;
  - `ds2_renut_clear_repeat`: a whole clear of a target that already holds exactly that clear is not sent;
  - `ds2_renut_lazy_resolve_mips`, `ds2_renut_direct_resolve_mips`: the resolve mip levels made where read (docked);
  - `ds2_renut_lockfree_queue`: the render thread's queue is a lock-free ring, the chain's lock a light one (about
    4 % of SA2's game thread);
  - `ds2_renut_queue_drain`, `ds2_renut_double_buffer` (off), the display sync changes.
  - **Off for DS2**: `ds2_renut_setter_marks`, `ds2_renut_marks_copy` (and `ds2_renut_setters_precise`, off in sa2-nx
    too). The marks come from hooks on SA2's own Direct3D setters (sa2-nx's `me/sa2_setter_marks.*`), which this
    port does not have; with them on, a setter that stores without a dirty tag would go unmarked.
- **usf4-nx 0.8.0** (its `docs/deko3d-renderer-usf4.md`):
  - `ds2_renut_static_linear_textures`: a linear texture unchanged at 60 checks in a row is checked by sample, and
    hashed whole every 16 frames (their render thread 6.4 -> 2.8 ms); `PmullHash` is the first entry of this port's
    render thread profile too (11 %);
  - the shader compile a draw waits for goes ahead of the pipeline prebuild's queued compiles (`plume_deko3d.cpp`).

## Shader stutters: what the others do

- **sa2-nx, usf4-nx, mk9-nx**: the DKSH cache (`deko3d_shaders.bin`) compiled on the PC from a pipeline list
  (`tools/deko3d_variant_cache.py`), shipped beside the NRO; the console only compiles variants the list lacks (about
  0.08-0.45 s each, the stutter). mk9-nx fills its list with an unattended console rig that plays scripted routes
  (ladders, towers, the Krypt: 5,752 variants).
- **mgr-nx-deko**: players send their captures per chapter; a merge tool builds a shared "pipeline seed" shipped with
  the package, and a PREPARING GRAPHICS screen builds the whole seed at the first start.
- **This port** (since 2026-10-25): `tools/win/package_switch.ps1` compiles the console's own `pipeline_list.bin`
  (copied into `switch-logs`) on the PC, merged with its `deko3d_shaders.bin`. Next: merge the beta testers' lists
  too (the tool takes several `--list`), so the shipped cache covers the whole game.
- **Shipping**: the others do not put game shaders in the NRO (game data): an installer page builds
  `deko3d_glsl.bin` from the player's own game files (sa2-nx-installer, usf4-nx-installer, with wasm builds of DXC,
  SPIRV-Cross and the translator). This port's test NRO links `deko3d_glsl.bin` in: fine for private tests, not for
  a public release, which needs the same installer route (and the DKSH cache would be made there too, or on the
  console).

## Still to look at

- sa2-nx 1.0.4's CPU round (its `docs/cpu-rounds.md`): the localized copies and hand-written natives are per game,
  but the tools (`tools/localize_writebacks.py`, the batch scripts) are generic.
- sa2-nx 1.0.3's post-codegen round (about twenty tools), not ported yet.
- The setter marks for DS2: a census of DS2's Direct3D setters (the same Direct3D 2.0.21256 as SA2's) and hooks on
  them, then `ds2_renut_setter_marks` on (SA2: 74 % of `CaptureDelta`, 10 % of the game thread).
