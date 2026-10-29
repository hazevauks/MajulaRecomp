/**
 * @file    gpu/backend/queued.cpp
 */
#include "renut_engine/gpu/backend/queued.h"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <rex/cvar.h>

#include "renut_engine/gpu/guest/callbacks.h"
#include "renut_engine/gpu/guest/command_buffers.h"
#include "renut_engine/gpu/d3d/d3d.h"
#include "renut_engine/gpu/guest/guest_memory.h"
#include "renut_engine/gpu/log.h"
#include "renut_engine/gpu/backend/render_thread.h"
#include "renut_engine/gpu/banjo_renut_tuning.h"
#include "renut_engine/gpu/ds2_line_compare.h"
#include "renut_engine/gpu/ds2_setter_marks.h"
#if defined(SA2_D3D_CENSUS_BUILD)
#include "me/sa2_d3d_api.h"  // sa2-nx (2026-10-28): the setter census (census builds only)
#endif

// sa2-nx (2026-10-20): see SnapshotKind below.
REXCVAR_DEFINE_BOOL(ds2_renut_snapshot_deltas, true, "DS2/Renderer",
                    "Renut's renderer: a draw keeps what changed in the device since the device's previous draw (its "
                    "64-byte chunks that differ, and the float constant groups the game marked dirty) instead of a "
                    "copy of the device's first 13,088 bytes; the render thread rebuilds the device from them. Off: "
                    "the whole copy for every draw")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(ds2_renut_snapshot_deltas_constants, true, "DS2/Renderer",
                    "Renut's renderer (ds2_renut_snapshot_deltas): the chain's draws tell the constant uploads which "
                    "float constant groups changed since the previous draw, so only those are compared with what the "
                    "GPU holds (as a recorded command buffer's replayed draws do); a frame's first draw compares them "
                    "all. Off: every draw compares every group its shaders read")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(ds2_renut_capture_batches, true, "DS2/Renderer",
                    "Renut's renderer (with banjo_renut_capture_pool): the game thread takes its draws' captures from "
                    "the pool, and the render thread gives them back, 32 at a time instead of one lock of the pool's "
                    "per draw on each thread. Off: one at a time")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(ds2_renut_snapshot_deltas_narrow, true, "DS2/Renderer",
                    "Renut's renderer (ds2_renut_snapshot_deltas): the game thread compares only the device chunks the "
                    "render thread reads (40 of 77: not the dirty masks, ring pointers and call tables before the fetch "
                    "constants, nor the private words from 11008 to 12223, which only the hooks use on the live "
                    "device). The second console run: comparing all 77 was 9 % of the game thread. Off: all 77")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(ds2_renut_deferred_chain, true, "DS2/Renderer",
                    "Renut's renderer (from futari-nx, as peggle2-nx has it): Begin*Vertices draws are deltas of the "
                    "device snapshot chain too, not whole 13 KB copies (ds2_renut_snapshot_deltas). Made and queued "
                    "under the chain's lock; every hook queues its device's waiting Begin*Vertices draw first, so the "
                    "chain stays in order. Off: whole copies outside the chain, as before")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(ds2_renut_neon_compare, true, "DS2/Renderer",
                    "Renut's renderer on the Switch: the device chunks of the delta chain and the pushed shared "
                    "constants' lines are compared 64 bytes at a time with NEON (four 128-bit loads a side) instead of "
                    "eight 64-bit words, which GCC left scalar. Off: the 64-bit words")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(ds2_renut_snapshot_deltas_verify, false, "DS2/Renderer",
                    "Tests (ds2_renut_snapshot_deltas): every draw also keeps the whole copy, and the render thread "
                    "counts the draws whose rebuilt device differs from it in any byte (the 10 s \"[renut] device "
                    "snapshots\" line)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
// ds2-nx (2026-10-29): off. The marks come from SA2's Direct3D setter hooks (sa2-nx's me/sa2_setter_marks.h), which
// this port does not have: nothing would mark the setters that store without a dirty tag.
REXCVAR_DEFINE_BOOL(ds2_renut_setter_marks, false, "DS2/Renderer",
                    "Renut's renderer (ds2_renut_snapshot_deltas): a draw of the chain compares only the device chunks "
                    "marked since the previous one (gpu/ds2_setter_marks.h: by the Direct3D setters that store without "
                    "a dirty tag, by the dirty tags pending, and the vertex declaration's) instead of all 40. Off: all")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(ds2_renut_setter_marks_check, true, "DS2/Renderer",
                    "Renut's renderer (ds2_renut_setter_marks): every 32nd draw of the chain (every draw with "
                    "ds2_renut_snapshot_deltas_verify) compares all 40 chunks, and a changed chunk no mark named turns "
                    "the marks off for the session (logged). Off: no check")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(ds2_renut_marks_copy, false, "DS2/Renderer",  // ds2-nx: off with the marks
                    "Renut's renderer (ds2_renut_setter_marks): a draw of the chain copies its marked chunks as they "
                    "are, without comparing them with the chain's shadow first (the compare's loads were most of "
                    "CaptureDelta on the console); the render thread, whose copy of the device is in its cache, finds "
                    "which of them changed as it applies them. The every-32nd-draw check still compares all 40. Off: "
                    "the marked chunks compared on the game thread")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(ds2_renut_setters_precise, false, "DS2/Renderer",
                    "Renut's renderer (ds2_renut_setter_marks; banjo-nx's setter rewrite): SetTexture, SetStreamSource, "
                    "the fetch word setters and the float constant uploads mark only the chunks whose bytes they "
                    "changed, and the dirty bits they add go apart from the device until a function that reads them "
                    "runs (me/ds2_setter_marks.h), so a draw takes only what changed. The every-32nd-draw check also "
                    "compares the float constants with the render thread's. Off by default (the user's call, "
                    "2026-10-28): on the console the setters' own work cost about 2 ms a frame more than the draws "
                    "saved (SA2's setters were native already). Off: their marks and dirty bits as before")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DECLARE(bool, ds2_renut_lockfree_queue);  // backend/render_thread.cpp

namespace renut::gpu {

namespace {

// sa2-nx (2026-10-28, the ARM audit after the console's second run): the keys every draw reads (a dozen: the chain's,
// the pool's, the lock's), latched once into plain bools. Each had been a function-local static, whose guard is an
// ldar and a branch on AArch64 at every call (the ldar can wait for the stores before it). The latch is made on the
// thread of the first capture, before the render thread exists (its creation orders the latch before it), with the
// fields stored before 'ready'; the keys are init-only.
struct QueuedKeys {
  bool ready = false;
  bool deltas = false, constants = false, batches = false, neon = false, deferred = false, narrow = false;
  bool verify = false, marks = false, check = false, copy = false, precise = false, spin = false;
};
QueuedKeys g_keys;
[[gnu::noinline]] void LatchKeys() {
  QueuedKeys k;
  k.deltas = REXCVAR_GET(ds2_renut_snapshot_deltas);
  k.constants = k.deltas && REXCVAR_GET(ds2_renut_snapshot_deltas_constants);
  k.batches = REXCVAR_GET(ds2_renut_capture_batches);
  k.neon = REXCVAR_GET(ds2_renut_neon_compare);
  k.deferred = REXCVAR_GET(ds2_renut_deferred_chain);
  k.narrow = REXCVAR_GET(ds2_renut_snapshot_deltas_narrow);
  k.verify = k.deltas && REXCVAR_GET(ds2_renut_snapshot_deltas_verify);
#if defined(SA2_D3D_CENSUS_BUILD)
  k.marks = false;  // the census compares every chunk
#else
  k.marks = k.deltas && REXCVAR_GET(ds2_renut_setter_marks);
#endif
  k.check = REXCVAR_GET(ds2_renut_setter_marks_check);
  k.copy = k.marks && REXCVAR_GET(ds2_renut_marks_copy);
  k.precise = k.marks && REXCVAR_GET(ds2_renut_setters_precise);
  k.spin = REXCVAR_GET(ds2_renut_lockfree_queue);
  k.ready = false;
  g_keys = k;
  std::atomic_thread_fence(std::memory_order_release);
  g_keys.ready = true;
}
[[gnu::always_inline]] inline const QueuedKeys &Keys() {
  if (!g_keys.ready) [[unlikely]]
    LatchKeys();
  return g_keys;
}

// sa2-nx (2026-10-20, ds2_renut_snapshot_deltas): how a queued draw holds its device.
//   Whole:  DrawCapture::state is the device's first kDeviceSnapshotSize bytes (every draw before; the Begin*Vertices
//           draws, which are queued later than they are captured, still).
//   Rebase: the same, and the render thread takes it as its device (the chain's first draw, and each frame's).
//   Delta:  DrawCapture::state is a DeltaHeader and the bytes it names: the render thread applies them to its device.
// SA2 records no command buffers, so every draw was a whole copy: 13 KB a draw, 15-18 MB a frame of City Escape's
// 1,100-1,400 draws written on the game thread, into pooled captures last touched by the render thread on another
// core, then read back by the render thread. A draw changes a few bindings, render states and constants: the deltas are
// those. The chain's order is the queue's: deltas are made and queued under g_delta.mutex, and the render thread runs
// the queue in order.
enum class SnapshotKind : u8 { Whole, Rebase, Delta };

// What a queued draw reads besides guest memory the game keeps until a fence.
struct DrawCapture {
  std::vector<u8> state;  // the device, kDeviceSnapshotSize bytes (or a delta: SnapshotKind)
  std::vector<u8> vertices, indices;
  SnapshotKind snapshot = SnapshotKind::Whole;  // sa2-nx (2026-10-20)
  std::vector<u8> verify;  // sa2-nx: the whole device as well (ds2_renut_snapshot_deltas_verify)
  DrawCall call;
  // banjo-nx (2026-10-19, CPU proposal 23): a queued clear's or resolve's arguments. They come from the same pool as
  // the draws' captures: each made three heap allocations on the game thread (its arguments, a device snapshot, a
  // closure too large for std::function's own storage), freed on the render thread through the one malloc lock.
  u32 flags = 0;
  bool has_rect = false, has_color = false, has_dest_format = false;
  ClearRect rect{};
  float color[4] = {};
  float depth = 0.0f;
  u32 stencil = 0;
  u32 dest_texture_va = 0, dest_level = 0, dest_slice = 0;
  i32 dest_x = 0, dest_y = 0;
  d3d::TextureFetchConstant dest_format{};
};

// banjo-nx (banjo_renut_capture_pool): a draw's capture comes from a pool and goes back to it once the render thread
// has drawn it. Each draw made three heap allocations on the game thread (the capture, its 12.7 KB device snapshot,
// the queued closure) and freed them on the render thread: about 2,000 a frame through the Switch's one malloc lock.
// A pooled capture keeps its snapshot's storage, and the closure holds a plain pointer, which std::function stores
// without allocating. Off: every capture is deleted after use, as before (a new one per draw).
// sa2-nx (2026-10-20, ds2_renut_capture_batches): the pool's lock was taken twice a draw, by the game thread (Take) and
// by the render thread (Give), on two cores: its cache line went back and forth about 1,800 times a City Escape frame.
// Each thread now keeps a few captures of its own and goes to the pool kBatch at a time.
bool CaptureBatchesOn();

class CapturePool {
public:
  // sa2-nx (2026-10-20): 'chain_locked' when the caller holds g_delta.mutex (the chain's draws): its batch is a plain
  // list that lock guards. Thread-local lists cost a call a use here (this toolchain's TLS goes through
  // __aarch64_read_tp: 0.8 % of the render thread in the second console run).
  DrawCapture *Take(bool chain_locked = false) {
    if (CaptureBatchesOn() && chain_locked) {
      std::vector<DrawCapture *> &taken = chain_taken_;
      if (taken.empty()) {
        std::lock_guard lock(mutex_);
        const size_t n = std::min(kBatch, free_.size());
        taken.assign(free_.end() - std::ptrdiff_t(n), free_.end());
        free_.resize(free_.size() - n);
      }
      if (!taken.empty()) {
        DrawCapture *capture = taken.back();
        taken.pop_back();
        return capture;
      }
      return new DrawCapture();
    }
    {
      std::lock_guard lock(mutex_);
      if (!free_.empty()) {
        DrawCapture *capture = free_.back();
        free_.pop_back();
        return capture;
      }
    }
    return new DrawCapture();
  }
  // sa2-nx (2026-10-28, the ARM audit of the console's second profile): the chain's next captures, whose lines the
  // render thread had last (it drew them and gave them back): the one after next's own lines, and the next one's delta
  // buffer (its first lines: the header and a few chunks), into the game thread's cache for writing while this draw is
  // made. CaptureDelta's samples were spread over its first stores into a capture (its call, its header, the pooled
  // vector's lines). Under g_delta.mutex, as Take(true).
  void PrefetchChainNext() const {
    const size_t n = chain_taken_.size();
    if (n >= 1) {
      if (const u8 *s = chain_taken_[n - 1]->state.data()) {
        for (u32 k = 0; k < 8; ++k)
          __builtin_prefetch(s + 64 * k, 1);
      }
    }
    if (n >= 2) {
      const char *c = reinterpret_cast<const char *>(chain_taken_[n - 2]);
      for (size_t k = 0; k < sizeof(DrawCapture); k += 64)
        __builtin_prefetch(c + k, 1);
    }
  }
  // sa2-nx (2026-10-20): 'render_thread' from the render thread's items (only that thread then touches its batch; a
  // queue run inline at shutdown has the render thread gone).
  void Give(DrawCapture *capture, bool render_thread = false) {
    if (!capture)
      return;
    if (banjo::renut_gpu::CapturePoolOn()) {
      capture->vertices.clear();
      capture->indices.clear();
      if (CaptureBatchesOn() && render_thread) {
        std::vector<DrawCapture *> &given = render_given_;
        given.push_back(capture);
        if (given.size() < kBatch)
          return;
        std::lock_guard lock(mutex_);
        for (DrawCapture *c : given) {
          if (free_.size() < kMaxFree)
            free_.push_back(c);
          else
            delete c;
        }
        given.clear();
        return;
      }
      std::lock_guard lock(mutex_);
      if (free_.size() < kMaxFree) {
        free_.push_back(capture);
        return;
      }
    }
    delete capture;
  }

private:
  static constexpr size_t kMaxFree = 2048;  // more than a frame's draws in flight
  static constexpr size_t kBatch = 32;      // sa2-nx: captures a thread takes or gives back at once
  std::mutex mutex_;
  std::vector<DrawCapture *> free_;
  std::vector<DrawCapture *> chain_taken_;   // sa2-nx: under g_delta.mutex
  std::vector<DrawCapture *> render_given_;  // sa2-nx: the render thread's
};
CapturePool g_capture_pool;

// A Begin*Vertices draw waiting for its vertices, per device.
struct DeferredDraw {
  DrawCapture *capture = nullptr;  // owned (g_capture_pool)
  const u8 *scratch = nullptr;
  u32 bytes = 0;
  const u8 *index_scratch = nullptr;  // ds2-nx (2026-10-24): BeginIndexedVertices
  u32 index_bytes = 0;
};
std::mutex g_deferred_mutex;
std::unordered_map<u32, DeferredDraw> g_deferred;
// sa2-nx (2026-10-20, ds2_renut_callbacks_fast): g_deferred.size(), kept under g_deferred_mutex and read without it
// (FlushDeferredVertices: every hook calls it first). A Begin*Vertices draw is flushed by the next hook of its device,
// on the thread that made it, which sees the count in program order.
std::atomic<size_t> g_deferred_count{0};

// sa2-nx (2026-10-20, ds2_renut_snapshot_deltas): the delta chain's layout. Of the device's first kDeviceSnapshotSize
// bytes, the float constants (VertexShaderF, PixelShaderF: 8 KB, two thirds) go by the groups m_Pending marks dirty
// (one bit per four registers, bit 63 - group, as the recorded command buffers keep them: command_buffers.cpp); the
// rest goes by 64-byte chunks (30 before the constants, 47 after them, the last one 32 bytes), those that differ from
// the chain's previous draw (compared on the game thread with its shadow).
constexpr u32 kDeltaVertexF = u32(offsetof(d3d::Device, m_Constants) + offsetof(d3d::Constants, VertexShaderF));
constexpr u32 kDeltaPixelF = u32(offsetof(d3d::Device, m_Constants) + offsetof(d3d::Constants, PixelShaderF));
constexpr u32 kDeltaTailBegin = kDeltaPixelF + 256 * 16;
constexpr u32 kDeltaUnitBytes = 64;  // a chunk, and a constant group
constexpr u32 kDeltaHeadChunks = kDeltaVertexF / kDeltaUnitBytes;
constexpr u32 kDeltaTailBytes = d3d::kDeviceSnapshotSize - kDeltaTailBegin;
constexpr u32 kDeltaChunks = kDeltaHeadChunks + (kDeltaTailBytes + kDeltaUnitBytes - 1) / kDeltaUnitBytes;
static_assert(kDeltaVertexF % kDeltaUnitBytes == 0 && kDeltaPixelF == kDeltaVertexF + 256 * 16);
static_assert(kDeltaTailBegin == u32(offsetof(d3d::Device, m_Constants) + offsetof(d3d::Constants, VertexShaderB)));
static_assert(kDeltaChunks <= 128);
static_assert(kDeltaHeadChunks == kChainHeadChunks && kDeltaTailBegin == kChainTailBegin);  // draw.h's ChainHint

constexpr u32 DeltaChunkOffset(u32 i) {
  return i < kDeltaHeadChunks ? i * kDeltaUnitBytes : kDeltaTailBegin + (i - kDeltaHeadChunks) * kDeltaUnitBytes;
}
constexpr u32 DeltaChunkSize(u32 i) {
  return i < kDeltaHeadChunks ? kDeltaUnitBytes
                              : std::min(kDeltaUnitBytes, kDeltaTailBytes - (i - kDeltaHeadChunks) * kDeltaUnitBytes);
}

// sa2-nx (2026-10-20, ds2_renut_snapshot_deltas_narrow): the device bytes the render thread never reads from a draw's
// device (DrawStateLocked and what it calls: the accessors of render/draw.cpp, textures.cpp, surfaces.cpp, the pipeline
// key): before the fetch constants (the dirty masks, ring pointers and Set/Get*State call tables, which a recorded
// command buffer's replay leaves zero too), and the private words 11008-12223 (the creation thread, the fences, the
// swap flags: the hooks read and write them on the live device). Their chunks are not compared; the render thread's
// device keeps them as the frame's whole copy had them.
constexpr u32 kDeltaUnreadEnd = kDeltaVertexF - 0x300;  // 0x480: the fetch constants' start
constexpr u32 kDeltaPrivateUnreadBegin = 11008, kDeltaPrivateUnreadEnd = 12224;
constexpr bool DeltaChunkRead(u32 i) {
  const u32 at = DeltaChunkOffset(i);
  return !(at < kDeltaUnreadEnd || (at >= kDeltaPrivateUnreadBegin && at + 64 <= kDeltaPrivateUnreadEnd));
}
static_assert(kDeltaUnreadEnd == 0x480 && kDeltaUnreadEnd % kDeltaUnitBytes == 0);
static_assert((kDeltaPrivateUnreadBegin - kDeltaTailBegin) % kDeltaUnitBytes == 0 &&
              (kDeltaPrivateUnreadEnd - kDeltaTailBegin) % kDeltaUnitBytes == 0);
static_assert(d3d::kDeviceOffsetVertexDeclaration >= kDeltaPrivateUnreadEnd);
// The chunks compared: all, or the read ones.
struct DeltaChunkList {
  u32 count = 0;
  u8 index[128] = {};
};
constexpr DeltaChunkList MakeDeltaChunkList(bool narrow) {
  DeltaChunkList l{};
  for (u32 i = 0; i < kDeltaChunks; ++i)
    if (!narrow || DeltaChunkRead(i))
      l.index[l.count++] = u8(i);
  return l;
}
constexpr DeltaChunkList kDeltaAllChunks = MakeDeltaChunkList(false);
constexpr DeltaChunkList kDeltaReadChunks = MakeDeltaChunkList(true);
static_assert(kDeltaReadChunks.count == 40);
// sa2-nx (2026-10-28, ds2_renut_setter_marks): the same lists as chunk masks (bit i % 64 of word i / 64), and the
// chunks every draw compares whatever the marks say: the vertex declaration's (0x2FC0, set inline without a tag).
constexpr ds2::renut::ChunkBits MakeDeltaChunkMask(const DeltaChunkList &l) {
  ds2::renut::ChunkBits b{};
  for (u32 k = 0; k < l.count; ++k)
    b.w[l.index[k] / 64] |= uint64_t(1) << (l.index[k] % 64);
  return b;
}
constexpr ds2::renut::ChunkBits kDeltaAllMask = MakeDeltaChunkMask(kDeltaAllChunks);
constexpr ds2::renut::ChunkBits kDeltaReadMask = MakeDeltaChunkMask(kDeltaReadChunks);
constexpr ds2::renut::ChunkBits kMarksAlways = ds2::renut::MarkBits(d3d::kDeviceOffsetVertexDeclaration, 4);
static_assert(ds2::renut::kMarkHeadChunks == kDeltaHeadChunks && ds2::renut::kMarkTailBegin == kDeltaTailBegin &&
              ds2::renut::kMarkSnapshotEnd == d3d::kDeviceSnapshotSize);
static_assert(kMarksAlways.w[0] == uint64_t(1) << 63 && kMarksAlways.w[1] == 0);  // chunk 63: 0x2FC0
// Whether device byte 'at' is one the render thread reads (the verify mode compares those).
constexpr bool DeltaByteRead(u32 at) {
  return !(at < kDeltaUnreadEnd || (at >= kDeltaPrivateUnreadBegin && at < kDeltaPrivateUnreadEnd));
}

struct DeltaHeader {
  uint64_t chunks[2];  // chunk i follows: bit i % 64 of word i / 64
  uint64_t vs, ps;     // the float constant groups that follow (bit 63 - group), the vertex shader's first
};
// The most a delta holds: every chunk and every group.
constexpr size_t kDeltaMaxBytes = sizeof(DeltaHeader) + d3d::kDeviceSnapshotSize;

// Calls f(first, count) for each run of consecutive groups in an m_Pending mask (bit 63 - group).
template <typename F>
void ForEachGroupRun(uint64_t mask, F f) {
  while (mask) {
    const u32 first = u32(std::countl_zero(mask));
    const u32 count = u32(std::countl_one(mask << first));
    f(first, count);
    mask = first + count >= 64 ? 0 : mask & (~uint64_t(0) >> (first + count));
  }
}

// Whether two whole chunks differ: fixed-size loads the compiler keeps inline and vectorises.
bool DeltaChunkDiffers(const u8 *a, const u8 *b) {
  uint64_t diff = 0;
  for (u32 k = 0; k < kDeltaUnitBytes; k += 8) {
    uint64_t x, y;
    std::memcpy(&x, a + k, 8);
    std::memcpy(&y, b + k, 8);
    diff |= x ^ y;
  }
  return diff != 0;
}

// sa2-nx (2026-10-28, ds2_renut_lockfree_queue): the chain's lock. The game thread takes it for every draw (making and
// queueing its delta), and nothing else but the frame's start and the 10 s report: an uncontended std::mutex there was
// a call into newlib and libnx each way (mutexLock / mutexUnlock / pthread_mutex_lock: 2 % of the game thread in the 2P
// console profile, with the queue's). With the key on, an exclusive load/store pair takes it and a release store gives
// it back; a thread that finds it held yields until it is free (the holders keep it for a few hundred instructions).
// Off: the std::mutex.
bool ChainSpinOn() {
  return Keys().spin;
}
class ChainLock {
public:
  void lock() {
    if (!ChainSpinOn()) {
      mutex_.lock();
      return;
    }
    while (held_.exchange(true, std::memory_order_acquire)) {
      while (held_.load(std::memory_order_relaxed))
        std::this_thread::yield();
    }
  }
  bool try_lock() {
    if (!ChainSpinOn())
      return mutex_.try_lock();
    return !held_.exchange(true, std::memory_order_acquire);
  }
  void unlock() {
    if (!ChainSpinOn()) {
      mutex_.unlock();
      return;
    }
    held_.store(false, std::memory_order_release);
  }

private:
  std::atomic<bool> held_{false};
  std::mutex mutex_;
};
using ChainGuard = std::unique_lock<ChainLock>;

// The game threads' side of the chain.
struct DeltaChain {
  ChainLock mutex;                      // the chain's draws are made and queued in one order
  const d3d::Device *device = nullptr;  // the device it follows; none: the next draw rebases
  uint64_t vs = 0, ps = 0;              // groups whole captures consumed since its last draw: they follow
  uint64_t deltas = 0, rebases = 0, bytes = 0, chunks = 0, groups = 0;  // the 10 s report's
  uint64_t whole = 0;  // sa2-nx: whole captures outside the chain (Begin*Vertices draws), under the lock too
  // sa2-nx (2026-10-28, ds2_renut_setter_marks): chunks compared (the report's), the chain's draws (every 32nd
  // checks), checks and misses (a changed chunk no mark named).
  uint64_t compared = 0, mark_draws = 0, checks = 0, misses = 0;
  bool marks_off = false;  // the first miss turns the marks off for the session
  // sa2-nx (2026-10-28, ds2_renut_setters_precise): the checks' float constant misses (a group that differs from the
  // render thread's with no dirty bit: the precise setters go off for the session), and the chunks a delta copied as
  // marked (ds2_renut_marks_copy, the report's).
  uint64_t const_misses = 0, copied = 0;
  u32 const_check_next = 0;  // the first of the 16 groups the next check compares
  alignas(64) u8 shadow[d3d::kDeviceSnapshotSize];  // the device at its last draw (the chunks)
  alignas(64) u8 const_shadow[512 * 16];  // sa2-nx: the float constants the render thread's device has (precise)
};
DeltaChain g_delta;

// The render thread's side: the device the chain's draws rebuild.
alignas(64) u8 g_delta_device[d3d::kDeviceSnapshotSize];
std::atomic<uint64_t> g_delta_verified{0}, g_delta_differ{0};
// ds2_renut_snapshot_deltas_constants: the chain as a replay of the constant uploads' tracking (draw.h,
// ReplayConstants): its id (a new one at each whole copy; the top bit keeps it apart from the command buffers'
// replays, numbered from 1) and the float constant groups written since the upload last consumed them.
uint64_t g_chain_id = 0, g_chain_count = 0;
uint64_t g_chain_vs_dirty = ~uint64_t(0), g_chain_ps_dirty = ~uint64_t(0);
// ds2_renut_chain_reuse: the number of the chain's last draw (draw.h ChainHint::seq; from 2).
uint64_t g_chain_seq = 1;

// sa2-nx (2026-10-28): the keys, latched (QueuedKeys above).
bool DeltasOn() { return Keys().deltas; }
bool ChainConstantsOn() { return Keys().constants; }
bool CaptureBatchesOn() { return Keys().batches; }
bool NeonCompareOn() { return Keys().neon; }
// sa2-nx (2026-10-20, from futari-nx via peggle2-nx): ds2_renut_deferred_chain (QueueDeferredDraw).
bool DeferredChainOn() { return Keys().deferred; }
bool DeltasNarrowOn() { return Keys().narrow; }
bool DeltasVerifyOn() { return Keys().verify; }
// sa2-nx (2026-10-28): ds2_renut_setter_marks (not in census builds, whose census compares every chunk).
bool SetterMarksOn() { return Keys().marks; }
bool SetterMarksCheckOn() { return Keys().check; }
// sa2-nx (2026-10-28): ds2_renut_marks_copy and ds2_renut_setters_precise (both with the marks only).
bool MarksCopyOn() { return Keys().copy; }
bool SettersPreciseOn() { return Keys().precise; }

// The original draw calls consume the device's dirty masks as they write
// their packets; a snapshot taken after this has them clear, as the command
// buffer ops' do.
void ClearPending(d3d::Device *device) {
  // sa2-nx (ds2_renut_snapshot_deltas): the dirty constant groups a whole capture consumes go with the chain's next
  // delta (its callers hold g_delta.mutex when the deltas are on).
  if (device == g_delta.device) {
    g_delta.vs |= uint64_t(device->m_Pending[0]);
    g_delta.ps |= uint64_t(device->m_Pending[1]);
    // sa2-nx (2026-10-28, ds2_renut_setter_marks): and the chunks of the dirty tags it consumes.
    if (SetterMarksOn())
      ds2::renut::Mark(ds2::renut::MarkedDevice(),
                       ds2::renut::TagChunks(uint64_t(device->m_Pending[2]), uint64_t(device->m_Pending[3]),
                                             uint64_t(device->m_Pending[4])));
  }
  // sa2-nx (ds2_renut_setters_precise): the bits held apart for this device are consumed with its own (their state is
  // the render thread's already: nothing to carry).
  if (const u32 marked = ds2::renut::MarkedDevice(); marked && GuestPtr<d3d::Device>(marked) == device)
    ds2::renut::DropApart();
  for (auto &mask : device->m_Pending)
    mask = 0;
}

// sa2-nx (ds2_renut_snapshot_deltas), on the render thread: the device a queued draw reads.
const u8 *DeviceForDraw(const DrawCapture &capture) {
  if (capture.snapshot == SnapshotKind::Whole) {
    g_chain_hint = {};  // not a chain draw (ds2_renut_chain_reuse)
    return capture.state.data();
  }
  if (capture.snapshot == SnapshotKind::Rebase) {
    std::memcpy(g_delta_device, capture.state.data(), d3d::kDeviceSnapshotSize);
    g_chain_id = (uint64_t(1) << 63) | ++g_chain_count;  // its first draw compares every group
    g_chain_vs_dirty = g_chain_ps_dirty = ~uint64_t(0);
    g_chain_hint.seq = ++g_chain_seq;
    g_chain_hint.changed[0] = g_chain_hint.changed[1] = ~uint64_t(0);  // a whole copy: every chunk
  } else {
    const u8 *from = capture.state.data();
    DeltaHeader h;
    std::memcpy(&h, from, sizeof(h));
    from += sizeof(h);
    g_chain_hint.seq = ++g_chain_seq;
    g_chain_hint.changed[0] = h.chunks[0];
    g_chain_hint.changed[1] = h.chunks[1];
    g_chain_vs_dirty |= h.vs;
    g_chain_ps_dirty |= h.ps;
    if (MarksCopyOn()) {
      // sa2-nx (2026-10-28, ds2_renut_marks_copy): the game thread copied its marked chunks without comparing them:
      // a chunk is applied, and named changed for the reuse of derived state (ds2_renut_chain_reuse), only when it
      // differs from this device's (in this thread's cache).
      uint64_t changed[2] = {0, 0};
      for (u32 word = 0; word < 2; ++word) {
        for (uint64_t bits = h.chunks[word]; bits; bits &= bits - 1) {
          const u32 i = word * 64 + u32(std::countr_zero(bits));
          const u32 size = DeltaChunkSize(i);
          u8 *const to = g_delta_device + DeltaChunkOffset(i);
          if (size == kDeltaUnitBytes ? DeltaChunkDiffers(to, from) : std::memcmp(to, from, size) != 0) {
            std::memcpy(to, from, size);
            changed[word] |= uint64_t(1) << (i % 64);
          }
          from += size;
        }
      }
      g_chain_hint.changed[0] = changed[0];
      g_chain_hint.changed[1] = changed[1];
    } else {
      for (u32 word = 0; word < 2; ++word) {
        for (uint64_t bits = h.chunks[word]; bits; bits &= bits - 1) {
          const u32 i = word * 64 + u32(std::countr_zero(bits));
          const u32 size = DeltaChunkSize(i);
          // Whole chunks as a fixed-size copy, which stays inline.
          if (size == kDeltaUnitBytes)
            std::memcpy(g_delta_device + DeltaChunkOffset(i), from, kDeltaUnitBytes);
          else
            std::memcpy(g_delta_device + DeltaChunkOffset(i), from, size);
          from += size;
        }
      }
    }
    for (const auto [dirty, base] : {std::pair{h.vs, kDeltaVertexF}, std::pair{h.ps, kDeltaPixelF}}) {
      ForEachGroupRun(dirty, [&](u32 first, u32 count) {
        const u32 bytes = count * kDeltaUnitBytes;
        std::memcpy(g_delta_device + base + first * kDeltaUnitBytes, from, bytes);
        from += bytes;
      });
    }
  }
  if (!capture.verify.empty()) {
    g_delta_verified.fetch_add(1, std::memory_order_relaxed);
    // sa2-nx (ds2_renut_snapshot_deltas_narrow): the bytes the render thread reads; with the narrow compare off, all.
    bool same;
    if (DeltasNarrowOn()) {
      same = std::memcmp(g_delta_device + kDeltaUnreadEnd, capture.verify.data() + kDeltaUnreadEnd,
                         kDeltaPrivateUnreadBegin - kDeltaUnreadEnd) == 0 &&
             std::memcmp(g_delta_device + kDeltaPrivateUnreadEnd, capture.verify.data() + kDeltaPrivateUnreadEnd,
                         d3d::kDeviceSnapshotSize - kDeltaPrivateUnreadEnd) == 0;
    } else {
      same = std::memcmp(g_delta_device, capture.verify.data(), d3d::kDeviceSnapshotSize) == 0;
    }
    if (!same) {
      g_delta_differ.fetch_add(1, std::memory_order_relaxed);
      static uint32_t logged = 0;
      if (logged < 8) {
        ++logged;
        u32 at = 0;
        while (at < d3d::kDeviceSnapshotSize &&
               (g_delta_device[at] == capture.verify[at] || (DeltasNarrowOn() && !DeltaByteRead(at))))
          ++at;
        RGPU_WARN("Device snapshot delta: the rebuilt device differs from the device at +{} ({} draw): {:02X}, the "
                  "device {:02X} (ds2_renut_snapshot_deltas_verify)",
                  at, capture.snapshot == SnapshotKind::Rebase ? "a whole" : "a delta", g_delta_device[at],
                  capture.verify[at]);
      }
    }
  }
  return g_delta_device;
}

void QueueCapture(DrawCapture *capture) {
  // sa2-nx (2026-10-20, ds2_renut_prefetch_next): the capture and its device bytes, written on the game thread's core,
  // are fetched into the render thread's cache while it does the item before this one.
  render_thread::QueuePrefetched(capture, capture->state.data(), [capture] {
    DrawCall call = capture->call;
    if (!capture->vertices.empty())
      call.up_vertices = capture->vertices.data();
    if (!capture->indices.empty())
      call.up_indices = capture->indices.data();
    const u8 *device = DeviceForDraw(*capture);
    if (capture->snapshot != SnapshotKind::Whole && ChainConstantsOn()) {
      // sa2-nx (ds2_renut_snapshot_deltas_constants): every constant is the rebuilt device's (no running device).
      const ReplayConstants chain{nullptr, ~uint64_t(0), ~uint64_t(0), &g_chain_vs_dirty, &g_chain_ps_dirty,
                                  g_chain_id};
      DrawStateLocked(device, call, &chain);
    } else {
      DrawStateLocked(device, call);
    }
    g_chain_hint = {};  // sa2-nx: only for the draw itself
    g_capture_pool.Give(capture, true);
  });
}

DrawCapture *Capture(u32 device_va, const DrawCall &call) {
  auto *device = GuestPtr<d3d::Device>(device_va);
  if (!device)
    return nullptr;
  ClearPending(device);
  DrawCapture *capture = g_capture_pool.Take();
  // assign() keeps a pooled capture's storage (Snapshot() made a new vector each time).
  const auto *bytes = reinterpret_cast<const u8 *>(device);
  capture->state.assign(bytes, bytes + d3d::kDeviceSnapshotSize);
  capture->snapshot = SnapshotKind::Whole;  // sa2-nx
  capture->verify.clear();
  capture->call = call;
  capture->call.up_vertices = nullptr;
  capture->call.up_indices = nullptr;
  return capture;
}

// sa2-nx (2026-10-20, ds2_renut_snapshot_deltas): a draw of the chain. Requires g_delta.mutex, held until the capture
// is queued.
DrawCapture *CaptureDelta(u32 device_va, const DrawCall &call) {
  auto *device = GuestPtr<d3d::Device>(device_va);
  if (!device)
    return nullptr;
  DeltaChain &c = g_delta;
  DrawCapture *capture = g_capture_pool.Take(true);
  if (MarksCopyOn())
    g_capture_pool.PrefetchChainNext();  // sa2-nx (2026-10-28): the next draws' captures, for writing
  capture->call = call;
  capture->call.up_vertices = nullptr;
  capture->call.up_indices = nullptr;
  const auto *bytes = reinterpret_cast<const u8 *>(device);
  if (c.device != device) {
    // The chain's first draw (or a frame's): the whole device, which the render thread takes as its own.
    for (auto &mask : device->m_Pending)
      mask = 0;
    capture->state.assign(bytes, bytes + d3d::kDeviceSnapshotSize);
    std::memcpy(c.shadow, bytes, d3d::kDeviceSnapshotSize);
    c.device = device;
    c.vs = c.ps = 0;
    if (SetterMarksOn()) {  // sa2-nx (ds2_renut_setter_marks): marks from here on, for this device
      ds2::renut::g_setter_marks.device = device_va;
      ds2::renut::TakeMarks();
      // sa2-nx (2026-10-28, ds2_renut_setters_precise): the bits held apart go with the device's, and the precise
      // setters run from here on while no check has found a miss; the render thread's float constants are these.
      ds2::renut::DropApart();
      ds2::renut::g_setters_precise =
          SettersPreciseOn() && !c.marks_off && (c.const_misses == 0 || DeltasVerifyOn());
      if (ds2::renut::g_setters_precise) {
        std::memcpy(c.const_shadow, bytes + kDeltaVertexF, sizeof(c.const_shadow));
        ds2::renut::g_const_shadow = c.const_shadow;
      }
    }
    capture->snapshot = SnapshotKind::Rebase;
    ++c.rebases;
    c.bytes += d3d::kDeviceSnapshotSize;
  } else {
    uint64_t vs = c.vs | uint64_t(device->m_Pending[0]);
    uint64_t ps = c.ps | uint64_t(device->m_Pending[1]);
#if defined(SA2_D3D_CENSUS_BUILD)
    sa2::d3d::CensusDrawTags(uint64_t(device->m_Pending[2]), uint64_t(device->m_Pending[3]),
                             uint64_t(device->m_Pending[4]));
#endif
    // sa2-nx (2026-10-28, ds2_renut_setter_marks): the chunks to compare, from the marks and the dirty tags pending
    // (before they are cleared below).
    const bool marks = SetterMarksOn() && !c.marks_off;
    ds2::renut::ChunkBits marked{};
    if (marks) {
      marked = ds2::renut::TakeMarks() |
               ds2::renut::TagChunks(uint64_t(device->m_Pending[2]), uint64_t(device->m_Pending[3]),
                                     uint64_t(device->m_Pending[4])) |
               kMarksAlways;
    }
    // sa2-nx (2026-10-28, ds2_renut_setters_precise): the bits held apart need nothing (the precise setters marked
    // the bytes they changed; their constant groups did not change) and are consumed with the device's.
    const bool precise = ds2::renut::g_setters_precise;
    ds2::renut::DropApart();
    c.vs = c.ps = 0;
    for (auto &mask : device->m_Pending)
      mask = 0;
    std::vector<u8> &out = capture->state;
    if (out.size() < kDeltaMaxBytes)
      out.resize(kDeltaMaxBytes);  // once per pooled capture: written below, never cleared
    u8 *const begin = out.data();
    u8 *to = begin + sizeof(DeltaHeader);
    DeltaHeader h{};
    u32 chunks = 0;
    const bool narrow = DeltasNarrowOn();
    const bool neon = NeonCompareOn();  // sa2-nx (ds2_renut_neon_compare)
    // Chunk i: compared with the shadow, and kept (in the shadow and the delta) when it differs.
    const auto compare = [&](u32 i) {
      const u32 at = DeltaChunkOffset(i), size = DeltaChunkSize(i);
      const bool differs = size == kDeltaUnitBytes ? ds2::renut::Line64Differs(bytes + at, c.shadow + at, neon)
                                                   : std::memcmp(bytes + at, c.shadow + at, size) != 0;
      if (!differs)
        return false;
      std::memcpy(c.shadow + at, bytes + at, size);
      std::memcpy(to, bytes + at, size);
      to += size;
      h.chunks[i / 64] |= uint64_t(1) << (i % 64);
      ++chunks;
#if defined(SA2_D3D_CENSUS_BUILD)
      sa2::d3d::CensusDrawChunk(at, size);
#endif
      return true;
    };
    // sa2-nx (2026-10-28): the verify mode checks every other draw, so that the draws between take the marks' path
    // (ds2_renut_marks_copy, the precise setters) and the render thread's compare of every rebuilt device covers it.
    const bool check = marks && SetterMarksCheckOn() &&
                       (DeltasVerifyOn() ? (++c.mark_draws & 1) == 0 : (++c.mark_draws & 31) == 0);
    if (marks && !check) {
      // The marked chunks, in order (the render thread reads the delta's chunks in index order). Their lines are all
      // known first: prefetched together, the device's and the shadow's L2 misses overlap instead of each waiting on
      // the compare before it (the full compare's stall was on those loads, 47 % of CaptureDelta on the console).
      const ds2::renut::ChunkBits &mask = narrow ? kDeltaReadMask : kDeltaAllMask;
      const uint64_t todo[2] = {marked.w[0] & mask.w[0], marked.w[1] & mask.w[1]};
      const u32 marked_count = u32(std::popcount(todo[0]) + std::popcount(todo[1]));
      if (MarksCopyOn()) {
        // sa2-nx (2026-10-28, ds2_renut_marks_copy): the marked chunks as they are, into the delta and the shadow
        // (which the checks compare with), without loading the shadow's first: the render thread finds which of them
        // changed (DeviceForDraw). A marked chunk that did not change costs it a compare of a line in its cache.
        for (u32 word = 0; word < 2; ++word) {
          for (uint64_t bits = todo[word]; bits; bits &= bits - 1) {
            const u32 at = DeltaChunkOffset(word * 64 + u32(std::countr_zero(bits)));
            __builtin_prefetch(bytes + at);
            __builtin_prefetch(c.shadow + at, 1);
          }
        }
        for (u32 word = 0; word < 2; ++word) {
          for (uint64_t bits = todo[word]; bits; bits &= bits - 1) {
            const u32 i = word * 64 + u32(std::countr_zero(bits));
            const u32 at = DeltaChunkOffset(i), size = DeltaChunkSize(i);
            if (size == kDeltaUnitBytes) {
              std::memcpy(c.shadow + at, bytes + at, kDeltaUnitBytes);
              std::memcpy(to, bytes + at, kDeltaUnitBytes);
            } else {
              std::memcpy(c.shadow + at, bytes + at, size);
              std::memcpy(to, bytes + at, size);
            }
            to += size;
          }
        }
        h.chunks[0] = todo[0];
        h.chunks[1] = todo[1];
        chunks = marked_count;
        c.copied += marked_count;
      } else {
        for (u32 word = 0; word < 2; ++word) {
          for (uint64_t bits = todo[word]; bits; bits &= bits - 1) {
            const u32 at = DeltaChunkOffset(word * 64 + u32(std::countr_zero(bits)));
            __builtin_prefetch(bytes + at);
            __builtin_prefetch(c.shadow + at);
          }
        }
        for (u32 word = 0; word < 2; ++word)
          for (uint64_t bits = todo[word]; bits; bits &= bits - 1)
            compare(word * 64 + u32(std::countr_zero(bits)));
        c.compared += marked_count;
      }
    } else {
      const DeltaChunkList &list = narrow ? kDeltaReadChunks : kDeltaAllChunks;
      for (u32 k = 0; k < list.count; ++k) {
        const u32 i = list.index[k];
        if (compare(i) && check && !(marked.w[i / 64] >> (i % 64) & 1)) {
          // A change no mark named: the marks are off from here on (this draw keeps it, as every draw will). The
          // verify mode, which checks every draw and so never uses them, keeps counting instead.
          ++c.misses;
          c.marks_off = !DeltasVerifyOn();
          if (c.marks_off)
            ds2::renut::g_setters_precise = false;  // sa2-nx (ds2_renut_setters_precise): with the marks
          if (c.misses <= 32)
            RGPU_WARN("Setter marks: device chunk +{} (0x{:X}) changed with no mark{} (ds2_renut_setter_marks)",
                      DeltaChunkOffset(i), DeltaChunkOffset(i), c.marks_off ? "; the marks are off" : "");
        }
      }
      c.compared += list.count;
      c.checks += check;
      // sa2-nx (2026-10-28, ds2_renut_setters_precise): the check compares float constant groups no dirty bit names
      // with the render thread's (the shadow): 16 of the 128 at each check, in turn (all 8 KB at every 32nd draw was
      // about 0.1 us a draw on the console), all of them in the verify mode. One that differs is taken by this draw,
      // and the precise setters go off for the session (logged), as a chunk's miss turns the marks off.
      if (check && precise) {
        const u32 first = DeltasVerifyOn() ? 0 : c.const_check_next;
        const u32 count = DeltasVerifyOn() ? 128 : 16;
        c.const_check_next = (c.const_check_next + 16) & 127;
        for (u32 k = 0; k < count; ++k) {
          const u32 side = ((first + k) & 127) >> 6, g = (first + k) & 63;
          uint64_t &dirty = side ? ps : vs;
          const uint64_t bit = uint64_t(1) << (63 - g);
          const u32 at = (side ? kDeltaPixelF : kDeltaVertexF) + g * kDeltaUnitBytes;
          if ((dirty & bit) || !DeltaChunkDiffers(bytes + at, c.const_shadow + (at - kDeltaVertexF)))
            continue;
          dirty |= bit;
          ++c.const_misses;
          ds2::renut::g_setters_precise = DeltasVerifyOn();  // the verify mode keeps counting
          if (c.const_misses <= 32)
            RGPU_WARN("Setter marks: float constant group {} of the {} shader changed with no dirty bit{} "
                      "(ds2_renut_setters_precise)",
                      g, side ? "pixel" : "vertex", DeltasVerifyOn() ? "" : "; the precise setters are off");
        }
      }
    }
    for (const auto [dirty, base] : {std::pair{vs, kDeltaVertexF}, std::pair{ps, kDeltaPixelF}}) {
      ForEachGroupRun(dirty, [&](u32 first, u32 count) {
        const u32 group_bytes = count * kDeltaUnitBytes;
        std::memcpy(to, bytes + base + first * kDeltaUnitBytes, group_bytes);
        to += group_bytes;
        // sa2-nx (ds2_renut_setters_precise): the render thread's constants, for the checks.
        if (precise)
          std::memcpy(c.const_shadow + (base - kDeltaVertexF) + first * kDeltaUnitBytes,
                      bytes + base + first * kDeltaUnitBytes, group_bytes);
      });
    }
    h.vs = vs;
    h.ps = ps;
    std::memcpy(begin, &h, sizeof(h));
    capture->snapshot = SnapshotKind::Delta;
    ++c.deltas;
    c.bytes += uint64_t(to - begin);
    c.chunks += chunks;
    c.groups += uint64_t(std::popcount(vs) + std::popcount(ps));
  }
  if (DeltasVerifyOn())
    capture->verify.assign(bytes, bytes + d3d::kDeviceSnapshotSize);
  else
    capture->verify.clear();
#if defined(SA2_D3D_CENSUS_BUILD)
  sa2::d3d::CensusDrawDone(device_va);
#endif
  return capture;
}

} // namespace

void SnapshotDeltasNewFrame() {
  if (!DeltasOn())
    return;
  std::lock_guard lock(g_delta.mutex);
  g_delta.device = nullptr;  // the frame's first draw rebases: whatever went wrong lasts a frame at most
}

void ForgetSnapshotDeltas() {
  if (!DeltasOn())
    return;
  std::lock_guard lock(g_delta.mutex);
  g_delta.device = nullptr;
}

std::string SnapshotDeltaReport() {
  if (!DeltasOn())
    return {};
  uint64_t deltas, rebases, bytes, chunks, groups, whole, compared, checks, misses, copied, const_misses;
  bool marks_off, precise;
  {
    std::lock_guard lock(g_delta.mutex);
    whole = g_delta.whole;
    g_delta.whole = 0;
    deltas = g_delta.deltas;
    rebases = g_delta.rebases;
    bytes = g_delta.bytes;
    chunks = g_delta.chunks;
    groups = g_delta.groups;
    compared = g_delta.compared;
    checks = g_delta.checks;
    misses = g_delta.misses;  // the session's
    marks_off = g_delta.marks_off;
    copied = g_delta.copied;              // sa2-nx (ds2_renut_marks_copy)
    const_misses = g_delta.const_misses;  // sa2-nx (ds2_renut_setters_precise): the session's
    precise = ds2::renut::g_setters_precise;
    g_delta.deltas = g_delta.rebases = g_delta.bytes = g_delta.chunks = g_delta.groups = 0;
    g_delta.compared = g_delta.checks = g_delta.copied = 0;
  }
  const uint64_t draws = deltas + rebases;
  if (!draws)
    return {};
  const double per_delta = double(std::max<uint64_t>(deltas, 1));
  std::string line = fmt::format(
      "device snapshots (ds2_renut_snapshot_deltas): {} draws, {:.0f} bytes kept a draw on average (the whole device "
      "is {}); {:.1f} chunks and {:.1f} constant groups a delta; {} whole copies (the frames' first draws); {} "
      "Begin*Vertices draws as whole copies",
      draws, double(bytes) / double(draws), d3d::kDeviceSnapshotSize, double(chunks) / per_delta,
      double(groups) / per_delta, rebases, whole);
  if (SetterMarksOn()) {  // sa2-nx (2026-10-28)
    line += fmt::format("; setter marks (ds2_renut_setter_marks): {:.1f} chunks compared and {:.1f} copied as "
                        "marked (ds2_renut_marks_copy) a delta, {} checks, {} unmarked changes this session{}",
                        double(compared) / per_delta, double(copied) / per_delta, checks, misses,
                        marks_off ? " (the marks are off)" : "");
    if (SettersPreciseOn())
      line += fmt::format("; precise setters (ds2_renut_setters_precise) {}, {} float constant groups changed "
                          "without a dirty bit this session",
                          precise ? "on" : "off", const_misses);
  }
  if (DeltasVerifyOn()) {
    line += fmt::format("; verify: {} draws checked, {} differ", g_delta_verified.exchange(0, std::memory_order_relaxed),
                        g_delta_differ.exchange(0, std::memory_order_relaxed));
  }
  return line;
}

void QueueDraw(u32 device_va, const DrawCall &call, u32 up_vertex_bytes, u32 up_index_bytes) {
  FlushDeferredVertices(device_va);
  RunReadyCallbacks();
  // sa2-nx (2026-10-20, ds2_renut_snapshot_deltas): made and queued under the chain's lock.
  ChainGuard chain;
  if (DeltasOn())
    chain = ChainGuard(g_delta.mutex);
  DrawCapture *capture = DeltasOn() ? CaptureDelta(device_va, call) : Capture(device_va, call);
  if (!capture)
    return;
  if (call.up_vertices)
    capture->vertices.assign(call.up_vertices, call.up_vertices + up_vertex_bytes);
  if (call.up_indices)
    capture->indices.assign(call.up_indices, call.up_indices + up_index_bytes);
  QueueCapture(capture);
}

void QueueDeferredDraw(u32 device_va, const DrawCall &call, const u8 *scratch, u32 bytes,
                       const u8 *index_scratch, u32 index_bytes) {
  FlushDeferredVertices(device_va);
  RunReadyCallbacks();
  // sa2-nx (2026-10-20, ds2_renut_deferred_chain, from futari-nx via peggle2-nx): a Begin*Vertices draw of the chain
  // too. Made under the chain's lock and queued by FlushDeferredVertices under it as well, before any other draw of the
  // device is made (every hook flushes its device's deferred draw first), so the render thread still applies the chain
  // in order. A draw that will not be queued (no scratch) stays a whole capture outside the chain, as before.
  if (DeltasOn() && DeferredChainOn() && scratch) {
    ChainGuard chain(g_delta.mutex);
    {
      std::lock_guard lock(g_deferred_mutex);
      auto it = g_deferred.find(device_va);
      if (it != g_deferred.end()) {
        // One not flushed (FlushDeferredVertices above queues it, so none is expected): dropped as the original
        // replaced it; its delta never reaches the render thread, so the chain rebases.
        g_capture_pool.Give(it->second.capture);
        g_deferred.erase(it);
        g_deferred_count.store(g_deferred.size(), std::memory_order_release);
        g_delta.device = nullptr;
      }
    }
    DrawCapture *delta = CaptureDelta(device_va, call);
    if (!delta)
      return;
    std::lock_guard lock(g_deferred_mutex);
    g_deferred[device_va] = {delta, scratch, bytes, index_scratch, index_bytes};
    g_deferred_count.store(g_deferred.size(), std::memory_order_release);
    return;
  }
  DrawCapture *capture = nullptr;
  {
    // sa2-nx (ds2_renut_snapshot_deltas): a whole capture (queued later than made: outside the chain), whose dirty
    // constant groups ClearPending hands to the chain's next delta.
    ChainGuard chain;
    if (DeltasOn())
      chain = ChainGuard(g_delta.mutex);
    capture = Capture(device_va, call);
    if (DeltasOn())
      ++g_delta.whole;
  }
  if (!capture)
    return;
  if (!scratch) {
    g_capture_pool.Give(capture);
    return;
  }
  std::lock_guard lock(g_deferred_mutex);
  DeferredDraw &slot = g_deferred[device_va];
  g_capture_pool.Give(slot.capture);  // one not flushed (the original replaced it the same way)
  slot = {capture, scratch, bytes, index_scratch, index_bytes};
  g_deferred_count.store(g_deferred.size(), std::memory_order_release);
}

void FlushDeferredVertices(u32 device_va) {
  FlushRecordedVertices(device_va);
  if (HookFastPathsOn() && g_deferred_count.load(std::memory_order_acquire) == 0)
    return;  // sa2-nx (2026-10-20): none waiting, no lock
  // sa2-nx (2026-10-20, ds2_renut_deferred_chain): a chain draw is queued under the chain's lock (taken before the
  // deferred map's, as QueueDeferredDraw takes them), so no draw of the chain is made or queued between.
  ChainGuard chain;
  if (DeltasOn() && DeferredChainOn())
    chain = ChainGuard(g_delta.mutex);
  DeferredDraw deferred;
  {
    std::lock_guard lock(g_deferred_mutex);
    auto it = g_deferred.find(device_va);
    if (it == g_deferred.end())
      return;
    deferred = std::move(it->second);
    g_deferred.erase(it);
    g_deferred_count.store(g_deferred.size(), std::memory_order_release);
  }
  deferred.capture->vertices.assign(deferred.scratch, deferred.scratch + deferred.bytes);
  if (deferred.index_scratch)
    deferred.capture->indices.assign(deferred.index_scratch, deferred.index_scratch + deferred.index_bytes);
  QueueCapture(deferred.capture);
}

void QueueClear(u32 device_va, u32 flags, const ClearRect *rect, const float color[4], float depth,
                u32 stencil) {
  FlushDeferredVertices(device_va);
  RunReadyCallbacks();
  const auto *device = GuestPtr<const u8>(device_va);
  if (!device)
    return;
  DrawCapture *args = g_capture_pool.Take();  // banjo-nx (2026-10-19): pooled, see DrawCapture
  args->state.assign(device, device + d3d::kDeviceSnapshotSize);
  args->has_rect = rect != nullptr;
  args->rect = rect ? *rect : ClearRect{};
  std::memcpy(args->color, color, sizeof(args->color));
  args->flags = flags;
  args->depth = depth;
  args->stencil = stencil;
  render_thread::Queue([args] {
    ClearStateLocked(args->state.data(), args->flags, args->has_rect ? &args->rect : nullptr, args->color,
                     args->depth, args->stencil);
    g_capture_pool.Give(args, true);
  });
}

void QueueResolve(u32 device_va, u32 flags, const ClearRect *source_rect, u32 dest_texture_va,
                  i32 dest_x, i32 dest_y, u32 dest_level, u32 dest_slice,
                  const float clear_color[4], float clear_depth, u32 clear_stencil) {
  FlushDeferredVertices(device_va);
  RunReadyCallbacks();
  const auto *device = GuestPtr<const u8>(device_va);
  if (!device)
    return;
  // The destination header as it is now: the game often builds it on the
  // stack (as for recorded resolves).
  DrawCapture *args = g_capture_pool.Take();  // banjo-nx (2026-10-19): pooled, see DrawCapture
  args->state.assign(device, device + d3d::kDeviceSnapshotSize);
  args->has_rect = source_rect != nullptr;
  args->rect = source_rect ? *source_rect : ClearRect{};
  args->has_color = clear_color != nullptr;
  if (clear_color)
    std::memcpy(args->color, clear_color, sizeof(args->color));
  const auto *header = GuestPtr<const d3d::BaseTexture>(dest_texture_va);
  args->has_dest_format = header != nullptr;
  if (header)
    args->dest_format = header->Format;
  args->flags = flags;
  args->dest_texture_va = dest_texture_va;
  args->dest_x = dest_x;
  args->dest_y = dest_y;
  args->dest_level = dest_level;
  args->dest_slice = dest_slice;
  args->depth = clear_depth;
  args->stencil = clear_stencil;
  render_thread::Queue([args] {
    ResolveStateLocked(args->state.data(), args->flags, args->has_rect ? &args->rect : nullptr,
                       args->dest_texture_va, args->has_dest_format ? &args->dest_format : nullptr,
                       args->dest_x, args->dest_y, args->dest_level, args->dest_slice,
                       args->has_color ? args->color : nullptr, args->depth, args->stencil);
    g_capture_pool.Give(args, true);
  });
}

} // namespace renut::gpu
