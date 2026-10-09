// Stage 2, phase A of the native renderer: confirm the map of the game's Direct3D before replacing anything.
//
// Dark Souls II carries the Xbox 360 XDK's Direct3D (2.0.21256.0) statically linked, around 0x82993000-0x829D0000.
// The candidates below come from the PM4 packet headers each function builds (`lis rX,0xC0nn; ori rX,rX,0xooxx` in
// out/default_image.bin, 2026-10-09; docs/ds2-notes.md, "Direct3D"). This file checks them at run time: it wraps each
// candidate, always calls the original, and with ds2_d3d_trace=true logs every 5 seconds how often each one ran, from
// where (its most frequent return addresses) and with which arguments (r3..r8 of its first calls). With the cvar off
// it only adds one branch per call.
//
// How the wrapping works: each recompiled function sub_XXXXXXXX is a weak alias of __imp__sub_XXXXXXXX. The strong
// definitions here win for every caller (direct, and indirect through the function table), and __imp__ is still the
// original.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <fmt/format.h>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(ds2_d3d_trace, false, "DS2",
                    "Stage 2 tracing: count the calls to the game's Direct3D candidates (docs/ds2-notes.md) and log "
                    "them every 5 s with their most frequent callers and the arguments of their first calls");

namespace {

struct Traced {
  uint32_t address;
  const char* what;
  std::atomic<uint64_t> calls{0};
  std::atomic<uint32_t> samples{0};  // argument lines logged so far (the first kArgumentSamples calls)
  std::mutex mu;
  std::unordered_map<uint32_t, uint64_t> callers;  // return address -> calls
};

constexpr uint32_t kArgumentSamples = 6;

Traced g_traced[] = {
    {0x829C2340, "DRAW_INDX n=2 (x2) + SET_BIN_MASK"},
    {0x829C2848, "DRAW_INDX n=4 (x2) + SET_BIN_MASK"},
    {0x829C2E28, "DRAW_INDX n=2 (x2) + SET_BIN_MASK, second copy"},
    {0x829C3240, "DRAW_INDX n=4 (x2) + SET_BIN_MASK, second copy"},
    {0x829C3E88, "DRAW_INDX_2 n=1"},
    {0x829AC6C0, "DRAW_INDX_2 n=1 after op 3B"},
    {0x829AC818, "IM_LOAD_IMMEDIATE x2 + DRAW_INDX_2"},
    {0x829B0E48, "DRAW_INDX_2 n=1"},
    {0x829CB130, "IM_LOAD_IMMEDIATE x2 + DRAW_INDX_2 + EVENT_WRITE"},
    {0x829CBAE8, "DRAW_INDX_2 n=1"},
    {0x829BA308, "IM_LOAD_IMMEDIATE + DRAW_INDX n=4"},
    {0x829BF328, "IM_LOAD n=2 (draw-time shader load?)"},
    {0x829BF138, "LOAD_ALU_CONSTANT n=3"},
    {0x829B7E58, "EVENT_WRITE + MEM_WRITE x2 + WAIT_REG_MEM x2 (swap?)"},
    {0x829A8978, "WAIT_REG_MEM"},
    {0x829A8CC8, "WAIT_REG_MEM"},
    {0x829BB060, "WAIT_REG_MEM x2"},
    {0x829BE2B8, "WAIT_REG_MEM"},
    {0x829BA900, "INTERRUPT n=3"},
    {0x829A86A8, "INDIRECT_BUFFER (kick-off?)"},
};

std::atomic<int64_t> g_next_report_ms{0};

void Report() {
  const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                       .count();
  int64_t due = g_next_report_ms.load(std::memory_order_relaxed);
  if (now < due || !g_next_report_ms.compare_exchange_strong(due, now + 5000)) {
    return;
  }
  REXLOG_INFO("[d3d-trace] calls since the last report:");
  for (auto& t : g_traced) {
    const uint64_t calls = t.calls.exchange(0, std::memory_order_relaxed);
    if (!calls) {
      continue;
    }
    std::vector<std::pair<uint32_t, uint64_t>> top;
    {
      std::lock_guard lock(t.mu);
      top.assign(t.callers.begin(), t.callers.end());
      t.callers.clear();
    }
    std::sort(top.begin(), top.end(), [](auto& a, auto& b) { return a.second > b.second; });
    std::string from;
    for (size_t i = 0; i < top.size() && i < 4; ++i) {
      from += fmt::format(" {:08X}x{}", top[i].first, top[i].second);
    }
    REXLOG_INFO("[d3d-trace]   sub_{:08X} {:<52} {:>8} calls, from{}", t.address, t.what, calls, from);
  }
}

inline void Record(Traced& t, const PPCContext& ctx) {
  t.calls.fetch_add(1, std::memory_order_relaxed);
  const uint32_t lr = uint32_t(ctx.lr);
  if (t.samples.load(std::memory_order_relaxed) < kArgumentSamples &&
      t.samples.fetch_add(1, std::memory_order_relaxed) < kArgumentSamples) {
    REXLOG_INFO("[d3d-trace] sub_{:08X} from {:08X}: r3={:08X} r4={:08X} r5={:08X} r6={:08X} r7={:08X} r8={:08X}",
                t.address, lr, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, ctx.r8.u32);
  }
  std::lock_guard lock(t.mu);
  ++t.callers[lr];
}

}  // namespace

// One wrapper per candidate: record (when tracing), report now and then, call the original.
#define DS2_TRACE_HOOK(index, addr)                    \
  extern "C" REX_FUNC(__imp__sub_##addr);              \
  REX_HOOK_RAW(sub_##addr) {                           \
    if (REXCVAR_GET(ds2_d3d_trace)) {                  \
      Record(g_traced[index], ctx);                    \
      Report();                                        \
    }                                                  \
    __imp__sub_##addr(ctx, base);                      \
  }

DS2_TRACE_HOOK(0, 829C2340)
DS2_TRACE_HOOK(1, 829C2848)
// 829C2E28 (DrawVertices) and 829C3240 (DrawIndexedVertices): confirmed; hooked by src/me/ds2_me_hooks.cpp now.
DS2_TRACE_HOOK(4, 829C3E88)
DS2_TRACE_HOOK(5, 829AC6C0)
DS2_TRACE_HOOK(6, 829AC818)
DS2_TRACE_HOOK(7, 829B0E48)
DS2_TRACE_HOOK(8, 829CB130)
DS2_TRACE_HOOK(9, 829CBAE8)
DS2_TRACE_HOOK(10, 829BA308)
// 829BF328 (the draw-time shader load): confirmed; hooked by src/me/ds2_me_hooks.cpp now.
DS2_TRACE_HOOK(12, 829BF138)
DS2_TRACE_HOOK(13, 829B7E58)
DS2_TRACE_HOOK(14, 829A8978)
DS2_TRACE_HOOK(15, 829A8CC8)
DS2_TRACE_HOOK(16, 829BB060)
DS2_TRACE_HOOK(17, 829BE2B8)
DS2_TRACE_HOOK(18, 829BA900)
DS2_TRACE_HOOK(19, 829A86A8)
