// Dark Souls II: the trophy screen, the unlock notices and our platinum trophy. See ds2_trophies.h.
//
// Both are drawn as the game's own menus (ds2_trophy_art: the pieces of its menu pages and its font, read from the
// player's files): the screen as its menu window (Player Status, Inventory), the notice as its dialogs.

#include "ds2_trophies.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

#include <imgui.h>

#include <cstdio>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/platform.h>
#include <rex/system/achievement_manager.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>
#include <rex/ui/immediate_drawer.h>
#include <rex/ui/overlay/achievement_icon_cache.h>

#include "ds2_trophy_art.h"

#if REX_PLATFORM_SWITCH
#include <switch.h>
#endif

REXCVAR_DECLARE(uint32_t, user_language);  // sdk/src/kernel/xam/xam_user.cpp
REXCVAR_DECLARE(std::string, ds2_trophy_dump);  // ds2_trophy_art.cpp

REXCVAR_DEFINE_BOOL(ds2_trophy_notices, true, "DS2",
                    "Show a notice in the corner when a trophy unlocks (it closes itself after a few seconds; while "
                    "one is on screen the presenter takes its slower path)");
REXCVAR_DEFINE_BOOL(ds2_trophy_platinum, true, "DS2",
                    "Our platinum trophy: unlocked when every achievement of the game is unlocked (1000G)");

namespace ds2::trophies {
namespace {

namespace art = ds2::trophy_art;

using rex::system::AchievementEvent;
using rex::system::AchievementInfo;
using rex::system::AchievementManager;

Hooks g_hooks;
ImFont* g_font = nullptr;  // the system font (Switch) or Segoe UI (PC), for the texts the game's font lacks

constexpr float kNoticeSeconds = 5.0f;
constexpr float kPlatinumNoticeSeconds = 8.0f;

// The game's menu text colours (measured on its own screens) and the platinum's.
const ImU32 kWhite = IM_COL32(232, 230, 224, 255);
const ImU32 kText = IM_COL32(210, 206, 196, 255);
const ImU32 kTextDim = IM_COL32(150, 146, 138, 255);
const ImU32 kTextFaint = IM_COL32(104, 101, 96, 255);
const ImU32 kWarm = IM_COL32(224, 204, 146, 255);
const ImU32 kPlatinumLight = IM_COL32(236, 238, 242, 255);
const ImU32 kPlatinumMid = IM_COL32(170, 176, 188, 255);
const ImU32 kPlatinumDark = IM_COL32(96, 102, 116, 255);

// Sizes of the game's font in its menus, at 720 lines (the font is 28 pixels a line).
constexpr float kBody = 0.6f;
constexpr float kSmall = 0.52f;
constexpr float kTitle = 0.62f;

// The screen's own words follow the language the game runs in (user_language, the same choice that picks the
// achievement names from the XDBF): the eight languages of Dark Souls II, English for any other. The key guide is
// the game's own (common.fmg); 'footer' stands in for it when that file is not there.
struct Words {
  const char* unlocked;        // notice title
  const char* platinum_title;  // notice title of the platinum
  const char* platinum_name;
  const char* platinum_got;    // what it is for
  const char* platinum_hint;   // how to get it
  const char* screen_title;
  const char* platinum_count;  // "%d", "%d" = unlocked, total
  const char* footer;
  const char* date_format;     // strftime
  const char* help;            // the help line under the window, as every menu of the game has
  const char* col_trophy;
  const char* col_completion;
  const char* col_gamerscore;
  const char* unlocked_on;     // "%s" = the date
  const char* locked;
  const char* platinum;        // the platinum row's value
};

constexpr Words kEnglish = {
    "Trophy unlocked", "Platinum trophy", "Bearer of the Curse", "Every trophy in Dark Souls II: 1000G.",
    "Unlock every other trophy in the game.", "Trophies", "%d of %d trophies", "D-pad: Select   B: Back",
    "%m/%d/%Y", "Check trophies.", "Trophy", "Completion", "Gamerscore", "Unlocked %s", "Locked", "Platinum"};
constexpr Words kJapanese = {
    "トロフィー獲得", "プラチナトロフィー", "呪いを受けし者", "DARK SOULS II のすべてのトロフィー：1000G",
    "ほかのすべてのトロフィーを獲得する", "トロフィー", "%d / %d トロフィー", "十字ボタン：選択　B：戻る", "%Y/%m/%d",
    "トロフィーを確認する", "トロフィー", "達成率", "ゲーマースコア", "%s に獲得", "未獲得", "プラチナ"};
constexpr Words kGerman = {
    "Trophäe freigeschaltet", "Platin-Trophäe", "Träger des Fluchs", "Alle Trophäen von Dark Souls II: 1000G.",
    "Schalte alle anderen Trophäen des Spiels frei.", "Trophäen", "%d von %d Trophäen",
    "Steuerkreuz: Auswählen   B: Zurück", "%d.%m.%Y", "Trophäen ansehen.", "Trophäe", "Fortschritt", "Gamerscore",
    "Freigeschaltet am %s", "Nicht freigeschaltet", "Platin"};
constexpr Words kFrench = {
    "Trophée débloqué", "Trophée platine", "Porteur de la Malédiction", "Tous les trophées de Dark Souls II : 1000G.",
    "Débloquez tous les autres trophées du jeu.", "Trophées", "%d trophées sur %d", "Croix : Choisir   B : Retour",
    "%d/%m/%Y", "Consulter les trophées.", "Trophée", "Progression", "Gamerscore", "Débloqué le %s", "Verrouillé",
    "Platine"};
constexpr Words kSpanish = {
    "Trofeo desbloqueado", "Trofeo de platino", "Portador de la Maldición", "Todos los trofeos de Dark Souls II: 1000G.",
    "Desbloquea todos los demás trofeos del juego.", "Trofeos", "%d de %d trofeos", "Cruceta: Elegir   B: Atrás",
    "%d/%m/%Y", "Consultar los trofeos.", "Trofeo", "Progreso", "Gamerscore", "Desbloqueado el %s", "Bloqueado",
    "Platino"};
constexpr Words kItalian = {
    "Trofeo sbloccato", "Trofeo di platino", "Portatore della Maledizione", "Tutti i trofei di Dark Souls II: 1000G.",
    "Sblocca tutti gli altri trofei del gioco.", "Trofei", "%d di %d trofei", "Croce direzionale: Scegli   B: Indietro",
    "%d/%m/%Y", "Consulta i trofei.", "Trofeo", "Completamento", "Gamerscore", "Sbloccato il %s", "Bloccato",
    "Platino"};
constexpr Words kPolish = {
    "Trofeum odblokowane", "Platynowe trofeum", "Nosiciel Klątwy", "Wszystkie trofea Dark Souls II: 1000G.",
    "Odblokuj wszystkie pozostałe trofea w grze.", "Trofea", "%d z %d trofeów", "Krzyżak: Wybierz   B: Wróć",
    "%d.%m.%Y", "Sprawdź trofea.", "Trofeum", "Postęp", "Gamerscore", "Odblokowano %s", "Zablokowane", "Platyna"};
constexpr Words kRussian = {
    "Трофей получен", "Платиновый трофей", "Носитель проклятия", "Все трофеи Dark Souls II: 1000G.",
    "Получите все остальные трофеи игры.", "Трофеи", "%d из %d трофеев", "Крестовина: выбор   B: назад", "%d.%m.%Y",
    "Просмотр трофеев.", "Трофей", "Прогресс", "Очки", "Получен %s", "Не получен", "Платина"};

// XLanguage numbers (the SDK's user_language): 1 English, 2 Japanese, 3 German, 4 French, 5 Spanish, 6 Italian,
// 11 Polish, 12 Russian.
const Words& T() {
  switch (REXCVAR_GET(user_language)) {
    case 2: return kJapanese;
    case 3: return kGerman;
    case 4: return kFrench;
    case 5: return kSpanish;
    case 6: return kItalian;
    case 11: return kPolish;
    case 12: return kRussian;
    default: return kEnglish;
  }
}

std::string Format(const char* format, int a, int b) {
  char text[160];
  std::snprintf(text, sizeof(text), format, a, b);
  return text;
}

std::string Format(const char* format, const std::string& value) {
  char text[160];
  std::snprintf(text, sizeof(text), format, value.c_str());
  return text;
}

AchievementInfo PlatinumInfo() {
  AchievementInfo info;
  info.id = kPlatinumId;
  info.label = T().platinum_name;
  info.description = T().platinum_got;
  info.unachieved_description = T().platinum_hint;
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
  std::strftime(text, sizeof(text), T().date_format, &local);
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

// PC test (ds2_trophy_dump): a frame's draw data written as its draw lists and the textures the game's art made, for
// tools/features/raster_dump.py. Layout: "DS2D", display w, h (f32), list count; per list: vertex count, vertices
// (pos, uv: f32, colour: u32), index count, indices (u32), command count, commands (clip rect 4 f32, texture u64,
// index offset, element count, vertex offset: u32); then texture count, per texture: id u64, w, h (u32), RGBA.
void DumpDrawData(const char* name, const ImDrawData* data) {
  const std::string dir = REXCVAR_GET(ds2_trophy_dump);
  if (dir.empty() || !data) return;
  const std::string path = dir + "/" + name + ".bin";
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return;
  auto u32 = [f](uint32_t v) { std::fwrite(&v, 4, 1, f); };
  auto f32 = [f](float v) { std::fwrite(&v, 4, 1, f); };
  std::fwrite("DS2D", 1, 4, f);
  f32(data->DisplaySize.x), f32(data->DisplaySize.y);
  u32(uint32_t(data->CmdLists.Size));
  std::vector<ImTextureID> textures;
  for (const ImDrawList* dl : data->CmdLists) {
    u32(uint32_t(dl->VtxBuffer.Size));
    for (const ImDrawVert& v : dl->VtxBuffer) f32(v.pos.x), f32(v.pos.y), f32(v.uv.x), f32(v.uv.y), u32(v.col);
    u32(uint32_t(dl->IdxBuffer.Size));
    for (ImDrawIdx i : dl->IdxBuffer) u32(uint32_t(i));
    u32(uint32_t(dl->CmdBuffer.Size));
    for (const ImDrawCmd& c : dl->CmdBuffer) {
      f32(c.ClipRect.x), f32(c.ClipRect.y), f32(c.ClipRect.z), f32(c.ClipRect.w);
      const ImTextureID t = c.TexRef._TexData ? ImTextureID_Invalid : c.TexRef._TexID;  // ImGui's own font: none
      const uint64_t id = uint64_t(t);
      std::fwrite(&id, 8, 1, f);
      u32(c.IdxOffset), u32(c.ElemCount), u32(c.VtxOffset);
      if (std::find(textures.begin(), textures.end(), t) == textures.end()) textures.push_back(t);
    }
  }
  std::vector<std::tuple<ImTextureID, int, int, const uint8_t*>> known;
  for (ImTextureID t : textures) {
    int w = 0, h = 0;
    const uint8_t* rgba = nullptr;
    if (art::CpuPixels(t, w, h, rgba)) known.emplace_back(t, w, h, rgba);
  }
  u32(uint32_t(known.size()));
  for (auto& [t, w, h, rgba] : known) {
    const uint64_t id = uint64_t(t);
    std::fwrite(&id, 8, 1, f);
    u32(uint32_t(w)), u32(uint32_t(h));
    std::fwrite(rgba, 1, size_t(w) * h * 4, f);
  }
  std::fclose(f);
  REXLOG_INFO("[trophies] {} written ({} textures known of {})", path, known.size(), textures.size());
}

ImU32 Faded(ImU32 c, float a) { return (c & 0x00FFFFFFu) | (ImU32(float(c >> 24) * a) << 24); }

// The game's screens are laid out for 1280x720: a point of that frame, centred, at the display's size.
struct Frame {
  float s, ox;
  explicit Frame(const ImGuiIO& io)
      : s(io.DisplaySize.y / 720.0f), ox((io.DisplaySize.x - 1280.0f * (io.DisplaySize.y / 720.0f)) * 0.5f) {}
  ImVec2 operator()(float x, float y) const { return ImVec2(ox + x * s, y * s); }
};

// A trophy's icon in an item tile, as the game's item lists show them.
void DrawIcon(ImDrawList* dl, rex::ui::AchievementIconCache& icons, const AchievementInfo& a, ImVec2 tile, float s,
              bool lit, bool got) {
  art::Draw(dl, lit ? art::Piece::kTileLit : art::Piece::kTile, tile, ImVec2(tile.x + 29 * s, tile.y + 32 * s));
  const ImVec2 i0(tile.x + 3 * s, tile.y + 4 * s), i1(i0.x + 23 * s, i0.y + 23 * s);
  if (a.id == kPlatinumId) {
    DrawPlatinumEmblem(dl, ImVec2((i0.x + i1.x) * 0.5f, (i0.y + i1.y) * 0.5f), 12.5f * s, got);
  } else if (rex::ui::ImmediateTexture* tex = icons.GetIcon(a)) {
    dl->AddImage(reinterpret_cast<ImTextureID>(tex), i0, i1, ImVec2(0, 0), ImVec2(1, 1),
                 got ? IM_COL32_WHITE : IM_COL32(95, 95, 95, 255));
  }
}

// ------------------------------------------------------------------------------------------------------------------
// The notice: one at a time, in the lower right corner, as the game's own dialogs: a leather strip between the
// dialog frame's two lines. The dialog closes itself when nothing is left.

void DrawNoticeCard(ImGuiIO& io, const AchievementInfo& info, float alpha, rex::ui::AchievementIconCache& icons) {
  const bool platinum = info.id == kPlatinumId;
  const float s = io.DisplaySize.y / 720.0f;
  const float w = 420.0f * s, h = 104.0f * s;
  const ImVec2 a(io.DisplaySize.x - w - 36.0f * s, io.DisplaySize.y - h - 120.0f * s), b(a.x + w, a.y + h);
  ImGui::SetNextWindowPos(ImVec2(a.x - 8 * s, a.y - 12 * s), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(w + 16 * s, h + 20 * s), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.0f);
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs |
                                 ImGuiWindowFlags_NoFocusOnAppearing;
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  if (ImGui::Begin("##ds2_trophy_notice", nullptr, flags)) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 tint = Faded(IM_COL32_WHITE, alpha);
    art::Draw(dl, art::Piece::kLeather, a, b, Faded(IM_COL32_WHITE, alpha * 0.95f));
    art::Draw(dl, art::Piece::kFrameTop, ImVec2(a.x - 4 * s, a.y - 8 * s), ImVec2(b.x + 4 * s, a.y + 32 * s), tint);
    art::Draw(dl, art::Piece::kFrameBottom, ImVec2(a.x - 4 * s, b.y - 30 * s), ImVec2(b.x + 4 * s, b.y + 4 * s), tint);
    const ImVec2 tile(a.x + 18 * s, a.y + 26 * s);
    art::Draw(dl, art::Piece::kTile, tile, ImVec2(tile.x + 46 * s, tile.y + 52 * s), tint);
    const ImVec2 i0(a.x + 22 * s, a.y + 31 * s), i1(i0.x + 38 * s, i0.y + 38 * s);
    if (platinum) {
      DrawPlatinumEmblem(dl, ImVec2((i0.x + i1.x) * 0.5f, (i0.y + i1.y) * 0.5f), 19.0f * s, true);
    } else if (rex::ui::ImmediateTexture* tex = icons.GetIcon(info)) {
      dl->AddImage(reinterpret_cast<ImTextureID>(tex), i0, i1, ImVec2(0, 0), ImVec2(1, 1), tint);
    }
    const float tx = a.x + 78 * s;
    art::Text(dl, ImVec2(tx, a.y + 24 * s), platinum ? T().platinum_title : T().unlocked, kSmall * s,
              Faded(platinum ? kWarm : kTextDim, alpha));
    art::Text(dl, ImVec2(tx, a.y + 44 * s), info.label, 0.7f * s, Faded(kWhite, alpha));
    const std::string value = platinum ? std::string(T().platinum) : std::to_string(info.gamerscore) + "G";
    art::Text(dl, ImVec2(b.x - 22 * s - art::TextWidth(value, kBody * s), a.y + 46 * s), value, kBody * s,
              Faded(platinum ? kWarm : kText, alpha));
  }
  ImGui::End();
  ImGui::PopStyleVar();
}

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
    const float age = now - item.since, length = Duration(item.info);
    DrawNoticeCard(io, item.info, std::clamp(std::min(age / 0.3f, (length - age) / 0.5f), 0.0f, 1.0f), icons_);
  }

 private:
  struct Item {
    AchievementInfo info;
    float since;
  };
  static float Duration(const AchievementInfo& info) {
    return info.id == kPlatinumId ? kPlatinumNoticeSeconds : kNoticeSeconds;
  }

  std::deque<Item> queue_;
  rex::ui::AchievementIconCache icons_;
};

// ------------------------------------------------------------------------------------------------------------------
// The screen, as one of the game's menu windows (Player Status, Inventory: the same window, at the same place): the
// title on the blue strip, a summary row, the list on the game's row bands with its tiles and the orange glow of the
// selected row, the description box of the selected trophy, and under the window the help line and the key guide.

constexpr int kVisibleRows = 7;
constexpr float kRowHeight = 34.0f;

// The screen's drawing, apart from the dialog (the PC test draws it in a context of its own).
class ScreenView {
 public:
  ScreenView() : icons_(g_hooks.immediate, g_hooks.runtime) {}

  // A row shown selected when none has the focus (the PC test, whose context has no navigation).
  int preview_selected = -1;

  void Draw(ImGuiIO& io, AchievementManager& store) {
    const Frame P(io);
    const float s = P.s;
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    if (first_frame_) ImGui::SetNextWindowFocus();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    for (ImGuiCol c : {ImGuiCol_Header, ImGuiCol_HeaderHovered, ImGuiCol_HeaderActive, ImGuiCol_NavCursor})
      ImGui::PushStyleColor(c, ImVec4(0, 0, 0, 0));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoBackground;
    if (ImGui::Begin("##ds2_trophies", nullptr, flags)) {
      ImDrawList* dl = ImGui::GetWindowDrawList();
      dl->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(0, 0, 0, 90));  // the game dims the world a little
      art::Draw(dl, art::Piece::kWindow, P(84, 98), P(896, 599));
      DrawHeader(store, dl, P);
      const AchievementInfo* selected = DrawList(store, dl, P);
      if (selected) DrawDescription(store, dl, P, *selected);
      art::Text(dl, P(96, 620), T().help, kBody * s, kText);
      const std::string select = art::Message(10000), back = art::Message(10002);
      art::Text(dl, P(96, 645), select.empty() || back.empty() ? std::string(T().footer) : select + "   " + back,
                kBody * s, kText);
    }
    ImGui::End();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(3);
    first_frame_ = false;
  }

 private:
  void DrawHeader(AchievementManager& store, ImDrawList* dl, const Frame& P) {
    const float s = P.s;
    const Progress p = CountGame(store);
    // The title: the game's first achievement's icon (the game's titles carry an icon), then the name.
    if (const auto first = store.FindAchievement(1)) {
      if (rex::ui::ImmediateTexture* tex = icons_.GetIcon(*first)) {
        const ImVec2 i0 = P(110, 126);
        dl->AddImage(reinterpret_cast<ImTextureID>(tex), i0, ImVec2(i0.x + 21 * s, i0.y + 21 * s));
      }
    }
    art::Text(dl, P(138, 126), T().screen_title, kTitle * s, kWhite);
    // The summary row, as Player Status' first row: three cells on a blue strip.
    art::Draw(dl, art::Piece::kStrip, P(102, 158), P(878, 182));
    const int percent = p.total ? (100 * p.unlocked + p.total / 2) / p.total : 0;
    const std::string cells[3][2] = {
        {T().col_trophy, std::to_string(p.unlocked) + " / " + std::to_string(p.total)},
        {T().col_completion, std::to_string(percent) + "%"},
        {T().col_gamerscore, std::to_string(p.earned_g) + " / " + std::to_string(p.total_g)}};
    const float cell = 776.0f / 3.0f;
    for (int k = 0; k < 3; ++k) {
      const float x = 102 + k * cell;
      art::Text(dl, P(x + 12, 161), cells[k][0], kSmall * s, kTextDim);
      const ImVec2 v = P(x + cell - 14, 160);
      art::Text(dl, ImVec2(v.x - art::TextWidth(cells[k][1], kBody * s), v.y), cells[k][1], kBody * s, kWhite);
    }
    // Progress: the orange gauge in a framed track.
    art::Draw(dl, art::Piece::kTrack, P(102, 188), P(878, 202));
    if (p.unlocked > 0 && p.total > 0) {
      const float fill = std::max(14.0f, 768.0f * float(p.unlocked) / float(p.total));
      art::Draw(dl, art::Piece::kFill, P(106, 191), P(106 + fill, 199), IM_COL32_WHITE, 6.0f);
    }
    // The list's header strip.
    art::Draw(dl, art::Piece::kStrip, P(102, 210), P(878, 232));
    art::Text(dl, P(114, 212), T().col_trophy, kSmall * s, kText);
    const ImVec2 g = P(866, 212);
    art::Text(dl, ImVec2(g.x - art::TextWidth(T().col_gamerscore, kSmall * s), g.y), T().col_gamerscore, kSmall * s,
              kText);
  }

  // The rows; returns the selected trophy.
  const AchievementInfo* DrawList(AchievementManager& store, ImDrawList* dl, const Frame& P) {
    const float s = P.s;
    list_ = store.ListAchievements();
    list_.erase(std::remove_if(list_.begin(), list_.end(),
                               [](const AchievementInfo& a) { return a.id == kPlatinumId; }),
                list_.end());
    // Unlocked first (most recent on top), then the locked ones in the game's order; the platinum heads the list.
    std::stable_sort(list_.begin(), list_.end(), [&](const AchievementInfo& x, const AchievementInfo& y) {
      const bool ux = store.IsUnlocked(x.id), uy = store.IsUnlocked(y.id);
      if (ux != uy) return ux;
      if (ux) return store.GetUnlockTime(x.id) > store.GetUnlockTime(y.id);
      return x.id < y.id;
    });
    if (REXCVAR_GET(ds2_trophy_platinum))
      if (const auto plat = store.FindAchievement(kPlatinumId)) list_.insert(list_.begin(), *plat);

    const float row_h = kRowHeight * s, width = 774.0f * s;
    ImGui::SetCursorScreenPos(P(96, 236));
    ImGui::BeginChild("##ds2_trophy_list", ImVec2(width, kVisibleRows * row_h), ImGuiChildFlags_NavFlattened,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
    ImDrawList* cdl = ImGui::GetWindowDrawList();
    const AchievementInfo* selected = nullptr;
    for (size_t k = 0; k < list_.size(); ++k) {
      const AchievementInfo& a = list_[k];
      const bool got = store.IsUnlocked(a.id);
      ImGui::PushID(int(k));
      const ImVec2 r = ImGui::GetCursorScreenPos();
      ImGui::Selectable("##row", false, ImGuiSelectableFlags_None, ImVec2(width, row_h));
      if (first_frame_ && k == 0) ImGui::SetItemDefaultFocus();
      const bool lit = ImGui::IsItemFocused() || int(k) == preview_selected;
      ImGui::PopID();
      if (lit) selected = &a;
      art::Draw(cdl, art::Piece::kRowBand, ImVec2(r.x + 6 * s, r.y + 2 * s), ImVec2(r.x + 766 * s, r.y + 32 * s),
                IM_COL32(255, 255, 255, 218));
      if (lit)
        art::Draw(cdl, art::Piece::kGlow, ImVec2(r.x + 2 * s, r.y - 1 * s), ImVec2(r.x + 772 * s, r.y + 35 * s),
                  IM_COL32_WHITE, 30.0f);
      DrawIcon(cdl, icons_, a, ImVec2(r.x + 14 * s, r.y + 1 * s), s, lit, got);
      const ImU32 color = got ? kWhite : kTextFaint;
      art::Text(cdl, ImVec2(r.x + 54 * s, r.y + 6 * s), a.label, kBody * s, color);
      const std::string value = a.id == kPlatinumId ? std::string(T().platinum) : std::to_string(a.gamerscore) + "G";
      art::Text(cdl, ImVec2(r.x + 760 * s - art::TextWidth(value, kBody * s), r.y + 6 * s), value, kBody * s,
                a.id == kPlatinumId && got ? kWarm : color);
    }
    const float scroll = ImGui::GetScrollY(), scroll_max = ImGui::GetScrollMaxY();
    ImGui::EndChild();
    // The scroll mark, as the game's lists: a thin track and its thumb.
    const ImVec2 t0 = P(872, 236), t1 = P(875, 236 + kVisibleRows * kRowHeight);
    dl->AddRectFilled(t0, t1, IM_COL32(40, 37, 33, 230));
    if (scroll_max > 0.0f && !list_.empty()) {
      const float frac = std::min(1.0f, float(kVisibleRows) / float(list_.size()));
      const float h = (t1.y - t0.y) * frac, y = t0.y + (t1.y - t0.y - h) * (scroll / scroll_max);
      dl->AddRectFilled(ImVec2(t0.x, y), ImVec2(t1.x, y + h), IM_COL32(150, 144, 132, 255));
    }
    if (!selected && !list_.empty()) selected = &list_.front();
    return selected;
  }

  // The selected trophy, as Inventory's description box: the name strip with its icon, then the text.
  void DrawDescription(AchievementManager& store, ImDrawList* dl, const Frame& P, const AchievementInfo& a) {
    const float s = P.s;
    const bool got = store.IsUnlocked(a.id);
    art::Draw(dl, art::Piece::kNameStrip, P(102, 482), P(878, 508));
    const ImVec2 i0 = P(106, 485), i1(i0.x + 20 * s, i0.y + 20 * s);
    if (a.id == kPlatinumId) {
      DrawPlatinumEmblem(dl, ImVec2((i0.x + i1.x) * 0.5f, (i0.y + i1.y) * 0.5f), 10.0f * s, got);
    } else if (rex::ui::ImmediateTexture* tex = icons_.GetIcon(a)) {
      dl->AddImage(reinterpret_cast<ImTextureID>(tex), i0, i1, ImVec2(0, 0), ImVec2(1, 1),
                   got ? IM_COL32_WHITE : IM_COL32(95, 95, 95, 255));
    }
    art::Text(dl, P(136, 486), a.label, kBody * s, kWhite);
    const std::string& desc = got || a.unachieved_description.empty() ? a.description : a.unachieved_description;
    const int lines = art::TextWrapped(dl, P(114, 516), desc, kBody * s, kText, 744.0f * s, 22.0f * s);
    std::string status;
    if (got) {
      status = Format(T().unlocked_on, DateOf(store.GetUnlockTime(a.id)));
    } else if (a.id == kPlatinumId) {
      const Progress p = CountGame(store);
      status = Format(T().platinum_count, p.unlocked, p.total);
    } else {
      status = T().locked;
    }
    art::Text(dl, P(114, 518 + 22.0f * float(std::max(1, lines))), status, kSmall * s, got ? kTextDim : kTextFaint);
  }

  rex::ui::AchievementIconCache icons_;
  std::vector<AchievementInfo> list_;
  bool first_frame_ = true;
};

class Screen final : public rex::ui::ImGuiDialog {
 public:
  Screen() : ImGuiDialog(g_hooks.imgui) {
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
#if REX_PLATFORM_SWITCH
    padUpdate(&pad_);
    // B closes; Minus closes too, but not while L is held: L + Minus is the shortcut (ds2_render_mode.cpp), which
    // toggles the screen by itself. Both at once closed it and opened it again in the same instant.
    const u64 down = padGetButtonsDown(&pad_);
    const bool with_l = (padGetButtons(&pad_) & HidNpadButton_L) != 0;
    if (!close_requested_ && ((down & HidNpadButton_B) || ((down & HidNpadButton_Minus) && !with_l))) RequestClose();
#endif
    if (!close_requested_ && ImGui::IsKeyPressed(ImGuiKey_Escape)) RequestClose();
    view_.Draw(io, *g_hooks.achievements);
  }

 private:
  void RequestClose() {
    close_requested_ = true;
    // The SDK owns this dialog (bind_achievements): its toggle destroys it, after this draw.
    if (g_hooks.on_ui_thread && g_hooks.toggle_screen) g_hooks.on_ui_thread(g_hooks.toggle_screen);
  }

  ScreenView view_;
  bool close_requested_ = false;
#if REX_PLATFORM_SWITCH
  PadState pad_{};
#endif
};

// PC test (ds2_trophy_dump): the screen and the two notices drawn once in an ImGui context of our own at 1280x720
// (the PC's software driver shows no window, so the SDK never draws its dialogs there), their draw lists written for
// tools/features/raster_dump.py.
void DrawOffscreen() {
  if (REXCVAR_GET(ds2_trophy_dump).empty() || !g_hooks.achievements) return;
  ImGuiContext* previous = ImGui::GetCurrentContext();
  ImGuiContext* context = ImGui::CreateContext();
  ImGui::SetCurrentContext(context);
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(1280.0f, 720.0f);
  io.DeltaTime = 1.0f / 60.0f;
  io.IniFilename = nullptr;
  io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
  EnsurePlatinum(*g_hooks.achievements);
  {
    ScreenView view;
    view.preview_selected = 2;
    for (int frame = 0; frame < 3; ++frame) {
      ImGui::NewFrame();
      view.Draw(io, *g_hooks.achievements);
      ImGui::Render();
    }
    DumpDrawData("screen", ImGui::GetDrawData());
    rex::ui::AchievementIconCache icons(g_hooks.immediate, g_hooks.runtime);
    std::vector<AchievementInfo> list = g_hooks.achievements->ListAchievements();
    for (const AchievementInfo& a : list) {
      if (a.id == kPlatinumId) continue;
      ImGui::NewFrame();
      DrawNoticeCard(io, a, 1.0f, icons);
      ImGui::Render();
      DumpDrawData("notice", ImGui::GetDrawData());
      break;
    }
    ImGui::NewFrame();
    DrawNoticeCard(io, PlatinumInfo(), 1.0f, icons);
    ImGui::Render();
    DumpDrawData("notice_platinum", ImGui::GetDrawData());
  }
  ImGui::DestroyContext(context);
  ImGui::SetCurrentContext(previous);
}

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
  // The game's art for the screen and the notices (its files are mounted by now), then the platinum check: a
  // profile that already had everything before the platinum existed gets it at the first check.
  g_hooks.on_ui_thread([] {
    art::Load(g_hooks.runtime, g_hooks.immediate, REXCVAR_GET(user_language));
    CheckPlatinum();
    DrawOffscreen();
  });
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
  // The SDK's ImGui builds a static atlas (no dynamic glyphs): the ranges of the game's language. Latin with its
  // extended block (Polish), Cyrillic for Russian, ImGui's common Japanese set for Japanese.
  static const ImWchar kLatin[] = {0x0020, 0x00FF, 0x0100, 0x017F, 0x2000, 0x206F, 0x2212, 0x2212, 0};
  static const ImWchar kCyrillic[] = {0x0020, 0x00FF, 0x0100, 0x017F, 0x0400, 0x04FF, 0x2000, 0x206F, 0};
  const uint32_t language = REXCVAR_GET(user_language);
  config.GlyphRanges = language == 2 ? atlas->GetGlyphRangesJapanese() : language == 12 ? kCyrillic : kLatin;
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
