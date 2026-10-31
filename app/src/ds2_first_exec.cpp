// Dark Souls II: the first time each guest function runs, for diagnostic builds (tools/pch_first_exec.py).
//
// The patched REX_FUNC_PROLOGUE of the generated code calls RexFirstExec once per function (a static flag of its
// own; two threads may both report one, which the reader tolerates). The records (guest address, milliseconds since
// the first one) go to an array and a thread appends the new ones to <log folder>/first_exec.bin every two seconds:
// the timeline of the code the game reaches. A screen that never comes shows where that timeline stops
// (tools/first_exec_report.py). Without the patched prologue nothing calls this and the thread never starts.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#if defined(__SWITCH__)
extern "C" const char* RexSwitchLogDir(void);  // sdk/src/ui/switch_crash_hooks.c, ends in '/'
#endif

namespace {

constexpr uint32_t kMaxRecords = 1u << 18;  // the image has ~84,000 functions
// A record is published whole: address in the high half, milliseconds in the low one; 0 = not written yet.
std::atomic<uint64_t> g_records[kMaxRecords];
std::atomic<uint32_t> g_count{0};
std::once_flag g_writer_once;
std::chrono::steady_clock::time_point g_start;

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
    std::this_thread::sleep_for(std::chrono::seconds(2));
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

extern "C" void RexFirstExec(const char* func) {
  std::call_once(g_writer_once, [] {
    g_start = std::chrono::steady_clock::now();
    std::thread(Writer).detach();
  });
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
