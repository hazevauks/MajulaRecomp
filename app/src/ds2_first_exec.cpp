// Dark Souls II: the first time each guest function runs, for diagnostic builds (tools/pch_first_exec.py).
//
// The patched REX_FUNC_PROLOGUE of the generated code calls RexFirstExec once per function (a static flag of its
// own; two threads may both report one, which the reader tolerates). The records (guest address, milliseconds since
// the program started) go to an array and a thread appends the new ones to <log folder>/first_exec.bin every two seconds:
// the timeline of the code the game reaches. A screen that never comes shows where that timeline stops
// (tools/first_exec_report.py). Without the patched prologue nothing calls this and the thread never starts.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#if defined(__SWITCH__)
#include <switch.h>
extern "C" const char* RexSwitchLogDir(void);  // sdk/src/ui/switch_crash_hooks.c, ends in '/'
#endif

namespace {

constexpr uint32_t kMaxRecords = 1u << 18;  // the image has ~84,000 functions
// A record is published whole: address in the high half, milliseconds in the low one; 0 = not written yet.
std::atomic<uint64_t> g_records[kMaxRecords];
std::atomic<uint32_t> g_count{0};
std::atomic<bool> g_writer_started{false};
// Milliseconds from the program's start (static initialisation, before any guest code).
const std::chrono::steady_clock::time_point g_start = std::chrono::steady_clock::now();

std::string OutputPath() {
#if defined(__SWITCH__)
  return std::string(RexSwitchLogDir()) + "first_exec.bin";
#else
  return "first_exec.bin";
#endif
}

void Writer() {
  const std::string path = OutputPath();
  if (FILE* f = std::fopen(path.c_str(), "wb")) std::fclose(f);  // a fresh file each run
  uint32_t written = 0;
  for (;;) {
#if defined(__SWITCH__)
    svcSleepThread(2'000'000'000);
#else
    std::this_thread::sleep_for(std::chrono::seconds(2));
#endif
    const uint32_t count = std::min(g_count.load(std::memory_order_acquire), kMaxRecords);
    if (count == written) continue;
    FILE* f = std::fopen(path.c_str(), "ab");
    if (!f) continue;
    // Up to the first record not yet published by its thread; the rest go in a later round.
    while (written < count) {
      const uint64_t r = g_records[written].load(std::memory_order_acquire);
      if (!r) break;
      std::fwrite(&r, sizeof(r), 1, f);
      ++written;
    }
    std::fclose(f);
  }
}

}  // namespace

// --- Call counts (tools/pch_first_exec.py --counts): every generated function counts its calls in a slot of its own;
// a thread appends, every five seconds, the slots that changed to <log folder>/func_counts.bin as a record
// {0xFFFFFFFF, milliseconds} followed by {guest address, calls so far} pairs (tools/first_exec_report.py --counts).
namespace {
constexpr uint32_t kMaxCounters = 1u << 17;
uint32_t g_counter_address[kMaxCounters];
uint32_t g_counters[kMaxCounters];  // plain increments: a lost count between threads does not matter here
std::atomic<uint32_t> g_counter_count{0};
std::atomic<bool> g_counts_started{false};
uint32_t g_counter_overflow = 0;

void CountsWriter() {
  const std::string path =
#if defined(__SWITCH__)
      std::string(RexSwitchLogDir()) + "func_counts.bin";
#else
      "func_counts.bin";
#endif
  if (FILE* f = std::fopen(path.c_str(), "wb")) std::fclose(f);
  static uint32_t last[kMaxCounters];
  for (;;) {
#if defined(__SWITCH__)
    svcSleepThread(5'000'000'000);
#else
    std::this_thread::sleep_for(std::chrono::seconds(5));
#endif
    const uint32_t n = std::min(g_counter_count.load(std::memory_order_acquire), kMaxCounters);
    FILE* f = std::fopen(path.c_str(), "ab");
    if (!f) continue;
    const uint32_t head[2] = {
        0xFFFFFFFFu, uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::steady_clock::now() - g_start).count())};
    std::fwrite(head, sizeof(head), 1, f);
    for (uint32_t i = 0; i < n; ++i) {
      const uint32_t c = g_counters[i];
      if (c == last[i] || !g_counter_address[i]) continue;
      last[i] = c;
      const uint32_t pair[2] = {g_counter_address[i], c};
      std::fwrite(pair, sizeof(pair), 1, f);
    }
    std::fclose(f);
  }
}
}  // namespace

extern "C" uint32_t* RexFuncCounter(const char* func) {
  if (!g_counts_started.exchange(true, std::memory_order_acq_rel)) {
#if defined(__SWITCH__)
    static Thread thread;
    if (R_SUCCEEDED(threadCreate(&thread, [](void*) { CountsWriter(); }, nullptr, nullptr, 0x10000, 0x3B, -2)))
      threadStart(&thread);
#else
    std::thread(CountsWriter).detach();
#endif
  }
  const uint32_t i = g_counter_count.fetch_add(1, std::memory_order_acq_rel);
  if (i >= kMaxCounters) return &g_counter_overflow;
  const size_t n = std::strlen(func);
  g_counter_address[i] = n >= 8 ? uint32_t(std::strtoul(func + n - 8, nullptr, 16)) : 1;
  return &g_counters[i];
}

extern "C" void RexFirstExec(const char* func) {
  if (!g_writer_started.exchange(true, std::memory_order_acq_rel)) {
#if defined(__SWITCH__)
    // std::thread and std::call_once end in "Function not implemented" under libnx (the thirty-eighth build's
    // crash at start).
    static Thread thread;
    if (R_SUCCEEDED(threadCreate(&thread, [](void*) { Writer(); }, nullptr, nullptr, 0x10000, 0x3B, -2)))
      threadStart(&thread);
#else
    std::thread(Writer).detach();
#endif
  }
  // __func__ of a generated function: "__imp__sub_XXXXXXXX".
  const size_t n = std::strlen(func);
  // A name without an address (a helper the generator named) is recorded as 1.
  uint32_t address = n >= 8 ? uint32_t(std::strtoul(func + n - 8, nullptr, 16)) : 0;
  if (!address) address = 1;
  const uint32_t ms = uint32_t(
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - g_start).count());
  const uint32_t i = g_count.fetch_add(1, std::memory_order_acq_rel);
  // Guest addresses are never 0, so neither is a written record.
  if (i < kMaxRecords) g_records[i].store(uint64_t(address) << 32 | ms, std::memory_order_release);
}
