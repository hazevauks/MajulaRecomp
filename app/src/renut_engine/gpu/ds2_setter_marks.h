// sa2-nx (2026-10-28, ds2_renut_setter_marks): the device chunks SA2's Direct3D setters may have written since the
// delta chain's last draw, so that the chain's next draw compares those (backend/queued.cpp CaptureDelta) instead of all
// 40 it reads (the compare was 74 % of CaptureDelta and 10 % of the game thread in the two-player console profile).
//
// A chunk can change three ways between two draws, and each leaves a mark:
//   - a setter hooked for it (me/ds2_setter_marks.h, the Direct3D functions that store pointers and other fields
//     without a dirty tag: stream sources, textures, shaders, viewport, scissor, render targets, depth) marks the chunks
//     its arguments say it wrote;
//   - any write under a dirty tag (m_Pending words 2-4: the out-of-line setters' and the game's own inline ones, or the
//     GPU would never get the state) leaves the tag pending until the draw, which takes the tag's chunks
//     (ds2_setter_marks_tags.h); the setters that consume tags before a draw (the clear) mark the chunks of the tags
//     they found;
//   - the vertex declaration's chunk (0x2FC0), which the game sets inline without a tag, is compared at every draw.
// The census build (ds2_diag_d3d_census, me/sa2_d3d_api.cpp) found no other writer of a compared chunk on the draw
// thread; every 32nd draw still compares all 40 (ds2_renut_setter_marks_check), and the first change it finds that no
// mark had named turns the marks off for the session (the full compare, as before).
//
// The marks are the draw thread's: the hooks mark only for the chain's device (another thread's device, or a call
// with another first argument, marks nothing), as banjo-nx's touch slots do, with plain loads and stores (one ldp, two
// orr, one stp on the A57; atomics would keep the two words apart). The only other writer is a whole mark from a rare
// setter on another thread, a store of all ones, which can lose none of the draw thread's bits.
#pragma once

#include <cstdint>

#include "renut_engine/gpu/ds2_setter_marks_tags.h"

namespace ds2::renut {

// The delta chain's chunks (queued.cpp, which asserts the same): the device's first 0x780 bytes in 30 chunks of 64,
// then the tail from 0x2780 to the snapshot's end; chunk i is bit i % 64 of word i / 64.
constexpr uint32_t kMarkHeadEnd = 0x780, kMarkTailBegin = 0x2780, kMarkSnapshotEnd = 13088;
constexpr uint32_t kMarkHeadChunks = kMarkHeadEnd / 64;

struct ChunkBits {
  uint64_t w[2] = {0, 0};
};
// The chunks of device bytes [offset, offset + size) (none for the float constants between the head and the tail).
constexpr ChunkBits MarkBits(uint32_t offset, uint32_t size) {
  ChunkBits b{};
  for (uint32_t at = offset & ~63u; at < offset + size; at += 64) {
    uint32_t i;
    if (at < kMarkHeadEnd)
      i = at / 64;
    else if (at >= kMarkTailBegin && at < kMarkSnapshotEnd)
      i = kMarkHeadChunks + (at - kMarkTailBegin) / 64;
    else
      continue;
    b.w[i / 64] |= uint64_t(1) << (i % 64);
  }
  return b;
}
constexpr ChunkBits operator|(ChunkBits a, ChunkBits b) { return {{a.w[0] | b.w[0], a.w[1] | b.w[1]}}; }

// One cache line: the device and both words, reached from one adrp.
struct alignas(64) SetterMarks {
  uint64_t chunks[2] = {0, 0};
  uint32_t device = 0;  // the chain's device (its guest address); 0: none, nothing is marked
  // sa2-nx (2026-10-28, ds2_renut_setters_precise): the bits of m_Pending words 0-4 (native values) the precise
  // setters (me/ds2_setter_marks.h) added and hold apart from the device until a function that reads them runs, and
  // whether any word has one.
  uint32_t apart_any = 0;
  uint64_t apart[5] = {0, 0, 0, 0, 0};
};
static_assert(sizeof(SetterMarks) == 64);
inline SetterMarks g_setter_marks;
// sa2-nx (2026-10-28, ds2_renut_setters_precise): banjo-nx's setter rewrite (banjo_renut_touch_compare and
// banjo_renut_tags_apart) for SA2's hot setters, set by the chain (backend/queued.cpp) while its marks are on. The
// setters it names mark only the chunks whose bytes they changed, and keep the dirty bits they add out of the device:
// a bit in the device makes the draw take every chunk the tag may stand for, or a float constant group that did not
// change. The bits go back into the device before any function that reads m_Pending runs, and are dropped at a draw,
// whose capture consumes the device's (as the original draw's packets did).
inline bool g_setters_precise = false;
// The guest thread (its context) whose precise setters hold the bits apart: only that thread gives them back to the
// device (the Direct3D heap functions, for one, also run on loading threads, which must not touch the draw thread's
// words), as only it adds them.
inline const void *g_apart_owner = nullptr;
// The float constants the render thread's device has (device bytes 0x780-0x277F: the vertex shader's 256, then the
// pixel shader's), the chain's copy on the game thread (backend/queued.cpp; set while the precise setters are on). A
// constant upload compares its source with these (in this core's cache), not with the device's lines it is about to
// store (a load miss for each before the copy: the console's second run had the uploads at four times their cost).
inline const uint8_t *g_const_shadow = nullptr;
// The delta chain's chunk of device byte 'offset' (0x780-0x277F, the float constants, have none: ~0u).
constexpr uint32_t ChunkOf(uint32_t offset) {
  return offset < kMarkHeadEnd ? offset / 64
                               : (offset >= kMarkTailBegin && offset < kMarkSnapshotEnd
                                      ? kMarkHeadChunks + (offset - kMarkTailBegin) / 64
                                      : ~0u);
}
// Marks the one chunk of device byte 'offset' (a field within a chunk) for the chain's device.
inline void MarkChunkOf(uint32_t device, uint32_t offset) {
  const uint32_t i = ChunkOf(offset);
  if (device != g_setter_marks.device || i == ~0u) return;
  g_setter_marks.chunks[i >> 6] |= uint64_t(1) << (i & 63);
}
// The bits held apart, dropped (the draw thread, where a capture clears m_Pending).
inline void DropApart() {
  if (g_setter_marks.apart_any) {
    for (uint64_t &w : g_setter_marks.apart)
      w = 0;
    g_setter_marks.apart_any = 0;
  }
}

inline uint32_t MarkedDevice() { return g_setter_marks.device; }
// Marks from a table (both words, unconditionally: the stream and texture entries have bits in both).
inline void Mark(uint32_t device, ChunkBits bits) {
  if (device != g_setter_marks.device) return;
  g_setter_marks.chunks[0] |= bits.w[0];
  g_setter_marks.chunks[1] |= bits.w[1];
}
// Fixed marks: only the words with bits are touched.
template <uint64_t W0, uint64_t W1>
inline void MarkFixed(uint32_t device) {
  if (device != g_setter_marks.device) return;
  if constexpr (W0 != 0) g_setter_marks.chunks[0] |= W0;
  if constexpr (W1 != 0) g_setter_marks.chunks[1] |= W1;
}
// Every chunk: the setters whose writes are not followed.
inline void MarkAll() {
  g_setter_marks.chunks[0] = ~uint64_t(0);
  g_setter_marks.chunks[1] = ~uint64_t(0);
}
// The marks so far, cleared (the draw thread, at a draw).
inline ChunkBits TakeMarks() {
  const ChunkBits b{{g_setter_marks.chunks[0], g_setter_marks.chunks[1]}};
  g_setter_marks.chunks[0] = g_setter_marks.chunks[1] = 0;
  return b;
}

// The chunks of the dirty tags in m_Pending words 2-4 (native values).
inline ChunkBits TagChunks(uint64_t states, uint64_t fetch, uint64_t misc) {
  ChunkBits b{};
  const uint64_t words[3] = {states, fetch, misc};
  for (uint32_t w = 0; w < 3; ++w) {
    for (uint64_t bits = words[w]; bits; bits &= bits - 1) {
      const uint64_t(&t)[2] = kTagChunks[w * 64 + uint32_t(__builtin_ctzll(bits))];
      b.w[0] |= t[0];
      b.w[1] |= t[1];
    }
  }
  return b;
}

}  // namespace ds2::renut
