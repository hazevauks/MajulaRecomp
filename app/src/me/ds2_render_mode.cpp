// Sonic Adventure 2: render resolution per console mode and MSAA (see ds2_render_mode.h).
#include "ds2_render_mode.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>

#include <fmt/format.h>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/platform.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/switch_saltynx.h>

#include "cpu/ds2_enhancements.h"
#include "ds2_profiler.h"
#include "ds2_trophies.h"

#if REX_PLATFORM_SWITCH
#include <malloc.h>

#include <switch.h>
#endif

#if REX_PLATFORM_SWITCH
extern "C" void RexSwitchPerfIntervalSwap(unsigned vblanks);  // sdk/src/ui/switch_perf.cpp
#endif

// 2026-10-17: frame pacing at 30 frames a second on the Switch (the SDK's swap interval, switch_perf.cpp: each image
// stays on screen for two vblanks). Only worth it once the game holds 30 most of the time: below that, frames that
// miss a vblank wait for the next one (sa2-nx measured 22 % of frames at 66.7 ms with a median of 31 ms).
REXCVAR_DEFINE_INT32(ds2_swap_interval, 0, "DS2",
                     "Switch: vblanks per presented image (2 = paced at 30 frames a second, 1 = 60). 0 = left as the "
                     "system has it (no pacing)")
    .range(0, 4);
REXCVAR_DEFINE_INT32(ds2_trophy_test_at, 0, "DS2",
                     "Test: at this presented frame the trophy notices show (platinum and the first trophy, nothing is "
                     "unlocked); 600 frames later the trophy screen opens, 900 after that it closes. 0 = off");
REXCVAR_DEFINE_INT32(ds2_render_height_handheld, 720, "DS2",
                     "Render height in handheld mode (the PC's starting mode unless ds2_render_docked): 720 = the "
                     "game's own 1280x720. Any multiple of 180 from 540 to 2160 (others are rounded to the nearest); "
                     "the width follows at 16:9")
    .range(180, 4320)
    .lifecycle(rex::cvar::Lifecycle::kHotReload);
REXCVAR_DEFINE_INT32(ds2_render_height_docked, 1080, "DS2",
                     "Render height in docked mode (the TV): 1080 = 1920x1080. Any multiple of 180 from 540 to "
                     "2160; switching between docked and handheld changes it at the next frame")
    .range(180, 4320)
    .lifecycle(rex::cvar::Lifecycle::kHotReload);
REXCVAR_DEFINE_BOOL(ds2_render_docked, false, "DS2",
                    "PC only: start in the simulated docked mode (ds2_render_height_docked). The bind_ds2_docked key "
                    "(F9) switches between docked and handheld while the game runs, like docking the Switch. The "
                    "Switch itself always follows the console");
REXCVAR_DEFINE_INT32(ds2_msaa, 0, "DS2",
                     "Multisample anti-aliasing of the game's own (single-sample) render targets: 0 = off, as the "
                     "game and as before (default); 2 or 4 = that many samples per pixel, resolved when the game "
                     "copies the image. The game's 4x surfaces (Direct3D's fast Clear aliases) stay as they are. Read "
                     "at start")
    .range(0, 4)
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(ds2_render_height_combo, true, "DS2",
                    "Switch: holding both sticks pressed (L3 + R3) for one second switches the render height of the "
                    "current mode between 540 and 720 while the game runs (a line in the log says when), to compare "
                    "the two at the same spot. It lasts until the game is closed; ds2.toml keeps its value");
REXCVAR_DEFINE_INT32(ds2_render_test_height_at, 0, "DS2",
                     "Test (PC): switch the render height between 540 and 720, as the Switch's L3 + R3 does, after "
                     "this presented swap and every that many swaps after it (0 = never)")
    .range(0, 2000000000);
REXCVAR_DEFINE_INT32(ds2_render_test_toggle_at, 0, "DS2",
                     "Test (PC): toggle the simulated docked mode after this presented swap (0 = never)")
    .range(0, 2000000000);
REXCVAR_DEFINE_INT32(ds2_render_test_toggle_every, 0, "DS2",
                     "Test (PC): after ds2_render_test_toggle_at, toggle again every this many presented swaps "
                     "(0 = once)")
    .range(0, 2000000000);

// Non-CPU group: the new behaviours have their own keys like every other change (docs/enhancements.md).
DS2_ENHANCEMENT(g_render_scale, ds2_gpu_render_scale, ds2::enh::kNonCpu, true,
                "GPU: render at ds2_render_height_handheld / ds2_render_height_docked (720 / 1080 by default), "
                "following docked / handheld live; off = always the game's 1280x720");
DS2_ENHANCEMENT(g_msaa, ds2_gpu_msaa, ds2::enh::kNonCpu, true,
                "GPU: honour ds2_msaa (2x / 4x host multisampling of the game's single-sample render targets, "
                "resolved at the game's copies); with ds2_msaa = 0 (default) nothing changes");

namespace ds2::render {
namespace {

std::atomic<bool> g_docked_pc{false};
std::atomic<bool> g_docked_pc_set{false};
std::atomic<uint32_t> g_last_logged_q{0};
// ds2-nx: the height chosen with L3 + R3 (ds2_render_height_combo), 0 = the configuration's.
std::atomic<int32_t> g_height_override{0};

// q = height / 180, from 3 (540p) to 12 (2160p); other heights go to the nearest multiple of 180.
// ds2-nx: nothing below 540p. Dark Souls II draws into targets one row high (its 256x1 tone map table) and 1x1; the
// host edge of guest row 1 is ceil(q / 4 - 1 / 2) (me_resolution_scale.h): 1 at q = 3, but 0 at q = 2, where those
// images were created with height 0 (VUID-VkImageCreateInfo-extent-00945, then a heap corruption in lavapipe at the
// first 3D frame with ds2_render_height_handheld = 360).
uint32_t QuartersOf(int32_t height) {
  const int32_t q = (std::clamp(height, 540, 2160) + 90) / 180;
  return uint32_t(std::clamp(q, 3, 12));
}

bool DockedPc() {
  if (!g_docked_pc_set.load(std::memory_order_acquire)) {
    bool expected = false;
    if (g_docked_pc_set.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
      g_docked_pc.store(REXCVAR_GET(ds2_render_docked), std::memory_order_release);
  }
  return g_docked_pc.load(std::memory_order_acquire);
}

}  // namespace

bool Docked() {
#if REX_PLATFORM_SWITCH
  // libnx keeps the operation mode cached; the SDK's applet loop (windowed_app_context_switch.cpp) processes the
  // OperationModeChanged message that updates it. Reverse-NX's "fake" mode is obeyed, as the window does.
  return rex::ui::switch_saltynx::BaseMode(appletGetOperationMode() == AppletOperationMode_Console);
#else
  return DockedPc();
#endif
}

uint32_t WantedScaleQuarters() {
  if (!g_render_scale.active) return 4;
  const bool docked = Docked();
  const int32_t chosen = g_height_override.load(std::memory_order_relaxed);
  const int32_t height = chosen ? chosen
      : docked ? REXCVAR_GET(ds2_render_height_docked) : REXCVAR_GET(ds2_render_height_handheld);
  const uint32_t q = QuartersOf(height);
  if (g_last_logged_q.exchange(q | (docked ? 0x100u : 0u), std::memory_order_relaxed) != (q | (docked ? 0x100u : 0u))) {
    REXLOG_INFO("[res] {} mode: render {}x{} (ds2_render_height_{} = {}{})", docked ? "docked" : "handheld",
                320 * q, 180 * q, docked ? "docked" : "handheld", height,
                int32_t(180 * q) != height ? fmt::format(", rounded to a multiple of 180") : std::string());
  }
  return q;
}

int32_t RenderHeightOf(bool docked) {
  return docked ? REXCVAR_GET(ds2_render_height_docked) : REXCVAR_GET(ds2_render_height_handheld);
}

uint32_t ScaleQuartersOf(bool docked) {
  if (!g_render_scale.active) return 4;
  return QuartersOf(docked ? REXCVAR_GET(ds2_render_height_docked) : REXCVAR_GET(ds2_render_height_handheld));
}

void ToggleDocked() {
#if !REX_PLATFORM_SWITCH
  const bool docked = !DockedPc();
  g_docked_pc.store(docked, std::memory_order_release);
  REXLOG_INFO("[res] simulated {} (PC): render height {}", docked ? "docking" : "undocking",
              docked ? REXCVAR_GET(ds2_render_height_docked) : REXCVAR_GET(ds2_render_height_handheld));
#endif
}

// ds2-nx: 540 <-> 720 for the mode in use. The renderer asks WantedScaleQuarters() every frame and rescales its
// images when the answer changes, as it does when the console is docked or undocked.
void ToggleHeight(const char* by) {
  const bool docked = Docked();
  const int32_t chosen = g_height_override.load(std::memory_order_relaxed);
  const int32_t now = chosen ? chosen
      : docked ? REXCVAR_GET(ds2_render_height_docked) : REXCVAR_GET(ds2_render_height_handheld);
  const int32_t next = QuartersOf(now) == 3 ? 720 : 540;
  g_height_override.store(next, std::memory_order_relaxed);
  REXLOG_INFO("[res] render height switched by {}: {} -> {} (until the game is closed)", by, 180 * int32_t(QuartersOf(now)),
              next);
}

void OnPresented(uint64_t presented) {
#if REX_PLATFORM_SWITCH
  {
    static int32_t applied = -1;
    const int32_t interval = REXCVAR_GET(ds2_swap_interval);
    if (interval != applied) {
      applied = interval;
      RexSwitchPerfIntervalSwap(unsigned(interval));
      if (interval) REXLOG_INFO("[res] swap interval {}: paced at {} frames a second", interval, 60 / interval);
    }
  }
#endif
  // Tests of the trophy screen and notices (ds2_trophy_test_at, ds2_trophies.cpp): at that presented frame the notices
  // show, 600 frames later the screen opens and 900 frames later it closes.
  if (const int32_t at = REXCVAR_GET(ds2_trophy_test_at); at > 0) {
    if (presented == uint64_t(at)) ds2::trophies::TestNotices();
    if (presented == uint64_t(at) + 600 || presented == uint64_t(at) + 1500) ds2::trophies::RequestToggle();
  }
#if REX_PLATFORM_SWITCH
  // L + Minus: the trophy screen (ds2_trophies.cpp), once per press.
  {
    static PadState pad;
    static bool started = false, was_down = false;
    if (!started) {
      padInitializeAny(&pad);
      started = true;
    }
    padUpdate(&pad);
    const u64 combo = HidNpadButton_L | HidNpadButton_Minus;
    const bool down = (padGetButtons(&pad) & combo) == combo;
    if (down && !was_down) {
      REXLOG_INFO("[trophies] L + Minus");
      ds2::trophies::RequestToggle();
    }
    was_down = down;
    // ds2-nx (2026-10-29): R + Minus, the profiler window (ds2_profiler.cpp), once per press.
    static bool profiler_was_down = false;
    const u64 profiler_combo = HidNpadButton_R | HidNpadButton_Minus;
    const bool profiler_down = (padGetButtons(&pad) & profiler_combo) == profiler_combo;
    if (profiler_down && !profiler_was_down) {
      REXLOG_INFO("[profiler] R + Minus");
      ds2::profiler::RequestToggle();
    }
    profiler_was_down = profiler_down;
  }
  // ds2_render_height_combo: both sticks held for a second. The pad is read here, on its own PadState (HID's shared
  // memory; the game's input driver is not involved), once per presented frame.
  if (REXCVAR_GET(ds2_render_height_combo)) {
    static PadState pad;
    static bool started = false;
    static u64 held_since = 0;
    static bool fired = false;
    if (!started) {
      padInitializeAny(&pad);
      started = true;
    }
    padUpdate(&pad);
    const u64 both = HidNpadButton_StickL | HidNpadButton_StickR;
    if ((padGetButtons(&pad) & both) == both) {
      const u64 now = armGetSystemTick();
      if (!held_since) held_since = now;
      if (!fired && armTicksToNs(now - held_since) >= 1000000000ull) {
        fired = true;
        ToggleHeight("L3 + R3");
      }
    } else {
      held_since = 0;
      fired = false;
    }
  }
#endif
  {
    const int32_t height_at = REXCVAR_GET(ds2_render_test_height_at);
    if (height_at > 0 && presented >= uint64_t(height_at) && (presented - uint64_t(height_at)) % uint64_t(height_at) == 0)
      ToggleHeight("ds2_render_test_height_at");
  }
#if !REX_PLATFORM_SWITCH
  const int32_t at = REXCVAR_GET(ds2_render_test_toggle_at);
  if (at <= 0 || presented < uint64_t(at)) return;
  const int32_t every = REXCVAR_GET(ds2_render_test_toggle_every);
  if (presented == uint64_t(at) || (every > 0 && (presented - uint64_t(at)) % uint64_t(every) == 0)) {
    REXLOG_INFO("[res] test toggle at presented swap {}", presented);
    ToggleDocked();
  }
#else
  (void)presented;
#endif
}

uint32_t MsaaSamples() {
  if (!g_msaa.active) return 1;
  const int32_t samples = REXCVAR_GET(ds2_msaa);
  return samples >= 4 ? 4u : samples >= 2 ? 2u : 1u;
}

std::string ProcessMemoryText() {
#if REX_PLATFORM_SWITCH
  u64 total = 0, used = 0;
  svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
  svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
  const struct mallinfo heap = mallinfo();
  return fmt::format("process memory {} of {} MB; heap: {} MB taken from the system, {} MB allocated, {} MB free "
                     "inside it in {} pieces", used >> 20, total >> 20, size_t(heap.arena) >> 20,
                     size_t(heap.uordblks) >> 20, size_t(heap.fordblks) >> 20, size_t(heap.ordblks));
#else
  return {};
#endif
}

void NoteRescale() { g_render_scale.Use(); }
void NoteMsaaUse(uint64_t n) { g_msaa.UseSampled(); (void)n; }

void RegisterKeys() {
#if !REX_PLATFORM_SWITCH
  rex::ui::RegisterBind("bind_ds2_docked", "F9",
                        "Switch between the simulated docked and handheld modes (render resolution)",
                        [] { ToggleDocked(); });
#endif
}

}  // namespace ds2::render
