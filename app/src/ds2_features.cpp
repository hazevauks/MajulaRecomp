// Dark Souls II: the port's gameplay options (docs/features.md, "The options rows" and "Gameplay").
//
// Two rows of the game's Game Options page (gear tab), each On / Off and kept in ds2.toml, which the rows rewrite. They
// take the places of "Voice chat" and "Cross-region play" (rows 11 and 12 of 15), which only served Xbox Live: the
// page has no room for more rows (its 15 fill the panel; Screen Options has 4 and the brightness picture), so the
// two online rows show our names, help lines and values instead, and the game's own voice and region settings are
// left as they are.
//   - Early Dull Ember (ds2_early_dull_ember): the Last Giant's reward lot (ItemLotParam2_Other row 309600) also gives
//     the Dull Ember, and the Dull Ember's own pickup in the Iron Keep (row 10196160) gives a Titanite Slab instead.
//     The two rows are changed in the game's memory while the option is on and put back when it is turned off; a lot
//     is read when it is awarded, so the change is live.
//   - Better fog walls (ds2_fog_wall_iframes): the option and its row; the invulnerability itself waits for the
//     crossing's function, which the diagnostics below are for (ds2_diag_features).
//
// How the page works (static reading of the menu code, checked on the PC; docs/features.md): Game Options' rows are
// the menu manager's items 59 to 73; the page's value function (822A25F8: page, row, step) changes a row and writes
// its value text, the row names and help lines are messages (category 7) that the game's lookup (8264CF08) resolves,
// and "Voice chat" is drawn as unavailable when the voice check (8265DB68) says so.

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

// The game's functions we call (their __imp__ bodies: the direct-calls step leaves those addresses unhooked).
REX_EXTERN(__imp__sub_82272778);  // set a text element from a message: (element, category, id, 0)
REX_EXTERN(__imp__sub_82273258);  // a menu sound request on the stack: (request, kind)
REX_EXTERN(__imp__sub_82F9A4B0);  // play it: (request)

namespace ds2::features {
namespace {

// --- the rows ---------------------------------------------------------------------------------------------------------

constexpr uint32_t kCategoryMenu = 7;                    // ingamemenu.fmg
constexpr uint32_t kGameOptionsValueText = 25176;        // + row * 44 from the menu manager: a row's value text
constexpr uint32_t kMsgOn = 2603020, kMsgOff = 2603030;  // the game's own "On" / "Off"

struct OurRow {
  uint32_t row;    // Game Options' row it takes
  uint32_t name;   // that row's message ids, answered with ours
  uint32_t help;
  const char* cvar;
  const char16_t* name_text;
  const char16_t* help_text;
};
const std::array<OurRow, 2> kRows = {{
    {10, 2601202, 2609102, "ds2_early_dull_ember", u"Early Dull Ember",
     u"The Last Giant drops the Dull Ember. Iron Keep: a Titanite Slab."},
    {11, 2601210, 2609110, "ds2_fog_wall_iframes", u"Better fog walls",
     u"No damage while passing through a fog wall."},
}};

bool RowValue(size_t k) { return k == 0 ? REXCVAR_GET(ds2_early_dull_ember) : REXCVAR_GET(ds2_fog_wall_iframes); }

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
        const uint32_t guest = memory->SystemHeapAlloc(uint32_t((s.size() + 1) * 2));
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

void SetText(PPCContext& ctx, uint8_t* base, uint32_t element, uint32_t category, uint32_t id) {
  if (!element) return;
  ctx.r3.u64 = element;
  ctx.r4.u64 = category;
  ctx.r5.u64 = id;
  ctx.r6.u64 = 0;
  __imp__sub_82272778(ctx, base);
}

void PlaySound(PPCContext& ctx, uint8_t* base) {
  GuestFrame frame(ctx, 0x100);
  const uint32_t request = frame.at(0x80);
  ctx.r3.u64 = request;
  ctx.r4.u64 = 2;
  __imp__sub_82273258(ctx, base);
  ctx.r3.u64 = request;
  __imp__sub_82F9A4B0(ctx, base);
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

// Game Options' value function (page, row, step): our rows flip on any step, write ds2.toml and show On / Off; the
// game's voice and region settings are not touched.
REX_EXTERN(__imp__sub_822A25F8);
REX_HOOK_RAW(sub_822A25F8) {
  const uint32_t page = ctx.r3.u32, row = ctx.r4.u32;
  const int32_t step = ctx.r5.s32;
  size_t k = 0;
  while (k < kRows.size() && kRows[k].row != row) ++k;
  if (k == kRows.size()) {
    __imp__sub_822A25F8(ctx, base);
    return;
  }
  if (step != 0) {
    const bool value = !RowValue(k);
    rex::cvar::SetFlagByName(kRows[k].cvar, value ? "true" : "false");
    WriteKey(kRows[k].cvar, value);
    REXLOG_INFO("[features] {} = {} (Game Options)", kRows[k].cvar, value);
    if (k == 0) SyncLots(base);
    if (base[page + 340] == 0) PlaySound(ctx, base);  // the page is not being built
  }
  const uint32_t manager = L32(base, page + 68);
  if (Plausible(manager)) {
    const uint32_t value = L32(base, manager + row * 44 + kGameOptionsValueText);
    if (Plausible(value)) SetText(ctx, base, value, kCategoryMenu, RowValue(k) ? kMsgOn : kMsgOff);
  }
}

// Game Options' value text of a row (page, row) -> message id, used by the value function and by the page's refresh of
// all fifteen values: ours are On / Off.
REX_EXTERN(__imp__sub_822A2460);
REX_HOOK_RAW(sub_822A2460) {
  for (size_t k = 0; k < kRows.size(); ++k) {
    if (kRows[k].row == ctx.r4.u32) {
      ctx.r3.u64 = RowValue(k) ? kMsgOn : kMsgOff;
      return;
    }
  }
  __imp__sub_822A2460(ctx, base);
}

// The voice chat check (Game Options draws "Voice chat" as unavailable without it): its row is ours, always available.
REX_HOOK_RAW(sub_8265DB68) { ctx.r3.u64 = 1; }

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
