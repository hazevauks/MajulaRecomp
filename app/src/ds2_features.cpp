// Dark Souls II: the port's gameplay changes (docs/features.md, "Gameplay"). They are on by default and kept only in
// ds2.toml (no menu row): a player who wants the original game turns them off there.
//   - Early Dull Ember (ds2_early_dull_ember): the Dull Ember is in the iron chest by The Tower Apart bonfire in the
//     Lost Bastille (where the Pursuer's eagle leaves the player; Scholar of the First Sin keeps it near there), with
//     the chest's Antiquated Key and Covetous Silver Serpent Ring (ItemLotParam2_Other row 10165140), and its old
//     place in the Iron Keep (row 10196160) gives a Titanite Slab instead. The two rows are changed in the game's memory
//     when the regulation loads; a lot is read when it is awarded.
//   - Better fog walls (ds2_fog_wall_iframes): no damage while crossing a fog wall. The invulnerability waits for the
//     crossing's function, which the diagnostics below are for (ds2_diag_features).

#include <bit>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <unordered_map>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc.h>

REXCVAR_DEFINE_BOOL(ds2_early_dull_ember, true, "DS2",
                    "Early Dull Ember: the Dull Ember is in the iron chest by The Tower Apart bonfire (Lost Bastille), "
                    "and its old place in the Iron Keep gives a Titanite Slab. false = the original game")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);
REXCVAR_DEFINE_BOOL(ds2_fog_wall_iframes, true, "DS2",
                    "Better fog walls: no damage while crossing a fog wall. false = the original game")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);
REXCVAR_DEFINE_BOOL(ds2_diag_features, false, "DS2",
                    "Diagnostics of the gameplay options: log the item lots the game reads and the fog wall's functions");

namespace ds2::features {
namespace {

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

// --- Early Dull Ember: the two item lots ----------------------------------------------------------------------------

constexpr uint32_t kLotBastille = 10165140;  // the iron chest by The Tower Apart: Antiquated Key, Covetous ring
constexpr uint32_t kLotIronKeep = 10196160;  // the Dull Ember's pickup in the Iron Keep
constexpr uint32_t kDullEmber = 50990000, kTitaniteSlab = 60990000, kNoItem = 10;
// A row of ItemLotParam2 (124 bytes): amounts at 4, slot bytes at 34, items at 44, chances at 84 (ten each).

uint32_t g_item_params = 0;  // the item params' manager (823B8068): +36 ItemLotParam2_Other, +28 ..._Chr
uint32_t g_lot_data = 0;
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
  if (!data || data == g_lot_data) return;
  g_lot_data = data;  // a (re)loaded regulation
  const uint32_t bastille = LotRow(base, data, kLotBastille), iron_keep = LotRow(base, data, kLotIronKeep);
  uint32_t slot = 0;
  while (bastille && slot < 10 && L32(base, bastille + 44 + 4 * slot) != kNoItem) ++slot;
  if (!bastille || !iron_keep || slot == 10 || L32(base, iron_keep + 44) != kDullEmber) {
    REXLOG_WARN("[features] Early Dull Ember: the item lots are not the expected ones (rows {} and {}): left alone",
                kLotBastille, kLotIronKeep);
    return;
  }
  if (!REXCVAR_GET(ds2_early_dull_ember)) {
    REXLOG_INFO("[features] Early Dull Ember off (ds2_early_dull_ember): the Dull Ember stays in the Iron Keep");
    return;
  }
  base[bastille + 4 + slot] = 1;   // amount
  base[bastille + 34 + slot] = 1;  // the slot is in use
  S32(base, bastille + 44 + 4 * slot, kDullEmber);
  S32(base, bastille + 84 + 4 * slot, std::bit_cast<uint32_t>(1.0f));
  S32(base, iron_keep + 44, kTitaniteSlab);
  REXLOG_INFO("[features] Early Dull Ember: the Dull Ember is in the iron chest by The Tower Apart (lot {}, slot {}), "
              "the Iron Keep's pickup (lot {}) gives a Titanite Slab", kLotBastille, slot, kLotIronKeep);
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
}  // namespace ds2::features

using namespace ds2::features;

// The item params' manager is filled (the regulation loaded): find the two lots and apply the change.
REX_EXTERN(__imp__sub_823B8068);
REX_HOOK_RAW(sub_823B8068) {
  const uint32_t manager = ctx.r3.u32;
  __imp__sub_823B8068(ctx, base);
  g_item_params = manager;
  SyncLots(base);
}

// Diagnostics: a param row looked up by id (handle, id) -> row; the item lots are logged.
REX_EXTERN(__imp__sub_82274C08);
REX_HOOK_RAW(sub_82274C08) {
  TraceLot(base, ctx.r3.u32, ctx.r4.u32);
  __imp__sub_82274C08(ctx, base);
}

// Diagnostics: the fog wall's component (MapObjWhiteDoorComponent, its vtable at 8200CC24): which of its functions run
// when the player crosses.
#define DS2_TRACE_FOG(address)                \
  REX_EXTERN(__imp__sub_##address);           \
  REX_HOOK_RAW(sub_##address) {               \
    TraceCall(0x##address##u, ctx);           \
    __imp__sub_##address(ctx, base);          \
  }
DS2_TRACE_FOG(823CEAB8)
DS2_TRACE_FOG(823CED18)
DS2_TRACE_FOG(823CF810)
DS2_TRACE_FOG(823CF828)
DS2_TRACE_FOG(823CF970)
DS2_TRACE_FOG(823CFE60)
DS2_TRACE_FOG(823CFED8)
DS2_TRACE_FOG(823D0320)
DS2_TRACE_FOG(823CE3D0)
DS2_TRACE_FOG(823CE330)
