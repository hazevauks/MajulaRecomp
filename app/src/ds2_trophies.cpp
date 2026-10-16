// Dark Souls II: the trophy screen, the unlock notices and our platinum trophy. See ds2_trophies.h.

#include "ds2_trophies.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

#include <imgui.h>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/platform.h>
#include <rex/system/achievement_manager.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>
#include <rex/ui/immediate_drawer.h>
#include <rex/ui/overlay/achievement_icon_cache.h>

#if REX_PLATFORM_SWITCH
#include <switch.h>
#endif

REXCVAR_DEFINE_BOOL(ds2_trophy_notices, true, "DS2",
                    "Show a notice in the corner when a trophy unlocks (it closes itself after a few seconds; while "
                    "one is on screen the presenter takes its slower path)");
REXCVAR_DEFINE_BOOL(ds2_trophy_platinum, true, "DS2",
                    "Our platinum trophy: unlocked when every achievement of the game is unlocked (1000G)");

namespace ds2::trophies {
namespace {

using rex::system::AchievementEvent;
using rex::system::AchievementInfo;
using rex::system::AchievementManager;

Hooks g_hooks;
ImFont* g_font = nullptr;  // the system font (Switch) or Segoe UI (PC); null = ImGui's default

constexpr float kNoticeSeconds = 5.0f;
constexpr float kPlatinumNoticeSeconds = 8.0f;

// Colours: the gold of the game's menus over a dark stone background.
const ImU32 kGold = IM_COL32(214, 178, 104, 255);
const ImU32 kGoldDim = IM_COL32(150, 124, 74, 255);
const ImU32 kText = IM_COL32(232, 226, 214, 255);
const ImU32 kTextDim = IM_COL32(150, 144, 134, 255);
const ImU32 kPanel = IM_COL32(18, 16, 14, 238);
const ImU32 kRowUnlocked = IM_COL32(56, 46, 28, 150);
const ImU32 kPlatinumLight = IM_COL32(236, 238, 242, 255);
const ImU32 kPlatinumMid = IM_COL32(170, 176, 188, 255);
const ImU32 kPlatinumDark = IM_COL32(96, 102, 116, 255);

const char* const kPlatinumName = "Portador da Maldição";

AchievementInfo PlatinumInfo() {
  AchievementInfo info;
  info.id = kPlatinumId;
  info.label = kPlatinumName;
  info.description = "Todos os troféus de Dark Souls II: 1000G.";
  info.unachieved_description = "Desbloqueie todos os outros troféus do jogo.";
  info.gamerscore = 0;  // the game's 1000G stay the total
  return info;
}

// The store forgets everything it was given when the game's XDBF is loaded (ReplaceAchievements): registered again
// whenever it is about to be needed.
void EnsurePlatinum(AchievementManager& store) {
  if (!REXCVAR_GET(ds2_trophy_platinum)) return;
  if (!store.FindAchievement(kPlatinumId)) store.RegisterAchievement(PlatinumInfo());
}

struct Progress {
  int unlocked = 0, total = 0;
  int earned_g = 0, total_g = 0;
};

Progress CountGame(const AchievementManager& store) {
  Progress p;
  for (const AchievementInfo& a : store.ListAchievements()) {
    if (a.id == kPlatinumId) continue;
    ++p.total;
    p.total_g += int(a.gamerscore);
    if (store.IsUnlocked(a.id)) {
      ++p.unlocked;
      p.earned_g += int(a.gamerscore);
    }
  }
  return p;
}

// Unlocks the platinum when the game's achievements are all unlocked. Not from inside the store's own callback (the
// unlock it makes dispatches callbacks again): on the UI thread.
void CheckPlatinum() {
  if (!REXCVAR_GET(ds2_trophy_platinum) || !g_hooks.achievements) return;
  AchievementManager& store = *g_hooks.achievements;
  EnsurePlatinum(store);
  if (store.IsUnlocked(kPlatinumId)) return;
  const Progress p = CountGame(store);
  if (p.total > 0 && p.unlocked == p.total) {
    REXLOG_INFO("[trophies] all {} trophies of the game unlocked ({}G): platinum", p.total, p.earned_g);
    store.UnlockAchievement(kPlatinumId, rex::system::AchievementNotification::kShow);
  }
}

std::string DateOf(uint64_t filetime) {
  if (filetime < 116444736000000000ull) return {};
  const std::time_t t = std::time_t((filetime - 116444736000000000ull) / 10000000ull);
  std::tm local{};
#if defined(_WIN32)
  localtime_s(&local, &t);
#else
  localtime_r(&t, &local);
#endif
  char text[32];
  std::strftime(text, sizeof(text), "%d/%m/%Y", &local);
  return text;
}

float Seconds() {
  using namespace std::chrono;
  static const auto start = steady_clock::now();
  return duration<float>(steady_clock::now() - start).count();
}

// The platinum emblem, drawn: a ring, a cup with two handles on a stem and a base, and a glint when unlocked.
void DrawPlatinumEmblem(ImDrawList* dl, ImVec2 c, float r, bool unlocked) {
  const ImU32 light = unlocked ? kPlatinumLight : IM_COL32(110, 110, 110, 255);
  const ImU32 mid = unlocked ? kPlatinumMid : IM_COL32(80, 80, 80, 255);
  const ImU32 dark = unlocked ? kPlatinumDark : IM_COL32(48, 48, 48, 255);
  dl->AddCircleFilled(c, r, dark, 48);
  dl->AddCircleFilled(c, r * 0.90f, mid, 48);
  dl->AddCircleFilled(c, r * 0.80f, IM_COL32(22, 22, 26, 255), 48);
  dl->AddCircle(c, r * 0.80f, light, 48, r * 0.03f);
  // The cup.
  const float w = r * 0.36f, top = c.y - r * 0.46f, bowl = c.y + r * 0.02f;
  dl->PathClear();
  dl->PathLineTo(ImVec2(c.x - w, top));
  dl->PathLineTo(ImVec2(c.x + w, top));
  dl->PathBezierCubicCurveTo(ImVec2(c.x + w, bowl), ImVec2(c.x + w * 0.5f, bowl + r * 0.06f),
                             ImVec2(c.x, bowl + r * 0.06f));
  dl->PathBezierCubicCurveTo(ImVec2(c.x - w * 0.5f, bowl + r * 0.06f), ImVec2(c.x - w, bowl),
                             ImVec2(c.x - w, top));
  dl->PathFillConvex(light);
  // Handles.
  const float th = r * 0.07f;
  dl->AddBezierCubic(ImVec2(c.x - w, top + r * 0.06f), ImVec2(c.x - w - r * 0.26f, top + r * 0.04f),
                     ImVec2(c.x - w - r * 0.22f, top + r * 0.34f), ImVec2(c.x - w * 0.55f, bowl - r * 0.02f), mid, th);
  dl->AddBezierCubic(ImVec2(c.x + w, top + r * 0.06f), ImVec2(c.x + w + r * 0.26f, top + r * 0.04f),
                     ImVec2(c.x + w + r * 0.22f, top + r * 0.34f), ImVec2(c.x + w * 0.55f, bowl - r * 0.02f), mid, th);
  // Stem and base.
  dl->AddRectFilled(ImVec2(c.x - r * 0.06f, bowl + r * 0.05f), ImVec2(c.x + r * 0.06f, c.y + r * 0.34f), mid);
  dl->AddRectFilled(ImVec2(c.x - r * 0.26f, c.y + r * 0.34f), ImVec2(c.x + r * 0.26f, c.y + r * 0.46f), light,
                    r * 0.03f);
  // A shadow line on the bowl, and the glint.
  dl->AddLine(ImVec2(c.x - w * 0.72f, top + r * 0.10f), ImVec2(c.x - w * 0.60f, bowl - r * 0.04f), mid, r * 0.04f);
  if (unlocked) {
    const float phase = 0.5f + 0.5f * std::sin(Seconds() * 2.4f);
    const ImVec2 s(c.x + w * 0.45f, top + r * 0.14f);
    const float g = r * (0.10f + 0.05f * phase);
    const ImU32 white = IM_COL32(255, 255, 255, int(160 + 95 * phase));
    dl->AddLine(ImVec2(s.x - g, s.y), ImVec2(s.x + g, s.y), white, r * 0.025f);
    dl->AddLine(ImVec2(s.x, s.y - g), ImVec2(s.x, s.y + g), white, r * 0.025f);
  }
}

void PushFont(float size) { ImGui::PushFont(g_font, size); }

// ------------------------------------------------------------------------------------------------------------------
// The notice: one card at a time, in the lower right corner; the dialog closes itself when nothing is left.

class Notice;
Notice* g_notice = nullptr;  // UI thread only

class Notice final : public rex::ui::ImGuiDialog {
 public:
  Notice() : ImGuiDialog(g_hooks.imgui), icons_(g_hooks.immediate, g_hooks.runtime) {}

  void Push(const AchievementInfo& info) { queue_.push_back({info, -1.0f}); }

 protected:
  void OnClose() override {
    if (g_notice == this) g_notice = nullptr;
  }

  void OnDraw(ImGuiIO& io) override {
    const float now = Seconds();
    while (!queue_.empty()) {
      Item& front = queue_.front();
      if (front.since < 0) front.since = now;
      if (now - front.since < Duration(front.info)) break;
      queue_.pop_front();
    }
    if (queue_.empty()) {
      Close();  // the presenter goes back to its fast path once no dialog is left
      return;
    }
    const Item& item = queue_.front();
    const bool platinum = item.info.id == kPlatinumId;
    const float age = now - item.since, length = Duration(item.info);
    const float alpha = std::clamp(std::min(age / 0.3f, (length - age) / 0.5f), 0.0f, 1.0f);

    const float scale = std::clamp(io.DisplaySize.y / 720.0f, 0.75f, 2.0f);
    const ImVec2 size(platinum ? 520.0f * scale : 430.0f * scale, platinum ? 132.0f * scale : 96.0f * scale);
    const float margin = 24.0f * scale;
    const ImVec2 pos(io.DisplaySize.x - size.x - margin, io.DisplaySize.y - size.y - margin);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    if (ImGui::Begin("##ds2_trophy_notice", nullptr, flags)) {
      ImDrawList* dl = ImGui::GetWindowDrawList();
      auto faded = [alpha](ImU32 c) {
        return (c & 0x00FFFFFFu) | (ImU32(float(c >> 24) * alpha) << 24);
      };
      const ImVec2 a = pos, b(pos.x + size.x, pos.y + size.y);
      dl->AddRectFilled(a, b, faded(kPanel), 10.0f * scale);
      dl->AddRect(a, b, faded(platinum ? kPlatinumMid : kGold), 10.0f * scale, 0, 2.0f * scale);
      const float icon = size.y - 24.0f * scale;
      const ImVec2 icon_min(a.x + 12.0f * scale, a.y + 12.0f * scale);
      if (platinum) {
        DrawPlatinumEmblem(dl, ImVec2(icon_min.x + icon * 0.5f, icon_min.y + icon * 0.5f), icon * 0.5f, true);
      } else if (rex::ui::ImmediateTexture* tex = icons_.GetIcon(item.info)) {
        dl->AddImage(reinterpret_cast<ImTextureID>(tex), icon_min, ImVec2(icon_min.x + icon, icon_min.y + icon),
                     ImVec2(0, 0), ImVec2(1, 1), faded(IM_COL32_WHITE));
      }
      const float tx = icon_min.x + icon + 16.0f * scale;
      PushFont(16.0f * scale);
      dl->AddText(ImVec2(tx, a.y + 14.0f * scale), faded(platinum ? kPlatinumLight : kGold),
                  platinum ? "TROFÉU DE PLATINA" : "Troféu desbloqueado");
      ImGui::PopFont();
      PushFont((platinum ? 24.0f : 20.0f) * scale);
      dl->AddText(ImVec2(tx, a.y + 38.0f * scale), faded(kText), item.info.label.c_str());
      ImGui::PopFont();
      PushFont(15.0f * scale);
      const std::string detail =
          platinum ? std::string("1000G: todos os troféus de Dark Souls II") : fmt_g(item.info.gamerscore);
      dl->AddText(ImVec2(tx, a.y + (platinum ? 76.0f : 66.0f) * scale), faded(kTextDim), detail.c_str());
      ImGui::PopFont();
    }
    ImGui::End();
    ImGui::PopStyleVar();
  }

 private:
  struct Item {
    AchievementInfo info;
    float since;
  };
  static float Duration(const AchievementInfo& info) {
    return info.id == kPlatinumId ? kPlatinumNoticeSeconds : kNoticeSeconds;
  }
  static std::string fmt_g(uint32_t g) { return std::to_string(g) + "G"; }

  std::deque<Item> queue_;
  rex::ui::AchievementIconCache icons_;
};

// ------------------------------------------------------------------------------------------------------------------
// The screen.

class Screen final : public rex::ui::ImGuiDialog {
 public:
  Screen() : ImGuiDialog(g_hooks.imgui), icons_(g_hooks.immediate, g_hooks.runtime) {
#if REX_PLATFORM_SWITCH
    // Our own view of the controller, for B / Minus (HID shared memory can be read by several readers). Read once
    // now so the Minus of the L + Minus that opened the screen does not count as a new press.
    padInitializeAny(&pad_);
    padUpdate(&pad_);
#endif
    if (g_hooks.achievements) EnsurePlatinum(*g_hooks.achievements);
    REXLOG_INFO("[trophies] screen opened");
  }

 protected:
  void OnDraw(ImGuiIO& io) override {
    if (!g_hooks.achievements) return;
    AchievementManager& store = *g_hooks.achievements;
#if REX_PLATFORM_SWITCH
    padUpdate(&pad_);
    if (!close_requested_ && (padGetButtonsDown(&pad_) & (HidNpadButton_B | HidNpadButton_Minus))) RequestClose();
#endif
    if (!close_requested_ && ImGui::IsKeyPressed(ImGuiKey_Escape)) RequestClose();

    const float scale = std::clamp(io.DisplaySize.y / 720.0f, 0.75f, 2.0f);
    const ImVec2 margin(40.0f * scale, 28.0f * scale);
    const ImVec2 size(std::min(io.DisplaySize.x - margin.x * 2.0f, 1080.0f * scale), io.DisplaySize.y - margin.y * 2.0f);
    const ImVec2 pos((io.DisplaySize.x - size.x) * 0.5f, margin.y);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    if (first_frame_) ImGui::SetNextWindowFocus();
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::ColorConvertU32ToFloat4(kPanel));
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(kGoldDim));
    ImGui::PushStyleColor(ImGuiCol_Header, ImGui::ColorConvertU32ToFloat4(IM_COL32(90, 74, 44, 120)));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::ColorConvertU32ToFloat4(IM_COL32(120, 98, 56, 160)));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::ColorConvertU32ToFloat4(IM_COL32(140, 114, 64, 180)));
    ImGui::PushStyleColor(ImGuiCol_NavCursor, ImGui::ColorConvertU32ToFloat4(kGold));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f * scale, 18.0f * scale));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize;
    if (ImGui::Begin("##ds2_trophies", nullptr, flags)) {
      DrawHeader(store, scale);
      DrawList(store, scale, size.y);
      PushFont(14.0f * scale);
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(kTextDim));
      ImGui::TextUnformatted("Direcional: navegar     B: fechar     L + Menos: abrir / fechar");
      ImGui::PopStyleColor();
      ImGui::PopFont();
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(6);
    first_frame_ = false;
  }

 private:
  void RequestClose() {
    close_requested_ = true;
    // The SDK owns this dialog (bind_achievements): its toggle destroys it, after this draw.
    if (g_hooks.on_ui_thread && g_hooks.toggle_screen) g_hooks.on_ui_thread(g_hooks.toggle_screen);
  }

  void DrawHeader(AchievementManager& store, float scale) {
    const Progress p = CountGame(store);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    PushFont(30.0f * scale);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(kGold));
    ImGui::TextUnformatted("TROFÉUS");
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::SameLine();
    PushFont(18.0f * scale);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(kTextDim));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f * scale);
    ImGui::Text("Dark Souls II     %d / %d     %dG / %dG", p.unlocked, p.total, p.earned_g, p.total_g);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    // Progress bar.
    const ImVec2 bar_min = ImGui::GetCursorScreenPos();
    const float bar_w = ImGui::GetContentRegionAvail().x, bar_h = 8.0f * scale;
    const float frac = p.total ? float(p.unlocked) / float(p.total) : 0.0f;
    dl->AddRectFilled(bar_min, ImVec2(bar_min.x + bar_w, bar_min.y + bar_h), IM_COL32(50, 44, 34, 255), bar_h);
    if (frac > 0)
      dl->AddRectFilled(bar_min, ImVec2(bar_min.x + bar_w * frac, bar_min.y + bar_h), kGold, bar_h);
    ImGui::Dummy(ImVec2(bar_w, bar_h + 12.0f * scale));

    // The platinum card.
    if (REXCVAR_GET(ds2_trophy_platinum)) {
      const bool got = store.IsUnlocked(kPlatinumId);
      const ImVec2 card_min = ImGui::GetCursorScreenPos();
      const float card_h = 112.0f * scale, card_w = ImGui::GetContentRegionAvail().x;
      const ImVec2 card_max(card_min.x + card_w, card_min.y + card_h);
      dl->AddRectFilled(card_min, card_max, got ? IM_COL32(60, 64, 74, 200) : IM_COL32(34, 32, 30, 200), 10.0f * scale);
      dl->AddRect(card_min, card_max, got ? kPlatinumMid : IM_COL32(70, 66, 60, 255), 10.0f * scale, 0, 2.0f * scale);
      const float r = card_h * 0.40f;
      DrawPlatinumEmblem(dl, ImVec2(card_min.x + 16.0f * scale + r, card_min.y + card_h * 0.5f), r, got);
      const float tx = card_min.x + 32.0f * scale + r * 2.0f;
      PushFont(24.0f * scale);
      dl->AddText(ImVec2(tx, card_min.y + 16.0f * scale), got ? kPlatinumLight : kTextDim, kPlatinumName);
      ImGui::PopFont();
      PushFont(16.0f * scale);
      dl->AddText(ImVec2(tx, card_min.y + 50.0f * scale), got ? kText : kTextDim,
                  got ? "Todos os troféus de Dark Souls II: 1000G." : "Desbloqueie todos os outros troféus do jogo.");
      const std::string status =
          got ? "Platina em " + DateOf(store.GetUnlockTime(kPlatinumId))
              : std::to_string(p.unlocked) + " de " + std::to_string(p.total) + " troféus";
      dl->AddText(ImVec2(tx, card_min.y + 76.0f * scale), got ? kPlatinumMid : kGoldDim, status.c_str());
      ImGui::PopFont();
      ImGui::Dummy(ImVec2(card_w, card_h + 10.0f * scale));
    }
  }

  void DrawList(AchievementManager& store, float scale, float window_h) {
    std::vector<AchievementInfo> list = store.ListAchievements();
    list.erase(std::remove_if(list.begin(), list.end(), [](const AchievementInfo& a) { return a.id == kPlatinumId; }),
               list.end());
    // Unlocked first (most recent on top), then the locked ones in the game's order.
    std::stable_sort(list.begin(), list.end(), [&](const AchievementInfo& x, const AchievementInfo& y) {
      const bool ux = store.IsUnlocked(x.id), uy = store.IsUnlocked(y.id);
      if (ux != uy) return ux;
      if (ux) return store.GetUnlockTime(x.id) > store.GetUnlockTime(y.id);
      return x.id < y.id;
    });
    const float footer = 30.0f * scale;
    ImGui::BeginChild("##ds2_trophy_list", ImVec2(0, ImGui::GetContentRegionAvail().y - footer),
                      ImGuiChildFlags_NavFlattened);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float icon = 64.0f * scale, row_h = icon + 16.0f * scale;
    for (const AchievementInfo& a : list) {
      const bool got = store.IsUnlocked(a.id);
      ImGui::PushID(int(a.id));
      const ImVec2 row_min = ImGui::GetCursorScreenPos();
      const float row_w = ImGui::GetContentRegionAvail().x;
      // A selectable row: the D-pad moves between rows and the list scrolls to the one selected.
      ImGui::Selectable("##row", false, ImGuiSelectableFlags_None, ImVec2(row_w, row_h));
      if (got) dl->AddRectFilled(row_min, ImVec2(row_min.x + row_w, row_min.y + row_h), kRowUnlocked, 8.0f * scale);
      const ImVec2 icon_min(row_min.x + 8.0f * scale, row_min.y + 8.0f * scale);
      if (rex::ui::ImmediateTexture* tex = icons_.GetIcon(a)) {
        dl->AddImage(reinterpret_cast<ImTextureID>(tex), icon_min, ImVec2(icon_min.x + icon, icon_min.y + icon),
                     ImVec2(0, 0), ImVec2(1, 1), got ? IM_COL32_WHITE : IM_COL32(90, 90, 90, 255));
      } else {
        dl->AddRectFilled(icon_min, ImVec2(icon_min.x + icon, icon_min.y + icon), IM_COL32(40, 40, 40, 255));
      }
      const float tx = icon_min.x + icon + 16.0f * scale;
      const float right = row_min.x + row_w - 12.0f * scale;
      PushFont(19.0f * scale);
      dl->AddText(ImVec2(tx, row_min.y + 10.0f * scale), got ? kText : kTextDim, a.label.c_str());
      const std::string g = std::to_string(a.gamerscore) + "G";
      const float gw = ImGui::CalcTextSize(g.c_str()).x;
      dl->AddText(ImVec2(right - gw, row_min.y + 10.0f * scale), got ? kGold : kGoldDim, g.c_str());
      ImGui::PopFont();
      PushFont(15.0f * scale);
      const std::string& desc = got || a.unachieved_description.empty() ? a.description : a.unachieved_description;
      dl->AddText(nullptr, 0.0f, ImVec2(tx, row_min.y + 38.0f * scale), kTextDim, desc.c_str(), nullptr,
                  right - tx - 120.0f * scale);
      if (got) {
        const std::string date = DateOf(store.GetUnlockTime(a.id));
        const float dw = ImGui::CalcTextSize(date.c_str()).x;
        dl->AddText(ImVec2(right - dw, row_min.y + 40.0f * scale), kGoldDim, date.c_str());
      }
      ImGui::PopFont();
      ImGui::PopID();
    }
    (void)window_h;
    ImGui::EndChild();
  }

  rex::ui::AchievementIconCache icons_;
  bool first_frame_ = true;
  bool close_requested_ = false;
#if REX_PLATFORM_SWITCH
  PadState pad_{};
#endif
};

std::mutex g_toggle_mutex;

}  // namespace

void Install(Hooks hooks) {
  g_hooks = std::move(hooks);
  if (!g_hooks.achievements || !g_hooks.on_ui_thread) return;
  // A notice per unlock (the store calls this on the guest thread that unlocked it).
  g_hooks.achievements->RegisterNotificationCallback([](const AchievementEvent& event) {
    const AchievementInfo info = event.achievement;
    g_hooks.on_ui_thread([info] {
      if (REXCVAR_GET(ds2_trophy_notices) && g_hooks.imgui) {
        if (!g_notice) g_notice = new Notice();  // deletes itself when its queue is empty
        g_notice->Push(info);
      }
      REXLOG_INFO("[trophies] unlocked: {} ({}G)", info.label, info.gamerscore);
      if (info.id != kPlatinumId) CheckPlatinum();
    });
  });
  // A profile that already had everything before the platinum existed gets it at the first check.
  g_hooks.on_ui_thread([] { CheckPlatinum(); });
}

std::unique_ptr<rex::ui::ImGuiDialog> CreateScreen() {
  if (!g_hooks.imgui) return nullptr;
  if (g_hooks.achievements) CheckPlatinum();
  return std::make_unique<Screen>();
}

void TestNotices() {
  if (!g_hooks.on_ui_thread) return;
  g_hooks.on_ui_thread([] {
    if (!g_hooks.imgui || !g_hooks.achievements) return;
    std::vector<AchievementInfo> list = g_hooks.achievements->ListAchievements();
    if (!g_notice) g_notice = new Notice();
    for (const AchievementInfo& a : list)
      if (a.id != kPlatinumId) {
        g_notice->Push(a);
        break;
      }
    g_notice->Push(PlatinumInfo());
    REXLOG_INFO("[trophies] test notices");
  });
}

void RequestToggle() {
  std::lock_guard<std::mutex> lock(g_toggle_mutex);
  if (g_hooks.on_ui_thread && g_hooks.toggle_screen) g_hooks.on_ui_thread(g_hooks.toggle_screen);
}

void ConfigureFonts(ImFontAtlas* atlas) {
  ImFontConfig config;
  config.FontDataOwnedByAtlas = false;
  config.OversampleH = 2;
  config.OversampleV = 1;
#if REX_PLATFORM_SWITCH
  // The console's own font (shared memory of the pl service): nothing is shipped with the port.
  PlFontData font{};
  if (R_SUCCEEDED(plInitialize(PlServiceType_User)) &&
      R_SUCCEEDED(plGetSharedFontByType(&font, PlSharedFontType_Standard)) && font.address) {
    g_font = atlas->AddFontFromMemoryTTF(font.address, int(font.size), 20.0f, &config);
  }
#else
  for (const char* path : {"C:\\Windows\\Fonts\\segoeui.ttf", "C:\\Windows\\Fonts\\arial.ttf"}) {
    if (std::filesystem::exists(path)) {
      config.FontDataOwnedByAtlas = true;
      g_font = atlas->AddFontFromFileTTF(path, 20.0f, &config);
      break;
    }
  }
#endif
  REXLOG_INFO("[trophies] font for the trophy screen: {}", g_font ? "loaded" : "ImGui's default");
}

}  // namespace ds2::trophies
