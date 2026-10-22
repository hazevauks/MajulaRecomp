// Dark Souls II (from sa2-nx 1.0.2, app/src/cpu/sa2_codegen_checks.cpp): the PC side of the post-codegen steps of the second CPU round (docs/enhancements.md, "The
// second CPU round of 2026-10-12"). The Switch build has no code at any of these places: the macros of the generated
// pch are the bare operation there. Off the Switch each of them keeps the old operation next to the new one:
//   - tools/ro_constants.py: a load from read-only data folded to its constant still loads, counts itself
//     (g_rex_ro_checked) and calls RexRoMismatch when the memory does not hold the constant;
//   - tools/indirect_calls.py: a promoted indirect call counts itself (g_rex_promoted_calls) and calls
//     RexPromotedMismatch when the dispatch table's entry for the guarded target is not the function called;
//   - the census of indirect calls (ds2_diag_indirect_census = a file): per call site how often it ran and where to,
//     the input of tools/indirect_promotions.py;
//   - the SDK's kernel_critsec_pcr_owner: every critical section call compares the owner id it took from the PCR
//     with the looked-up thread's (rex/sys_counters.h, kCensusCritSecOwnerMismatch).
// So every PC run tests what it executes; the '[verify]' lines of the 10 s report carry the counts.
#include "ds2_enhancements.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <fmt/format.h>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/platform.h>
#include <rex/sys_counters.h>

// sa2-nx 1.0.3 (as banjo-nx): whether the generated code marks the store cards (tools/store_cards.py ran and
// REX_STORE_CARDS_OFF is not set). Renut proves its cached vertex, index and texture copies by them only then
// (banjo_renut_store_cards); this port does not run that step yet, so Renut hashes them.
#if defined(REX_CARD) && !defined(REX_STORE_CARDS_OFF)
extern "C" const bool g_banjo_store_cards = true;
#else
extern "C" const bool g_banjo_store_cards = false;
#endif

REXCVAR_DEFINE_STRING(ds2_diag_indirect_census, "", "DS2 enhancements",
                      "Diagnostic (PC): a file that gets, with every '[enh] uses' report, the census of the game's "
                      "indirect calls (per call site: calls and targets). tools/indirect_promotions.py makes "
                      "app/indirect_promotions.txt from it. Empty = no census")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

#if !REX_PLATFORM_SWITCH

extern "C" {
volatile unsigned long long g_rex_ro_checked = 0;      // plain lossy counts: "at least this many"
volatile unsigned long long g_rex_promoted_calls = 0;
unsigned char g_rex_indirect_census = 0;
}

namespace {

std::atomic<uint64_t> g_ro_mismatches{0};
std::atomic<uint64_t> g_promoted_mismatches{0};

// One call site: (function address << 32) | ordinal. Up to four targets are told apart; the rest is "other".
struct Site {
  uint64_t total = 0;
  uint32_t target[4] = {};
  uint64_t count[4] = {};
  uint64_t other = 0;
};
constexpr size_t kStripes = 64;
struct Stripe {
  std::mutex mutex;
  std::unordered_map<uint64_t, Site> sites;
};
Stripe g_stripes[kStripes];

}  // namespace

extern "C" void RexRoMismatch(unsigned address) noexcept {
  ds2::enh::VerifyDifference();
  if (g_ro_mismatches.fetch_add(1, std::memory_order_relaxed) < 16) {
    REXLOG_ERROR("[enh] DIFFERENCE: read-only constants: guest memory at {:08X} does not hold the constant the "
                 "generated code uses",
                 address);
  }
}

extern "C" void RexPromotedMismatch(unsigned target) noexcept {
  ds2::enh::VerifyDifference();
  if (g_promoted_mismatches.fetch_add(1, std::memory_order_relaxed) < 16) {
    REXLOG_ERROR("[enh] DIFFERENCE: promoted indirect call: the dispatch table's entry for {:08X} is not the "
                 "function the generated code calls",
                 target);
  }
}

extern "C" void RexIndirectCensus(unsigned function, unsigned site, unsigned target) noexcept {
  const uint64_t key = (uint64_t(function) << 32) | site;
  Stripe& stripe = g_stripes[(key * 0x9E3779B97F4A7C15ull) >> 58];
  std::lock_guard<std::mutex> lock(stripe.mutex);
  Site& s = stripe.sites[key];
  ++s.total;
  for (int i = 0; i < 4; ++i) {
    if (s.target[i] == target || s.count[i] == 0) {
      s.target[i] = target;
      ++s.count[i];
      return;
    }
  }
  ++s.other;
}

namespace ds2::enh {

void CodegenChecksInit() {
  g_rex_indirect_census = REXCVAR_GET(ds2_diag_indirect_census).empty() ? 0 : 1;
  if (g_rex_indirect_census) {
    REXLOG_INFO("[census] indirect calls: every tagged call site reports its target; the table goes to {} with each "
                "report",
                REXCVAR_GET(ds2_diag_indirect_census));
  }
}

void CodegenChecksReport(int seconds) {
  // The self-checks: what this run executed, and what was wrong (must stay 0).
  const uint64_t ro = g_rex_ro_checked, promoted = g_rex_promoted_calls;
  const uint64_t pcr = rex::syscount::CensusRead(rex::syscount::kCensusCritSecPcrOwner);
  const uint64_t ro_wrong = g_ro_mismatches.load(std::memory_order_relaxed);
  const uint64_t promoted_wrong = g_promoted_mismatches.load(std::memory_order_relaxed);
  const uint64_t pcr_wrong = rex::syscount::CensusRead(rex::syscount::kCensusCritSecOwnerMismatch);
  if (ro || promoted || pcr) {
    if (ro_wrong || promoted_wrong || pcr_wrong) {
      REXLOG_ERROR("[enh] DIFFERENCE: generated-code self-checks (cumulative): read-only constants {} of {} wrong, "
                   "promoted indirect calls {} of {} wrong, critical section owners from the PCR {} of {} wrong",
                   ro_wrong, ro, promoted_wrong, promoted, pcr_wrong, pcr);
    } else {
      REXLOG_INFO("[verify] generated-code self-checks so far: {} loads folded to read-only constants, {} promoted "
                  "indirect calls, {} critical section owners from the PCR; 0 differences",
                  ro, promoted, pcr);
    }
  }
  if (!g_rex_indirect_census) return;
  // The census: the whole table, most called sites first.
  struct Row {
    uint64_t key;
    Site site;
  };
  std::vector<Row> rows;
  for (Stripe& stripe : g_stripes) {
    std::lock_guard<std::mutex> lock(stripe.mutex);
    for (const auto& [key, site] : stripe.sites) rows.push_back({key, site});
  }
  std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
    return a.site.total != b.site.total ? a.site.total > b.site.total : a.key < b.key;
  });
  uint64_t calls = 0, single = 0, single_calls = 0, top_calls = 0;
  for (const Row& r : rows) {
    calls += r.site.total;
    const uint64_t top = *std::max_element(r.site.count, r.site.count + 4);
    top_calls += top;
    if (top == r.site.total) {
      ++single;
      single_calls += r.site.total;
    }
  }
  static uint64_t last_calls = 0;
  REXLOG_INFO("[census] indirect calls: {:.0f} per second; {} sites seen so far with {} calls, {} sites ({} calls) "
              "always went to one target, {:.1f} % of all calls went to their site's most frequent target",
              double(calls - last_calls) / double(seconds), rows.size(), calls, single, single_calls,
              calls ? 100.0 * double(top_calls) / double(calls) : 0.0);
  last_calls = calls;
  const std::string path = REXCVAR_GET(ds2_diag_indirect_census);
  const std::string temp = path + ".tmp";
  if (FILE* f = std::fopen(temp.c_str(), "w")) {
    std::fprintf(f, "# sa2-nx indirect call census (cumulative): <function> <site> <calls> <target>:<calls>... "
                    "other:<calls>\n");
    for (const Row& r : rows) {
      std::fprintf(f, "%08X %u %llu", unsigned(r.key >> 32), unsigned(r.key & 0xFFFFFFFFu),
                   static_cast<unsigned long long>(r.site.total));
      for (int i = 0; i < 4; ++i) {
        if (r.site.count[i]) {
          std::fprintf(f, " %08X:%llu", r.site.target[i], static_cast<unsigned long long>(r.site.count[i]));
        }
      }
      std::fprintf(f, " other:%llu\n", static_cast<unsigned long long>(r.site.other));
    }
    std::fclose(f);
    std::remove(path.c_str());
    std::rename(temp.c_str(), path.c_str());
  } else {
    REXLOG_WARN("[census] indirect calls: cannot write {}", temp);
  }
}

}  // namespace ds2::enh

#else  // the Switch build: nothing to check, nothing to count

namespace ds2::enh {
void CodegenChecksInit() {}
void CodegenChecksReport(int) {}
}  // namespace ds2::enh

#endif

// From sa2-nx 1.0.2 (its sa2_enhancements.cpp): tools/flush_mode_flow.py's PC self-check, the counts of the any-mode
// float helpers (rex/ppc/diet_ops.h), and the compare-and-swap barrier switch of tools/guest_cas.py and the SDK's
// lock words (rex/thread/atomic.h).
DS2_ENHANCEMENT(g_guest_cas_no_barrier, ds2_cpu_guest_cas_no_barrier, ds2::enh::kCpu, true,
                "Compare-and-swap (the game's interlocked operations, tools/guest_cas.py, and the SDK's lock words, "
                "rex/thread/atomic.h) without the legacy builtin's trailing `dmb ish` on the A57; off puts the barrier "
                "back at run time");
#if defined(__aarch64__)
extern "C" unsigned char g_rex_atomic_legacy_barrier;  // sdk/src/core/atomic_posix.cpp
#endif

#if !REX_PLATFORM_SWITCH
extern "C" {
volatile unsigned long long g_rex_flush_flow_checked = 0;  // a plain lossy count: "at least this many"
volatile unsigned long long g_rex_diet_census[6] = {};
}
namespace {
std::atomic<uint64_t> g_flush_flow_mismatches{0};
}
extern "C" void RexFlushFlowMismatch(int on) noexcept {
  ds2::enh::VerifyDifference();
  if (g_flush_flow_mismatches.fetch_add(1, std::memory_order_relaxed) < 16) {
    REXLOG_ERROR("[enh] DIFFERENCE: flush-mode flow: a check proven redundant had to switch the mode {} (return "
                 "address {})",
                 on ? "on" : "off", static_cast<const void*>(__builtin_return_address(0)));
  }
}
#endif

namespace ds2::enh {

// Called by Resolve, once the keys are known.
void CodegenRoundsInit() {
  CodegenChecksInit();
#if defined(__aarch64__)
  g_rex_atomic_legacy_barrier = g_guest_cas_no_barrier.active ? 0 : 1;
#endif
}

// Called with every '[enh] uses' report.
void CodegenRoundsReport(int seconds) {
#if !REX_PLATFORM_SWITCH
  if (const uint64_t proven = g_rex_flush_flow_checked) {
    const uint64_t wrong = g_flush_flow_mismatches.load(std::memory_order_relaxed);
    if (wrong) {
      REXLOG_ERROR("[enh] DIFFERENCE: flush-mode flow: {} of {} checks proven redundant had to switch the mode "
                   "(cumulative)",
                   wrong, proven);
    } else {
      REXLOG_INFO("[verify] flush-mode flow: {} checks proven redundant executed so far, 0 had to switch the mode",
                  proven);
    }
  }
#endif
  CodegenChecksReport(seconds);
}

}  // namespace ds2::enh
