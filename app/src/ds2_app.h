// Dark Souls II: the ReXGlue application object (paths, GPU plugin choice, analysis aids).

#pragma once

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <system_error>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/rex_app.h>
#include <rex/runtime.h>
#include <rex/system/gpu_plugin.h>
#include <rex/ui/overlay/achievement_notification.h>

#include "cpu/ds2_enhancements.h"
#include "ds2_capture.h"
#include "ds2_input_script.h"
#include "ds2_pgo.h"
#include "me/ds2_render_mode.h"
#include "me/me_native_system.h"

// Which host API the emulated Xenos GPU draws with. The SDK takes Direct3D 12 on Windows when both are built in; the
// Switch build has Vulkan only, so "vulkan" on the PC is the same code path the console's first frames use.
REXCVAR_DEFINE_STRING(ds2_gpu_backend, "", "DS2",
                      "Host API of the emulated GPU (gpu_plugin = xenos): empty = the SDK's choice, d3d12 or vulkan");

// The SDK shows an unlocked achievement as an ImGui dialog. A dialog registers a UI drawer for the rest of its life,
// and with one drawer registered the presenter stops presenting from the game thread: frames are painted by the UI
// thread on request (the SDK already creates the dialog only at the first achievement for this reason). On the console
// (2026-10-12, a 46-minute session): from the first achievement on, 8 to 14 GPU submissions a frame instead of 2, the
// presenter waiting 6-8 ms a frame for its send, the ring thread at 90-145 us a draw instead of 45-50, and 12 frames a
// second where there had been 20, until the game was closed. The achievement is recorded either way.
REXCVAR_DEFINE_BOOL(ds2_achievement_toast, false, "DS2",
                    "Show the on-screen notification when an achievement unlocks. false = none: the notification "
                    "dialog, once created, keeps the presenter on its slow path for the rest of the session");

class Ds2App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Ds2App>(new Ds2App(ctx, "ds2", PPCImageConfig));
  }

  void OnPostInitLogging() override {
    // The runtime defaults to no GPU plugin: select ReXGlue's Xenos implementation, keeping an explicit
    // command-line or config-file choice.
    if (!rex::cvar::HasNonDefaultValue("gpu_plugin")) {
      rex::cvar::SetFlagByName("gpu_plugin", "xenos");
    }
    // Which enhancements run (own key + group master), in one line; before any guest thread exists.
    ds2::enh::Resolve();
  }

  // Without --game_data_root, use a game_root/ folder next to the executable (on the Switch:
  // sdmc:/switch/ds2-nx/game_root), and on a PC build tree the repository's assets/game_root.
  // An explicit path always wins.
  void OnConfigurePaths(rex::PathConfig& paths) override {
    if (!paths.game_data_root.empty()) {
      return;
    }
    std::error_code ec;
    const auto folder = rex::filesystem::GetExecutableFolder();
    if (folder.empty()) {
      return;
    }
    for (auto candidate = folder; !candidate.empty(); candidate = candidate.parent_path()) {
      for (const auto& sub : {std::filesystem::path("game_root"),
                              std::filesystem::path("assets") / "game_root"}) {
        // The folder itself, or only its asset cache (game_root.nxpack + .nxidx, see sdk pack_device.h).
        auto pack = candidate / sub;
        pack += ".nxpack";
        if (std::filesystem::is_regular_file(candidate / sub / "Default.xex", ec) ||
            std::filesystem::is_regular_file(pack, ec)) {
          paths.game_data_root = candidate / sub;
          return;
        }
      }
      if (candidate == candidate.parent_path()) {
        break;
      }
    }
  }

  // An explicit backend for the emulated GPU (ds2_gpu_backend); otherwise ReXApp loads the plugin with its default.
  void OnPreSetup(rex::RuntimeConfig& config) override {
    ds2::enh::ApplyGpuBoost();
    // Stage 2: with ds2_renderer_native = true the graphics system is the app's own native Vulkan renderer (src/me),
    // and the emulated Xenos GPU plugin is not loaded (ReXApp only loads it when config.graphics is empty).
    if (me::native::Enabled()) {
      config.graphics = me::native::CreateGraphicsSystem();
      REXLOG_INFO("graphics: native Vulkan renderer (PM4 ring)");
      return;
    }
    const std::string backend = REXCVAR_GET(ds2_gpu_backend);
    if (!backend.empty() && !config.graphics && !config.gpu_plugin.empty()) {
      config.graphics = rex::system::LoadGpuPlugin(config.gpu_plugin, backend);
      REXLOG_INFO("graphics: emulated GPU '{}' on {}{}", config.gpu_plugin, backend,
                  config.graphics ? "" : " (not available: the SDK's default is used)");
    }
  }

  // Recording of the game image for testing (ds2_video_s = N seconds after start; 0 = off). Taken from the
  // presenter's guest output, so it works minimized and nothing can cover it.
  void OnPostSetup() override {
    // Unattended runs: a scripted pad (ds2_input_script) to get through the warnings and menus.
    ds2::input_script::Install(runtime());
    // Every 10 s: how often each active enhancement was used (a log shows they really ran).
    ds2::enh::StartReport();
    // PC: the key that simulates docking / undocking (render resolution, bind_ds2_docked = F9).
    ds2::render::RegisterKeys();
    ds2::capture::Start([this]() -> rex::ui::Presenter* {
      const auto* rt = runtime();
      const auto* graphics = rt ? rt->graphics_system() : nullptr;
      return graphics ? graphics->presenter() : nullptr;
    });
    // The PGO-instrumented build (DS2_PGO=generate) saves its counts to pgo/ every 2 minutes (nothing otherwise).
    ds2::pgo::Start();
  }

  std::unique_ptr<rex::ui::AchievementNotificationDialog> CreateAchievementNotificationDialog() override {
    if (!REXCVAR_GET(ds2_achievement_toast)) {
      static bool said = false;
      if (!said) {
        said = true;
        REXLOG_INFO("[app] achievement unlocked: no on-screen notification (ds2_achievement_toast = false)");
      }
      return nullptr;
    }
    return rex::ReXApp::CreateAchievementNotificationDialog();
  }

  // The PGO-instrumented build saves its counts when the game loses focus (HOME menu): it may be closed from there.
  void OnWindowFocusChanged(bool focused) override {
    if (!focused) ds2::pgo::DumpNow();
  }

  void OnShutdown() override {
    ds2::capture::Stop();
    ds2::pgo::Stop();
  }

  // Analysis aid: with DS2_DUMP_IMAGE=<file>, write the loaded guest image (image_base..image_base+image_size) so
  // code pointers stored in data can be scanned for (tools/pointer_scan.py). Does nothing unless the variable is set.
  void OnPostLoadXexImage() override {
    if (!runtime() || !runtime()->memory()) {
      return;
    }
    const char* path = std::getenv("DS2_DUMP_IMAGE");
    if (!path) {
      return;
    }
    const uint8_t* image =
        runtime()->memory()->TranslateVirtual<const uint8_t*>(PPCImageConfig.image_base);
    if (std::FILE* f = std::fopen(path, "wb")) {
      std::fwrite(image, 1, PPCImageConfig.image_size, f);
      std::fclose(f);
      REXLOG_INFO("guest image written to {}", path);
    }
  }
};
