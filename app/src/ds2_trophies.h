// Dark Souls II: the trophy screen, the unlock notices and our own platinum trophy (on top of the SDK's achievement
// store: the game's 38 achievements come from its XDBF, unlocks are saved under ds2/achievements/465307E4.toml).
//
//   - The screen replaces the SDK's achievements overlay (ReXApp::CreateAchievementsOverlay). It opens with L + Minus
//     on the Switch (and with the SDK's own L + R + Left), F7 on the PC; B, Minus or the same shortcut close it. While
//     it is open the game does not receive the controller (the SDK's rule for its menus).
//   - The notice: a card in the corner for a few seconds. It is an ImGui dialog that closes itself when it has nothing
//     left to show: with any ImGui dialog alive the presenter paints from the UI thread (the slow path that took the
//     twentieth build from ~20 to ~12 frames a second after an achievement), so it must not stay.
//   - The platinum trophy is ours: it unlocks when every achievement of the game is unlocked (1000G), with its own
//     emblem (drawn, no image) and a bigger notice.
#pragma once

#include <functional>
#include <memory>

struct ImFontAtlas;

namespace rex {
class Runtime;
namespace system {
class AchievementManager;
}
namespace ui {
class ImGuiDialog;
class ImGuiDrawer;
class ImmediateDrawer;
}  // namespace ui
}  // namespace rex

namespace ds2::trophies {

// The id of our platinum trophy in the achievement store (the game's ids are 1..38). "PLAT".
constexpr unsigned kPlatinumId = 0x504C4154u;

struct Hooks {
  rex::ui::ImGuiDrawer* imgui = nullptr;
  rex::ui::ImmediateDrawer* immediate = nullptr;
  rex::Runtime* runtime = nullptr;
  rex::system::AchievementManager* achievements = nullptr;
  // Runs a function on the UI thread later (never inside the current call): ImGui dialogs are created, drawn and
  // destroyed there.
  std::function<void(std::function<void()>)> on_ui_thread;
  // Opens or closes the screen (the SDK's bind_achievements, F7): called on the UI thread.
  std::function<void()> toggle_screen;
};

// Once, after the runtime is set up: the notices and the platinum check.
void Install(Hooks hooks);

// The screen (the app returns it from CreateAchievementsOverlay).
std::unique_ptr<rex::ui::ImGuiDialog> CreateScreen();

// The trophy screen's font (the app's OnConfigureFonts): the console's system font, Segoe UI on the PC.
void ConfigureFonts(ImFontAtlas* atlas);

// Tests (ds2_trophy_test_at): a notice for the platinum and one for the first trophy of the game, without unlocking
// anything. From any thread.
void TestNotices();

// The L + Minus shortcut, from any thread (ds2::render::OnPresented reads the pad once per presented frame).
void RequestToggle();

}  // namespace ds2::trophies
