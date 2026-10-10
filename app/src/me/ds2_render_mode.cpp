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

#if REX_PLATFORM_SWITCH
#include <switch.h>
#endif

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
  const int32_t height = docked ? REXCVAR_GET(ds2_render_height_docked) : REXCVAR_GET(ds2_render_height_handheld);
  const uint32_t q = QuartersOf(height);
  if (g_last_logged_q.exchange(q | (docked ? 0x100u : 0u), std::memory_order_relaxed) != (q | (docked ? 0x100u : 0u))) {
    REXLOG_INFO("[res] {} mode: render {}x{} (ds2_render_height_{} = {}{})", docked ? "docked" : "handheld",
                320 * q, 180 * q, docked ? "docked" : "handheld", height,
                int32_t(180 * q) != height ? fmt::format(", rounded to a multiple of 180") : std::string());
  }
  return q;
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

void OnPresented(uint64_t presented) {
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
