// sa2-nx (2026-10-20): banjo-nx's on-screen profiler, which Renut's frame statistics (renut_engine/perf) feed.
// ds2-nx (2026-10-29): this port's profiler window is behind them (app/src/ds2_profiler.cpp, docs/profiler-overlay.md);
// RecordGameFrame and AddGameThreadWait are its own two (frame_stats.cpp's OnGameFrame and OnGameThreadWait).
#pragma once

namespace banjo::profiler {

void SetGpuFrameMs(double ms);      // a frame's GPU time (timestamps), when it comes back
void SetRenderThreadMs(double ms);  // the render thread's busy time for a frame
void RecordHostPresent();           // the render thread presented an image
void SetRendererInfo(const char *name, const char *device);
void RecordGameFrame();             // the game thread's Swap
void AddGameThreadWait(double ms);  // the game thread waited for the render thread (pacing, fences)

}  // namespace banjo::profiler
