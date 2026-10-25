// Dark Souls II: the menu bar's two new tabs, Trophies and Port, left of the gear (docs/features.md, section 3).
//
// The bar (the root page of the in-game menu, its vtable at 82003D50) is drawn by the game from the In-Game layout
// (menu\02.febnd.dcx, l02_01_In-Game.flo), which it reads again at every opening of the menu and keeps in memory with
// its offsets made pointers. Before the bar is built (822B0B98, the page's slot 8) the layout is changed there:
//   - the bar's sprite (the chain and its stone medallions, one sprite of In-game_01) gets a texture rectangle 108
//     units wider, and its right arrow moves 108 units;
//   - the gear's tab node (its highlight and chevron), its dropdown and the RB label move 108 units right;
//   - the two icons that sat right of the bar in the texture (an axe and a shield) get the rectangle they are moved
//     to in the page.
// After the build, In-game_01's page in physical memory gets the strip of seven medallions (the chalice and the
// bonfire's coiled sword before the gear) and the icons in their new place, before the renderer first takes it.
//
// Stage one, the prototype: the page comes from files made by an investigation script (ds2_tabs_page_dir, PC only).
// The tabs' input and pages come next.

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>

#include <fmt/format.h>
#include <vector>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

REXCVAR_DEFINE_STRING(ds2_tabs_page_dir, "", "DS2",
                      "Prototype (PC): a folder with ingame01_orig.bin and ingame01_patched.bin, In-game_01's page "
                      "without and with the bar of seven medallions; empty = the bar is left as it is");
REXCVAR_DEFINE_BOOL(ds2_diag_tabs, false, "DS2",
                    "Diagnostics of the menu bar's new tabs: log the bar's cursor and the menu input it reads");

namespace {

uint32_t L32(const uint8_t* base, uint32_t a) {
  uint32_t v;
  std::memcpy(&v, base + a, 4);
  return __builtin_bswap32(v);
}
void S32(uint8_t* base, uint32_t a, uint32_t v) {
  v = __builtin_bswap32(v);
  std::memcpy(base + a, &v, 4);
}
float GetF(const uint8_t* base, uint32_t a) {
  const uint32_t w = L32(base, a);
  float v;
  std::memcpy(&v, &w, 4);
  return v;
}
void SetF(uint8_t* base, uint32_t a, float v) {
  uint32_t w;
  std::memcpy(&w, &v, 4);
  S32(base, a, w);
}
bool Plausible(uint32_t a) { return a >= 0x10000 && a < 0xA0000000u; }
bool Near(float a, float b) { return a > b - 0.05f && a < b + 0.05f; }

constexpr float kStep = 108.0f;  // two medallions of the bar (54 units each)

// l02_01_In-Game.flo, as offsets in the file.
constexpr uint32_t kBarRect = 0x8c50;     // the bar's texture rectangle in In-game_01: (1.1, 779.2) to (337.6, 850.9)
constexpr uint32_t kRightArrow = 0x6c70;  // the right arrow's sprite (mirrored): its x, 1305.45
constexpr uint32_t kIconRects = 0x7cf0;   // the axe's and the shield's texture rectangles, 16 bytes each
constexpr uint32_t kGearNode = 0x17a40;   // node records (0x20 bytes; +4: the property record, its x first)
constexpr uint32_t kDropdownNode = 0x17920;
constexpr uint32_t kRbLabelNode = 0x179a0;

// A node's property record: the node record's +4 is a pointer once the game has loaded the file, an offset before.
uint32_t Props(const uint8_t* base, uint32_t flo, uint32_t node) {
  const uint32_t v = L32(base, flo + node + 4);
  return v < 0x100000 ? flo + v : v;
}

// Moves the value at 'a' from 'was' to 'now', once (a value already moved is left as it is).
bool Move(uint8_t* base, uint32_t a, float was, float now) {
  const float v = GetF(base, a);
  if (Near(v, now)) return false;
  if (!Near(v, was)) {
    REXLOG_WARN("[tabs] the layout at {:#x}: {:.2f}, expected {:.2f}: left alone", a, v, was);
    return false;
  }
  SetF(base, a, now);
  return true;
}

// The In-Game layout of the bar's page: [layout+0x14]+0, the file as it was read (0 if not the expected one).
uint32_t Layout(const uint8_t* base, uint32_t page) {
  const uint32_t layout = L32(base, page + 4);
  if (!Plausible(layout) || !Plausible(L32(base, layout + 0x14))) return 0;
  const uint32_t flo = L32(base, L32(base, layout + 0x14));
  return Plausible(flo) && L32(base, flo) == 0x00020000u ? flo : 0;
}

// Before the build: the bar's sprite, its right arrow, the icons' rectangles, the gear's dropdown and the RB label
// (the dropdown's text is clipped to a box placed at the build). The gear's tab node moves after the build: LB / RB
// find the tabs by the positions the build reads.
void PatchLayout(uint8_t* base, uint32_t page) {
  const uint32_t flo = Layout(base, page);
  if (!flo) return;
  if (!Near(GetF(base, flo + kBarRect), 1.1f) || !Near(GetF(base, flo + kBarRect + 4), 779.2f)) {
    REXLOG_WARN("[tabs] the In-Game layout at {:#x} is not the expected one: the bar is left as it is", flo);
    return;
  }
  int moved = 0;
  moved += Move(base, flo + kBarRect + 8, 337.6f, 337.6f + kStep);
  moved += Move(base, flo + kRightArrow, 1305.45f, 1305.45f + kStep);
  // The icons' pixels move by (336, 167) in the page.
  const float icons[2][2][4] = {{{335.95f, 761.4f, 405.4f, 852.75f}, {671.95f, 928.4f, 741.4f, 1019.75f}},
                                {{405.4f, 761.4f, 473.9f, 852.75f}, {741.4f, 928.4f, 809.9f, 1019.75f}}};
  for (int k = 0; k < 2; ++k)
    for (int c = 0; c < 4; ++c)
      moved += Move(base, flo + kIconRects + 0x10 * k + 4 * c, icons[k][0][c], icons[k][1][c]);
  moved += Move(base, Props(base, flo, kDropdownNode), -60.15f, -60.15f + kStep);
  moved += Move(base, Props(base, flo, kRbLabelNode), 295.05f, 295.05f + kStep);
  if (moved) REXLOG_INFO("[tabs] the In-Game layout at {:#x}: the bar widened for two tabs ({} values)", flo, moved);
}

std::vector<uint8_t> ReadFile(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), {});
}

// In-game_01's page in physical memory: found by its first bytes (page-aligned), replaced, the store cards marked.
void PatchTexture() {
  const std::string dir = REXCVAR_GET(ds2_tabs_page_dir);
  static const std::vector<uint8_t> orig = ReadFile(dir + "/ingame01_orig.bin");
  static const std::vector<uint8_t> patched = ReadFile(dir + "/ingame01_patched.bin");
  if (orig.size() != 0x100000 || patched.size() != orig.size()) {
    static bool warned = false;
    if (!warned) REXLOG_WARN("[tabs] the page files are missing in {}", dir);
    warned = true;
    return;
  }
  auto* memory = rex::system::kernel_state()->memory();
  uint32_t found = UINT32_MAX;
  for (uint32_t view : {0xA0000000u, 0xC0000000u}) {
    uint32_t a = view;
    while (found == UINT32_MAX && a - view < 0x20000000u) {
      auto* heap = memory->LookupHeap(a);
      rex::memory::HeapAllocationInfo info{};
      if (!heap || !heap->QueryRegionInfo(a, &info) || !info.region_size) break;
      if (info.state & rex::memory::kMemoryAllocationCommit) {
        for (uint32_t p = info.base_address; p - info.base_address + 0x100000 <= info.region_size; p += 0x1000) {
          const uint8_t* host = memory->TranslateVirtual<const uint8_t*>(p);
          if (std::memcmp(host, orig.data(), 512) == 0) {
            found = p & 0x1FFFFFFF;
            break;
          }
        }
      }
      a = info.base_address + info.region_size;
    }
    if (found != UINT32_MAX) break;
  }
  if (found == UINT32_MAX) return;  // not loaded, or replaced already (the page's first bytes are the same in both)
  uint8_t* page = memory->TranslatePhysical<uint8_t*>(found);
  if (std::memcmp(page, patched.data(), patched.size()) == 0) return;
  std::memcpy(page, patched.data(), patched.size());
  uint8_t* cards = memory->virtual_membase();
  rex::memory::MarkStoreCardsAt(cards, 0xA0000000u + found, uint32_t(patched.size()));
  rex::memory::MarkStoreCardsAt(cards, 0xC0000000u + found, uint32_t(patched.size()));
  if (found >= 0x1000) rex::memory::MarkStoreCardsAt(cards, 0xE0000000u + found - 0x1000, uint32_t(patched.size()));
  REXLOG_INFO("[tabs] In-game_01's page at physical {:#x}: the bar of seven medallions", found);
}

}  // namespace

// The bar's build (page): the layout is widened before, the texture replaced after.
REX_EXTERN(__imp__sub_822B0B98);
REX_HOOK_RAW(sub_822B0B98) {
  const uint32_t page = ctx.r3.u32;
  const bool on = !REXCVAR_GET(ds2_tabs_page_dir).empty();
  if (on) PatchLayout(base, page);
  ctx.r3.u64 = page;
  __imp__sub_822B0B98(ctx, base);
  if (!on) return;
  if (const uint32_t flo = Layout(base, page)) Move(base, Props(base, flo, kGearNode), 211.55f, 211.55f + kStep);
  PatchTexture();
}

// The bar's update (page, f1): diagnostics of the menu input it reads, and of the page's words that change with it
// (to find its cursor).
REX_EXTERN(__imp__sub_822B36A8);
REX_HOOK_RAW(sub_822B36A8) {
  const uint32_t page = ctx.r3.u32;
  __imp__sub_822B36A8(ctx, base);
  if (!REXCVAR_GET(ds2_diag_tabs)) return;
  const uint32_t manager = L32(base, page + 12);
  if (!Plausible(manager)) return;
  static uint32_t last_page = 0, snapshot[0x100];
  const uint32_t pressed = L32(base, manager + 12);
  std::string diff;
  for (uint32_t i = 0; i < 0x100; ++i) {
    const uint32_t v = L32(base, page + 4 * i);
    if (page == last_page && v != snapshot[i]) diff += fmt::format(" +{:#x}:{:#x}->{:#x}", 4 * i, snapshot[i], v);
    snapshot[i] = v;
  }
  last_page = page;
  if (pressed || !diff.empty())
    REXLOG_INFO("[tabs] bar {:#x}: pressed {:#x};{}", page, pressed, diff.empty() ? " no word changed" : diff);
}
