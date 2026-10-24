// Dark Souls II: the port's gameplay options (docs/features.md, "The options rows" and "Gameplay").
//
// Two rows at the end of the game's Screen Options page (gear tab, after Adjust Brightness), each On / Off and kept in
// ds2.toml, which the rows rewrite:
//   - Early Dull Ember (ds2_early_dull_ember): the Last Giant's reward lot (ItemLotParam2_Other row 309600) also gives
//     the Dull Ember, and the Dull Ember's own pickup in the Iron Keep (row 10196160) gives a Titanite Slab instead.
//     The two rows are changed in the game's memory while the option is on and put back when it is turned off; a lot
//     is read when it is awarded, so the change is live.
//   - Better fog walls (ds2_fog_wall_iframes): the option and its row; the invulnerability itself waits for the
//     crossing's function, which the diagnostics below are for (ds2_diag_features).
//
// How the page works (static reading of the menu code; addresses in docs/features.md): Screen Options is a page class
// whose build function (822A34A8) adds its four rows to the menu manager as items 75 to 78, each on one of the 15 row
// slots of the In-Game layout (the Game Options page fills all 15, this one uses the first four). Our rows take the
// next two slots (layout elements 2010246 and 2010245) as items 79 and 80, built the way the game builds its own:
// the slot's element, the item in the page's list and in the manager, the name, the value, the two arrows. Their
// value changes (822A2918), help line and key guide (822A2388) are answered here; the texts come from the game's
// message lookup (8264CF08), which gets our ids.

#include "ds2_features.h"

#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

REXCVAR_DEFINE_BOOL(ds2_early_dull_ember, false, "DS2",
                    "Early Dull Ember: the Last Giant also drops the Dull Ember, and its pickup in the Iron Keep gives a "
                    "Titanite Slab instead (live; also a row of Screen Options)");
REXCVAR_DEFINE_BOOL(ds2_fog_wall_iframes, false, "DS2",
                    "Better fog walls: no damage while crossing a fog wall (also a row of Screen Options)");
REXCVAR_DEFINE_BOOL(ds2_diag_features, false, "DS2",
                    "Diagnostics of the gameplay options: log the item lots the game reads and the fog wall's functions");

namespace ds2::features {
namespace {

std::filesystem::path g_config_path;

// --- guest memory (big-endian; virtual addresses, below the physical range) -------------------------------------------

uint32_t L32(const uint8_t* base, uint32_t a) {
  uint32_t v;
  std::memcpy(&v, base + a, 4);
  return __builtin_bswap32(v);
}
uint16_t L16(const uint8_t* base, uint32_t a) {
  uint16_t v;
  std::memcpy(&v, base + a, 2);
  return __builtin_bswap16(v);
}
void S32(uint8_t* base, uint32_t a, uint32_t v) {
  v = __builtin_bswap32(v);
  std::memcpy(base + a, &v, 4);
}
bool Plausible(uint32_t a) { return a >= 0x10000 && a < 0xA0000000u; }

// A frame of our own below the caller's stack pointer, for the guest functions we call (their stack arguments and
// the buffers we pass them); the stack pointer is put back when it goes.
class GuestFrame {
 public:
  GuestFrame(PPCContext& ctx, uint32_t size) : ctx_(ctx), saved_(ctx.r1.u32) {
    ctx_.r1.u64 = (saved_ - size) & ~0xFu;
  }
  ~GuestFrame() { ctx_.r1.u64 = saved_; }
  uint32_t at(uint32_t offset) const { return ctx_.r1.u32 + offset; }

 private:
  PPCContext& ctx_;
  uint32_t saved_;
};

}  // namespace
}  // namespace ds2::features

// The game's functions we call.
REX_EXTERN(sub_82ECCC08);  // layout lookup: (layout, id path of up to 7, 0-terminated) -> element
REX_EXTERN(sub_82272778);  // set a text element from a message: (element, category, id, 0)
REX_EXTERN(sub_82414020);  // add an item id to a page's item list: (list, &id)
REX_EXTERN(sub_82312AD8);  // give the menu manager an item's element: (manager, item, element)
REX_EXTERN(sub_82273258);  // a menu sound request on the stack: (request, kind)
REX_EXTERN(sub_82F9A4B0);  // play it: (request)

namespace ds2::features {
namespace {

// --- the rows ---------------------------------------------------------------------------------------------------------

constexpr uint32_t kCategoryMenu = 7;          // ingamemenu.fmg
constexpr uint32_t kFirstItem = 75;            // Screen Options' rows are the menu manager's items 75..
constexpr uint32_t kGameRows = 4;              // Blood, Subtitles, HUD, Adjust Brightness
constexpr uint32_t kItemStride = 44;           // the manager's items, 44 bytes each
constexpr uint32_t kItemValueText = 25880 - kFirstItem * kItemStride;  // + row * 44 from the manager: value text
constexpr uint32_t kItemArrowLeft = 25884 - kFirstItem * kItemStride;
constexpr uint32_t kItemArrowRight = 25888 - kFirstItem * kItemStride;
constexpr uint32_t kMsgOn = 2603020, kMsgOff = 2603030;  // the game's own "On" / "Off" (Subtitles)
constexpr uint32_t kKeyGuideTable = 0x834133C4;          // Screen Options' key guides per row (category 7)
constexpr uint32_t kKeyGuideTable22 = 0x834133D4;        // the same, category 22 (the page's other mode)
constexpr uint32_t kShowAlpha = 0x820019B0;              // the float the builder shows a row's slot with

struct OurRow {
  uint32_t slot;   // layout element of the row slot
  uint32_t name;   // message ids (answered by the lookup hook)
  uint32_t help;
  const char* cvar;
  const char16_t* name_text;
  const char16_t* help_text;
};
const std::array<OurRow, 2> kRows = {{
    {2010246, 2601900, 2609900, "ds2_early_dull_ember", u"Early Dull Ember",
     u"The Last Giant drops the Dull Ember. Iron Keep: a Titanite Slab."},
    {2010245, 2601910, 2609910, "ds2_fog_wall_iframes", u"Better fog walls",
     u"No damage while passing through a fog wall."},
}};

bool RowValue(size_t row) { return row == 0 ? REXCVAR_GET(ds2_early_dull_ember) : REXCVAR_GET(ds2_fog_wall_iframes); }

// Our texts in guest memory (UTF-16 big-endian, as the game's messages), made once.
std::unordered_map<uint32_t, uint32_t> g_texts;
std::mutex g_texts_mutex;

uint32_t GuestText(uint32_t id) {
  std::lock_guard lock(g_texts_mutex);
  if (g_texts.empty()) {
    auto* memory = rex::system::kernel_state() ? rex::system::kernel_state()->memory() : nullptr;
    if (!memory) return 0;
    for (const OurRow& row : kRows) {
      for (auto [msg, text] : {std::pair{row.name, row.name_text}, std::pair{row.help, row.help_text}}) {
        const std::u16string_view s(text);
        const uint32_t size = uint32_t((s.size() + 1) * 2);
        const uint32_t guest = memory->SystemHeapAlloc(size);
        if (!guest) continue;
        uint8_t* p = memory->TranslateVirtual<uint8_t*>(guest);
        for (size_t i = 0; i <= s.size(); ++i) {
          const char16_t c = i < s.size() ? s[i] : 0;
          p[2 * i] = uint8_t(c >> 8);
          p[2 * i + 1] = uint8_t(c);
        }
        g_texts[msg] = guest;
      }
    }
  }
  auto it = g_texts.find(id);
  return it == g_texts.end() ? 0 : it->second;
}

uint32_t Find(PPCContext& ctx, uint8_t* base, uint32_t layout, uint32_t parent, uint32_t slot, uint32_t child,
              uint32_t extra) {
  GuestFrame frame(ctx, 0x80);
  S32(base, frame.at(84), 0);
  S32(base, frame.at(92), 0);
  S32(base, frame.at(100), 0);
  ctx.r3.u64 = layout;
  ctx.r4.u64 = parent;
  ctx.r5.u64 = slot;
  ctx.r6.u64 = child;
  ctx.r7.u64 = extra;
  ctx.r8.u64 = 0;
  ctx.r9.u64 = 0;
  ctx.r10.u64 = 0;
  sub_82ECCC08(ctx, base);
  return ctx.r3.u32;
}

void SetText(PPCContext& ctx, uint8_t* base, uint32_t element, uint32_t category, uint32_t id) {
  if (!element) return;
  ctx.r3.u64 = element;
  ctx.r4.u64 = category;
  ctx.r5.u64 = id;
  ctx.r6.u64 = 0;
  sub_82272778(ctx, base);
}

// A virtual call on a layout element: r3 = the element, the other arguments already set.
void VirtualCall(PPCContext& ctx, uint8_t* base, uint32_t element, uint32_t offset) {
  const uint32_t function = L32(base, L32(base, element) + offset);
  ctx.r3.u64 = element;
  rex::runtime::ResolveIndirectFunction(function)(ctx, base);
}

void PlaySound(PPCContext& ctx, uint8_t* base) {
  GuestFrame frame(ctx, 0x100);
  const uint32_t request = frame.at(0x80);
  ctx.r3.u64 = request;
  ctx.r4.u64 = 2;
  sub_82273258(ctx, base);
  ctx.r3.u64 = request;
  sub_82F9A4B0(ctx, base);
}

uint32_t RowParent(const uint8_t* base, uint32_t page) { return base[page + 72] ? 2010240 : 2010241; }

void ShowValue(PPCContext& ctx, uint8_t* base, uint32_t page, size_t row) {
  const uint32_t manager = L32(base, page + 68);
  if (!Plausible(manager)) return;
  const uint32_t value = L32(base, manager + (kGameRows + uint32_t(row)) * kItemStride + kItemValueText);
  if (Plausible(value)) SetText(ctx, base, value, kCategoryMenu, RowValue(row) ? kMsgOn : kMsgOff);
}

void AddRows(PPCContext& ctx, uint8_t* base, uint32_t page) {
  const uint32_t layout = L32(base, page + 4);
  const uint32_t manager = L32(base, page + 68);
  if (!Plausible(layout) || !Plausible(manager)) return;
  const uint32_t parent = RowParent(base, page);
  const float show = std::bit_cast<float>(L32(base, kShowAlpha));
  for (size_t k = 0; k < kRows.size(); ++k) {
    const OurRow& row = kRows[k];
    const uint32_t item = kFirstItem + kGameRows + uint32_t(k);
    const uint32_t element = Find(ctx, base, layout, parent, row.slot, 2010173, 0);
    if (!element) {
      REXLOG_WARN("[features] Screen Options: no layout slot {} for the row \"{}\"", row.slot, row.cvar);
      continue;
    }
    ctx.r4.u64 = 101;
    ctx.r5.u64 = 0;
    ctx.f1.f64 = show;
    VirtualCall(ctx, base, element, 96);
    {
      GuestFrame frame(ctx, 0x40);
      S32(base, frame.at(0x20), item);
      ctx.r3.u64 = page + 80;
      ctx.r4.u64 = frame.at(0x20);
      sub_82414020(ctx, base);
    }
    ctx.r3.u64 = manager;
    ctx.r4.u64 = item;
    ctx.r5.u64 = element;
    sub_82312AD8(ctx, base);
    SetText(ctx, base, Find(ctx, base, layout, parent, row.slot, 2010181, 99990002), kCategoryMenu, row.name);
    const uint32_t value = Find(ctx, base, layout, parent, row.slot, 2010182, 99990002);
    SetText(ctx, base, value, kCategoryMenu, RowValue(k) ? kMsgOn : kMsgOff);
    const uint32_t item_base = manager + (kGameRows + uint32_t(k)) * kItemStride;
    S32(base, item_base + kItemValueText, value);
    for (auto [child, offset] : {std::pair{2010171u, kItemArrowLeft}, std::pair{2010172u, kItemArrowRight}}) {
      const uint32_t arrow = Find(ctx, base, layout, parent, row.slot, child, 0);
      if (arrow) {
        ctx.r4.u64 = 1;
        VirtualCall(ctx, base, arrow, 100);
      }
      S32(base, item_base + offset, arrow);
    }
  }
}

// --- ds2.toml -----------------------------------------------------------------------------------------------------------

// Sets `key = value` in the config file: the key's line is replaced, or the key is added at the end. The rest of the
// file is kept as it is.
void WriteKey(std::string_view key, bool value) {
  if (g_config_path.empty()) return;
  std::vector<std::string> lines;
  {
    std::ifstream in(g_config_path, std::ios::binary);
    for (std::string line; std::getline(in, line);) lines.push_back(line);
  }
  const std::string text = std::string(key) + " = " + (value ? "true" : "false");
  bool found = false;
  for (std::string& line : lines) {
    const size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos || line.compare(start, key.size(), key) != 0) continue;
    const size_t after = line.find_first_not_of(" \t", start + key.size());
    if (after == std::string::npos || line[after] != '=') continue;
    line = text + (line.size() && line.back() == '\r' ? "\r" : "");
    found = true;
  }
  if (!found) lines.push_back(text);
  std::filesystem::path temp = g_config_path;
  temp += ".new";
  {
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    for (const std::string& line : lines) out << line << '\n';
    if (!out) {
      REXLOG_WARN("[features] could not write {}", temp.string());
      return;
    }
  }
  std::error_code ec;
  std::filesystem::rename(temp, g_config_path, ec);
  if (ec) {
    std::filesystem::remove(g_config_path, ec);
    std::filesystem::rename(temp, g_config_path, ec);
  }
  if (ec) REXLOG_WARN("[features] could not replace {}: {}", g_config_path.string(), ec.message());
}

// --- Early Dull Ember: the two item lots ----------------------------------------------------------------------------

constexpr uint32_t kLotGiant = 309600;     // the Last Giant's reward: Soul of the Last Giant, Soldier Key
constexpr uint32_t kLotIronKeep = 10196160;  // the Dull Ember's pickup in the Iron Keep
constexpr uint32_t kDullEmber = 50990000, kTitaniteSlab = 60990000, kNoItem = 10;
constexpr uint32_t kLotRowSize = 124;      // amounts at 4, slot bytes at 34, items at 44, chances at 84 (ten each)

uint32_t g_item_params = 0;  // the item params' manager (823A4860): +36 ItemLotParam2_Other, +28 ..._Chr
struct SavedRow {
  uint32_t address = 0;
  std::array<uint8_t, kLotRowSize> original{};
};
SavedRow g_giant, g_iron_keep;
uint32_t g_lot_data = 0;
bool g_applied = false;
std::mutex g_lots_mutex;

uint32_t LotParam(const uint8_t* base, uint32_t offset) {
  if (!g_item_params) return 0;
  const uint32_t resource = L32(base, g_item_params + offset);
  if (!Plausible(resource)) return 0;
  const uint32_t data = L32(base, resource + 148);
  if (!Plausible(data) || std::memcmp(base + data + 0x0C, "ITEM_LOT_PARAM2", 15) != 0) return 0;
  return data;
}

uint32_t LotRow(const uint8_t* base, uint32_t data, uint32_t id) {
  const uint32_t rows = L16(base, data + 0x0A);
  for (uint32_t i = 0; i < rows; ++i) {
    if (L32(base, data + 0x40 + 12 * i) == id) return data + L32(base, data + 0x44 + 12 * i);
  }
  return 0;
}

void SyncLots(uint8_t* base) {
  std::lock_guard lock(g_lots_mutex);
  const uint32_t data = LotParam(base, 36);
  if (!data) return;
  if (data != g_lot_data) {  // a (re)loaded regulation: find the rows and keep them as they are
    g_lot_data = data;
    g_applied = false;
    for (auto [saved, id] : {std::pair{&g_giant, kLotGiant}, std::pair{&g_iron_keep, kLotIronKeep}}) {
      saved->address = LotRow(base, data, id);
      if (saved->address) std::memcpy(saved->original.data(), base + saved->address, kLotRowSize);
    }
    if (!g_giant.address || !g_iron_keep.address ||
        L32(base, g_iron_keep.address + 44) != kDullEmber) {
      REXLOG_WARN("[features] Early Dull Ember: the item lots are not the expected ones (rows {} and {}): off",
                  kLotGiant, kLotIronKeep);
      g_giant.address = g_iron_keep.address = 0;
      return;
    }
    REXLOG_INFO("[features] item lots found: ItemLotParam2_Other at {:#x}, rows {} at {:#x} and {} at {:#x}", data,
                kLotGiant, g_giant.address, kLotIronKeep, g_iron_keep.address);
  }
  if (!g_giant.address) return;
  const bool on = REXCVAR_GET(ds2_early_dull_ember);
  if (on == g_applied) return;
  std::memcpy(base + g_giant.address, g_giant.original.data(), kLotRowSize);
  std::memcpy(base + g_iron_keep.address, g_iron_keep.original.data(), kLotRowSize);
  if (on) {
    uint32_t slot = 0;
    while (slot < 10 && L32(base, g_giant.address + 44 + 4 * slot) != kNoItem) ++slot;
    if (slot == 10) {
      REXLOG_WARN("[features] Early Dull Ember: the Last Giant's lot has no free slot");
      return;
    }
    base[g_giant.address + 4 + slot] = 1;   // amount
    base[g_giant.address + 34 + slot] = 1;  // the slot is in use
    S32(base, g_giant.address + 44 + 4 * slot, kDullEmber);
    S32(base, g_giant.address + 84 + 4 * slot, std::bit_cast<uint32_t>(1.0f));
    S32(base, g_iron_keep.address + 44, kTitaniteSlab);
  }
  g_applied = on;
  REXLOG_INFO("[features] Early Dull Ember {}: the Last Giant's lot {} the Dull Ember, the Iron Keep's gives {}",
              on ? "on" : "off", on ? "adds" : "without", on ? "a Titanite Slab" : "the Dull Ember");
}

// --- diagnostics ------------------------------------------------------------------------------------------------------

std::mutex g_diag_mutex;
std::unordered_map<uint32_t, uint64_t> g_calls;
std::unordered_map<uint64_t, std::chrono::steady_clock::time_point> g_lot_logged;

void TraceCall(uint32_t function, const PPCContext& ctx) {
  if (!REXCVAR_GET(ds2_diag_features)) return;
  std::lock_guard lock(g_diag_mutex);
  const uint64_t n = ++g_calls[function];
  if (n <= 4 || std::has_single_bit(n)) {
    REXLOG_INFO("[features] fog wall: sub_{:08X} call {} (r3 {:#x} r4 {:#x} r5 {:#x})", function, n, ctx.r3.u32,
                ctx.r4.u32, ctx.r5.u32);
  }
}

void TraceLot(const uint8_t* base, uint32_t handle, uint32_t id) {
  if (!REXCVAR_GET(ds2_diag_features) || !g_item_params) return;
  const char* which = nullptr;
  for (auto [offset, name] : {std::pair{36u, "Other"}, std::pair{28u, "Chr"}}) {
    const uint32_t resource = L32(base, g_item_params + offset);
    if (Plausible(resource) && handle == resource + 140) which = name;
  }
  if (!which) return;
  std::lock_guard lock(g_diag_mutex);
  const auto now = std::chrono::steady_clock::now();
  auto& last = g_lot_logged[(uint64_t(handle) << 32) | id];
  if (now - last < std::chrono::seconds(2)) return;
  last = now;
  REXLOG_INFO("[features] item lot read: ItemLotParam2_{} row {}", which, id);
}

}  // namespace

void SetConfigPath(const std::filesystem::path& path) { g_config_path = path; }

}  // namespace ds2::features

using namespace ds2::features;

// The item params' manager is filled (the regulation loaded): find the two lots and apply the option.
REX_EXTERN(__imp__sub_823A4860);
REX_HOOK_RAW(sub_823A4860) {
  const uint32_t manager = ctx.r3.u32;
  __imp__sub_823A4860(ctx, base);
  g_item_params = manager;
  SyncLots(base);
}

// Screen Options' build: the game's four rows, then ours.
REX_EXTERN(__imp__sub_822A34A8);
REX_HOOK_RAW(sub_822A34A8) {
  const uint32_t page = ctx.r3.u32;
  __imp__sub_822A34A8(ctx, base);
  AddRows(ctx, base, page);
}

// Screen Options' value change (page, row, step): ours flip on any step and write ds2.toml.
REX_EXTERN(__imp__sub_822A2918);
REX_HOOK_RAW(sub_822A2918) {
  const uint32_t page = ctx.r3.u32, row = ctx.r4.u32;
  const int32_t step = ctx.r5.s32;
  if (row < kGameRows || row >= kGameRows + kRows.size()) {
    __imp__sub_822A2918(ctx, base);
    return;
  }
  const size_t k = row - kGameRows;
  if (step != 0) {
    const bool value = !RowValue(k);
    rex::cvar::SetFlagByName(kRows[k].cvar, value ? "true" : "false");
    WriteKey(kRows[k].cvar, value);
    REXLOG_INFO("[features] {} = {} (Screen Options)", kRows[k].cvar, value);
    if (k == 0) SyncLots(base);
    PlaySound(ctx, base);
  }
  ShowValue(ctx, base, page, k);
}

// Screen Options' help line and key guide for the selected row.
REX_EXTERN(__imp__sub_822A2388);
REX_HOOK_RAW(sub_822A2388) {
  const uint32_t page = ctx.r3.u32;
  const uint32_t row = L32(base, page + 24) - kFirstItem;
  const uint32_t manager = L32(base, page + 68);
  if (row < kGameRows || row >= kGameRows + kRows.size() || !Plausible(manager)) {
    __imp__sub_822A2388(ctx, base);
    return;
  }
  const uint32_t widgets = L32(base, manager + 4);
  if (!Plausible(widgets)) return;
  SetText(ctx, base, L32(base, widgets + 836), kCategoryMenu, kRows[row - kGameRows].help);
  const bool other_mode = base[page + 72] != 0;
  const uint32_t guide = L32(base, (other_mode ? kKeyGuideTable22 : kKeyGuideTable) + 4);  // Subtitles' guide
  SetText(ctx, base, L32(base, widgets + 880), other_mode ? 22 : kCategoryMenu, guide);
}

// The message lookup (messages, category, id) -> UTF-16 text: our ids get our texts.
REX_EXTERN(__imp__sub_8264CF08);
REX_HOOK_RAW(sub_8264CF08) {
  if (ctx.r4.u32 == kCategoryMenu) {
    const uint32_t id = ctx.r5.u32;
    for (const OurRow& row : kRows) {
      if (id == row.name || id == row.help) {
        if (const uint32_t text = GuestText(id)) {
          ctx.r3.u64 = text;
          return;
        }
      }
    }
  }
  __imp__sub_8264CF08(ctx, base);
}

// Diagnostics: a param row looked up by id (handle, id) -> row; the item lots are logged.
REX_EXTERN(__imp__sub_82271CC8);
REX_HOOK_RAW(sub_82271CC8) {
  TraceLot(base, ctx.r3.u32, ctx.r4.u32);
  __imp__sub_82271CC8(ctx, base);
}

// Diagnostics: the fog wall's component (MapObjWhiteDoorComponent, its vtable at 8200BF4C): which of its functions run
// when the player crosses.
#define DS2_TRACE_FOG(address)                \
  REX_EXTERN(__imp__sub_##address);           \
  REX_HOOK_RAW(sub_##address) {               \
    TraceCall(0x##address##u, ctx);           \
    __imp__sub_##address(ctx, base);          \
  }
DS2_TRACE_FOG(823BA908)
DS2_TRACE_FOG(823BAB68)
DS2_TRACE_FOG(823BB620)
DS2_TRACE_FOG(823BB638)
DS2_TRACE_FOG(823BB780)
DS2_TRACE_FOG(823BBC70)
DS2_TRACE_FOG(823BBCE8)
DS2_TRACE_FOG(823BC130)
DS2_TRACE_FOG(823BA1C0)
DS2_TRACE_FOG(823BA120)
