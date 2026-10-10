#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <span>

namespace me::native {
enum class ShaderIdentityStage : uint8_t { Vertex, Pixel };
struct ShaderIdentityView {
  ShaderIdentityStage stage;
  std::span<const uint32_t> words;  // host-order sequencer words, not container bytes
};

// Full containers with different DEF/ABI metadata may share these words. Keep
// their identity when compatible; this only rejects a different GPU program.
inline bool PixelShaderIdentityMatches(ShaderIdentityView loaded, ShaderIdentityView selected) {
  return loaded.stage == ShaderIdentityStage::Pixel &&
         selected.stage == ShaderIdentityStage::Pixel && !loaded.words.empty() &&
         loaded.words.size() == selected.words.size() &&
         std::equal(loaded.words.begin(), loaded.words.end(), selected.words.begin());
}

// Exactly the mappings RemapCode + remapInput + the original HLSL fetch
// can express. Original constants are emitted after the fetch, and preserved
// destinations are not assigned by that HLSL at all. Reserved selector 6 has
// no supported meaning. Repeated input selectors share one remap component.
inline bool VertexFetchSwizzleRepresentable(uint32_t original, uint32_t patched) {
  uint32_t replacements[4]{7, 7, 7, 7};
  for (uint32_t destination = 0; destination < 4; ++destination) {
    const uint32_t before = (original >> (destination * 3)) & 7;
    const uint32_t after = (patched >> (destination * 3)) & 7;
    if (before == 6 || after == 6 || (before == 7) != (after == 7)) return false;
    if (before < 4) {
      if (after > 5 || (replacements[before] != 7 && replacements[before] != after)) return false;
      replacements[before] = after;
    } else if (before != after) return false;
  }
  return true;
}

// Experimental tolerance (cvar ds2_native_vs_identity_tolerance, default 0 = exact, as before): how many
// words OUTSIDE the declared fetch triples (ALU/CF words) a loaded vertex shader may differ from the library
// candidate of the same size and still be accepted. Without it such a draw is dropped (no VS identified).
inline std::atomic<uint32_t> g_vs_identity_alu_tolerance{0};

// ds2-nx: Direct3D's interpolator relink. When the bound pixel shader does not read an interpolator, the XDK 21256
// Direct3D replaces the vertex shader's ALU instruction that exports it by a no-op (C8000000 00000000 02000000: no
// destination, no write mask) and loads the patched copy as immediate microcode. Seen on Dark Souls II's model shaders
// (library C8038001 00B0C600 A100FF00 -> loaded C8000000 00000000 02000000 at instructions 39 and 40 of a 126-word
// shader). The unpatched library shader draws the same image: it only writes outputs nobody reads. Without this the
// draw had no vertex shader ("VS identity mismatch ... reason=non-fetch-word").
// ds2_native_vs_removed_exports (default true) turns the exception off.
inline std::atomic<bool> g_vs_identity_removed_exports{true};
// ds2_gpu_vs_fetch_reorder_streams: see me_vertex_fetch_reorder.h (EquivalentFullBases).
inline std::atomic<bool> g_reorder_across_streams{true};

// True when the control flow of `w` executes instruction `instruction` as an ALU instruction (in an EXEC clause, its
// sequence bit says "not a fetch"). False for anything else: a fetch, a control-flow word, an instruction no clause
// runs.
inline bool ExecutedAsAlu(std::span<const uint32_t> w, size_t instruction) {
  size_t end = w.size() / 3;
  for (size_t pair = 0; pair < end; ++pair) {
    const uint64_t cf[2]{uint64_t(w[pair * 3]) | uint64_t(w[pair * 3 + 1] & 65535) << 32,
                         uint64_t(w[pair * 3 + 1] >> 16) | uint64_t(w[pair * 3 + 2]) << 16};
    for (unsigned half = 0; half < 2; ++half) {
      const uint64_t c = cf[half];
      const unsigned op = (c >> 44) & 15;
      if (!((op >= 1 && op <= 6) || op == 13 || op == 14)) continue;  // the EXEC family
      const size_t first = c & 4095, count = (c >> 12) & 7;
      const unsigned sequence = (c >> 16) & 4095;
      if (first) end = std::min(end, first);
      if (instruction >= first && instruction < first + count)
        return !((sequence >> ((instruction - first) * 2)) & 1);
    }
  }
  return false;
}

// The word at `word` differs between the loaded program and the library's, outside the declared fetches: true when
// the loaded instruction is Direct3D's no-op and the library's is an ALU instruction that only exports interpolators
// (export registers 0..15; never the position, 62, or the point size / kill register, 63).
inline bool RemovedInterpolatorExport(std::span<const uint32_t> loaded, std::span<const uint32_t> selected,
                                      size_t word) {
  const size_t instruction = word / 3, start = instruction * 3;
  if (start + 3 > loaded.size() || start + 3 > selected.size()) return false;
  if (loaded[start] != 0xC8000000u || loaded[start + 1] != 0u || loaded[start + 2] != 0x02000000u) return false;
  const uint32_t alu = selected[start];
  const bool export_data = (alu >> 15) & 1;
  const uint32_t vector_destination = alu & 0x3F, scalar_destination = (alu >> 8) & 0x3F;
  const uint32_t scalar_write_mask = (alu >> 20) & 0xF;
  if (!export_data || vector_destination >= 16 || (scalar_write_mask && scalar_destination >= 16)) return false;
  return ExecutedAsAlu(selected, instruction);
}

// selected.words must be the library's normalized VS words. Only the declared
// fetch triples may differ in the SAME loader-masked fields or in a swizzle
// proven representable by the existing typed input ABI above.
// No fetch permutation or ALU/CF exception is allowed. Projection returns a
// three-word instruction index (not a word offset). Avoids allocation per draw.
template <class FetchRange, class Projection>
inline bool VertexShaderIdentityMatches(ShaderIdentityView loaded, ShaderIdentityView selected,
                                        const FetchRange& fetches, Projection instruction_index) {
  if (loaded.stage != ShaderIdentityStage::Vertex || selected.stage != ShaderIdentityStage::Vertex ||
      loaded.words.empty() || loaded.words.size() != selected.words.size()) return false;
  constexpr uint32_t masks[3]{0x0007FFFFu, 0x80000FFFu, 0x80000000u};
  for (const auto& fetch : fetches) {
    const uint64_t instruction = uint64_t(instruction_index(fetch));
    if (instruction > UINT32_MAX || instruction * 3 + 3 > selected.words.size()) return false;
    const size_t start = size_t(instruction) * 3;
    if ((selected.words[start] & 0x1Fu) != 0u) return false;  // Not a vertex fetch.
    for (size_t lane = 0; lane < 3; ++lane)
      if ((selected.words[start + lane] & masks[lane]) != selected.words[start + lane]) return false;
    if (!VertexFetchSwizzleRepresentable(selected.words[start + 1] & 0xFFFu,
                                         loaded.words[start + 1] & 0xFFFu)) return false;
  }
  uint32_t tolerance_left = g_vs_identity_alu_tolerance.load(std::memory_order_relaxed);
  for (size_t word = 0; word < loaded.words.size(); ++word) {
    if (loaded.words[word] == selected.words[word]) continue;
    bool declared_fetch = false;
    for (const auto& fetch : fetches) {
      const size_t start = size_t(instruction_index(fetch)) * 3;
      if (word >= start && word - start < 3) {
        declared_fetch = true;
        const uint32_t mask = word - start == 1 ? 0x80000000u : masks[word - start];
        if ((loaded.words[word] & mask) != (selected.words[word] & mask)) return false;
        break;
      }
    }
    if (!declared_fetch) {
      if (g_vs_identity_removed_exports.load(std::memory_order_relaxed) &&
          RemovedInterpolatorExport(loaded.words, selected.words, word))
        continue;
      if (tolerance_left == 0) return false;
      --tolerance_left;
    }
  }
  return true;
}
}  // namespace me::native
