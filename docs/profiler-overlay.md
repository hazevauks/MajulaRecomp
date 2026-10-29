# The on-screen profiler (and how to take it to another port)

A small window over the game, top right, with the timings of the last 240 frames as lines and as rows (current, the
average of the last 60, and that average as a frame rate), and which part limits the frame. On the Switch, R + Minus
shows or hides it at any time; `ds2_profiler = true` in `ds2.toml` shows it from the start (on the PC that key is the
only switch). Every 10 s the log gets a `[profiler]` line with the same averages and the 1 % low, whether the window is
shown or not, so a test session keeps the figures without photos.

| Row | What it is | Where it comes from |
|---|---|---|
| Frame (Swap to Swap) | the time between two of the game's `D3DDevice_Swap`: what the player sees as the frame rate | `frame_stats::OnGameFrame` (Renut's Swap hook) |
| Game thread | the frame less the game thread's waits for Renut (Swap's pacing, fence waits): its own work, the game's code plus Renut's hooks on its thread. Waits for the game's own worker threads are not taken off | `frame_stats::OnGameThreadWait` |
| Render thread | Renut's render thread, busy time per frame (recording the frame's GPU work), less its waits for the GPU and the display | `frame_stats::OnRenderThreadFrame` |
| GPU | the GPU's time executing a frame's command lists (timestamp queries; a frame or two late) | `frame_stats::OnGpuFrame` |
| Present | the time between two host presents | the render thread at each present |

**Limit** names the largest of game thread, render thread and GPU (averages). At 30 fps every row must stay under
33.3 ms; the plot's lines are at 16.7, 33.3 and 50 ms (60, 30 and 20 fps).

## Code

- `app/src/ds2_profiler.cpp` / `.h`: the rings (one per row, under one mutex taken once per event), the window (an
  `rex::ui::ImGuiDialog`, the SDK's ImGui overlay, drawn on the UI thread; no ImPlot: the lines are `AddPolyline`
  on the window's draw list) and the 10 s log line.
- `app/src/renut_engine/ds2_compat/profiler/profiler.h`: the `banjo::profiler` functions Renut's frame statistics
  (`renut_engine/perf/frame_stats.cpp`) call. banjo-nx has a profiler window behind them; sa2-nx and its other ports
  left them empty. Two were added here: `RecordGameFrame` and `AddGameThreadWait`.
- `app/src/me/ds2_render_mode.cpp`: R + Minus, read once per presented frame on its own `PadState` (as L + Minus for
  the trophy screen).
- `app/src/ds2_app.h`: `ds2::profiler::Install(imgui_drawer(), <run on the UI thread>)` next to the trophies'.

## Taking it to another port (Renut on deko3d, sa2-nx's line)

1. Copy `ds2_profiler.cpp` and `.h`; rename the namespace and the cvar.
2. Replace the port's empty `profiler/profiler.h` (sa2-nx: `renut_engine/sa2_compat/profiler/profiler.h`) with this
   port's declarations, and add the two calls in `renut_engine/perf/frame_stats.cpp`: `RecordGameFrame()` at the
   start of `OnGameFrame`, `AddGameThreadWait(ms)` in `OnGameThreadWait`.
3. Call `Install` once the app has its ImGui drawer, with a function that runs on the UI thread
   (`app_context().CallInUIThreadDeferred`).
4. Read a button combination once per presented frame (any place that runs every frame on the Switch) and call
   `RequestToggle()` on its press.
5. Add the source to the app's CMake target.

A port without Renut feeds the same five rows from its own renderer: the frame at its present or Swap hook, the GPU
from its timestamp queries, the render thread (or ring thread) from its busy time.

## Not this

The Status Monitor overlay (and a fork of it, discussed on 2026-10-29) sees what the system shows: the clocks, the
cores' and the GPU's load, the temperature, and the frame rate a program reports through SaltyNX
(`sdk/src/ui/switch_saltynx.cpp`). The rows above live inside the game; a Status Monitor fork would need each game to
publish them first. Kept as an idea for later.
