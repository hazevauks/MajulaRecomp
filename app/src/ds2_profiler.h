// ds2-nx (2026-10-29): the on-screen profiler (docs/profiler-overlay.md). A small window over the game with the
// last frames' timings: the frame (Swap to Swap), the game thread's busy part of it, Renut's render thread, the GPU and
// the host present, as lines and as current / average rows, plus which of them limits. R + Minus shows or hides it on
// the Switch; ds2_profiler = true shows it from the start (the only way on the PC). Every 10 s a "[profiler]" log
// line keeps the same figures (with the 1 % low) whether the window is shown or not.
#pragma once

#include <functional>

namespace rex::ui {
class ImGuiDrawer;
}

namespace ds2::profiler {

// Once, after the runtime is set up. on_ui_thread runs a function on the UI thread later (where ImGui dialogs are
// created, drawn and destroyed).
void Install(rex::ui::ImGuiDrawer* imgui, std::function<void(std::function<void()>)> on_ui_thread);

// Shows or hides the window (any thread).
void RequestToggle();

}  // namespace ds2::profiler
