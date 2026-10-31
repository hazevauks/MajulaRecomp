# The road to the release

Agreed with the user on 2026-10-30, after the move to Scholar of the First Sin (docs/dlc.md). The goal is a steady
30 fps in handheld mode at stock clocks. Locked 30 at native 720p is not the plan: Dark Souls II already dipped below 30
on the Xbox 360 itself.

The starting point, measured in Things Betwixt (the console, 2026-10-30): 45.8 ms a frame (21.8 fps), GPU 45.5 ms,
game thread 39 ms. Both the GPU (~27 %) and the CPU (~15 %) have to shrink, and the later areas weigh more.

## The order

1. **The naming screen's softlock.** The screen stays black and the system keyboard never opens. The thirty-seventh
   build logs the first call of each export to find what the game waits on. The same build round makes the three
   `REX_EXPORT_STUB`s Scholar added to the imports return a proper value (`XamShowMarketplaceDownloadItemsUI`,
   `XamUserGetMembershipTierFromXUID`, `XamUserGetOnlineCountryFromXUID`).
2. **The areas after Things Betwixt.** Bugs, crashes and runtime gaps (code that only data pointers reach), the DLC
   areas included, since they hold the most new code. The shadow glitch near fires belongs here.
3. **A measurement session.** `configs/ds2_profile.toml` on the card: GPU milliseconds per pass and per pixel shader,
   plus CPU stacks. Play an area with a bonfire, water, grass and a fog wall (Majula, the Forest of Fallen Giants).
4. **Dynamic resolution and the effect cuts.**
   - The resolution switches on its own between 720p and 540p (and 600p, to be added), upscaled to 720p. It follows
     the GPU time with hysteresis: each switch rebuilds the render targets, so it cannot change every frame.
   - First, 540p must work: in 2014 it crashed on the PC when a save was confirmed, never explained
     (`app/ds2_switch.toml`).
   - The Xbox 360 executable has no quality settings like the PC version's, so each cut is made in the renderer, by
     its pass or its shaders. The candidates:
     - shadows (a smaller shadow map, or updated every other frame);
     - grass (fewer instances or a shorter distance; N&B's "every Nth fur shell" in `render/draw.cpp` is a
       precedent);
     - fog walls (simplified, not erased: they mark the boss and area doors);
     - bonfire fire and other particles (half resolution);
     - water (its reflection).
   - The order follows step 3's measurements. Every cut has its own toml key. The default configuration is the
     performance one, and a `configs/ds2_full_quality.toml` brings everything back.
5. **The indirect-call census** for Scholar. 61 of `app/indirect_promotions.txt`'s 460 promotions were dropped in
   the migration. The promotions change the generated code, so they come before PGO.
6. **Features and quality-of-life changes** (docs/features.md; out of the build since the migration).
7. **PGO, last.** A profile describes the code as it is, and every change after it leaves part of it stale. Collected
   once, on the final code.
8. **Release preparation.**
   - A regression pass on the console.
   - The player's instructions: dumping disc 1, and installing disc 2's Compatibility Pack with
     `tools/install_dlc.py`. No game data ships.
   - The final package.
