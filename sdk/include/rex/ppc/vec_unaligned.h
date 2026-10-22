/**
 * @file        rex/ppc/vec_unaligned.h
 * @brief       stvlx / stvrx without byte loops (from sasrt-nx, 2026-10-14; idea 5 of sa2-nx's list; banjo-nx's copy)
 *
 * The generator writes the left and right unaligned vector stores as loops of single-byte stores:
 *   stvlx vS,EA:  for (i = 0; i < 16 - (EA & 15); i++) MEM[EA + i]     = vS.u8[15 - i]   (guest bytes 0.. of vS)
 *   stvrx vS,EA:  for (i = 0; i < (EA & 15); i++)      MEM[EA - i - 1] = vS.u8[i]        (its last EA & 15 bytes)
 * (a register's guest byte j is u8[15 - j]). tools/vec_unaligned_stores.py writes them as REX_STVLX / REX_STVRX, which
 * reverse the vector once and store the same bytes with at most five stores of 16, 8, 4, 2 and 1 bytes.
 *
 * Exact: every byte the loop writes gets the same value, and no other byte is written.
 *   left:  b[j] = u8[15 - j], and MEM[EA + j] = b[j] for j < 16 - (EA & 15), the loop's bytes;
 *   right: with k = EA & 15 and block = EA & ~15, MEM[block + m] = b[16 - k + m] = u8[k - 1 - m] for m < k, which is
 *          the loop's MEM[EA - i - 1] = u8[i] with m = k - 1 - i.
 * The range stays inside EA's 16-byte block, which never straddles 0xE0000000 (the host offset of that view), so one
 * host address serves the whole range. Only the order of the byte writes differs, which another thread reading the
 * block in the middle of the store could see (these are ordinary data stores, never hardware registers).
 *
 * Tested by sdk/tests/vec_unaligned_test.cpp (every alignment against the loops).
 */
#pragma once

#include <cstdint>

namespace rex::ppc {

/// The guest byte order of a vector register's 16 bytes (u8[15 - j] is guest byte j). The register is copied to a
/// local first, so the compiler knows it cannot overlap `b` (otherwise GCC adds an overlap test and a byte path).
[[gnu::always_inline]] inline void GuestBytes(const uint8_t* u8, uint8_t* b) noexcept {
  uint8_t in[16];
  __builtin_memcpy(in, u8, 16);
  for (int j = 0; j < 16; ++j) b[j] = in[15 - j];
}

/// 1 <= n <= 16 bytes from s to d, at most five stores.
[[gnu::always_inline]] inline void StoreBytes(uint8_t* d, const uint8_t* s, uint32_t n) noexcept {
  if (n == 16) {
    __builtin_memcpy(d, s, 16);
    return;
  }
  if (n & 8) {
    __builtin_memcpy(d, s, 8);
    d += 8;
    s += 8;
  }
  if (n & 4) {
    __builtin_memcpy(d, s, 4);
    d += 4;
    s += 4;
  }
  if (n & 2) {
    __builtin_memcpy(d, s, 2);
    d += 2;
    s += 2;
  }
  if (n & 1) *d = *s;
}

/// stvlx: `host` is the host address of `ea`; the guest bytes 0 .. 15 - (ea & 15) of the vector at ea, upward.
[[gnu::always_inline]] inline void StoreVectorLeft(uint8_t* host, uint32_t ea, const uint8_t* u8) noexcept {
  uint8_t b[16];
  GuestBytes(u8, b);
  StoreBytes(host, b, 16 - (ea & 15));
}

/// stvrx: `block` is the host address of ea & ~15; the vector's last ea & 15 guest bytes at the start of the block.
[[gnu::always_inline]] inline void StoreVectorRight(uint8_t* block, uint32_t ea, const uint8_t* u8) noexcept {
  const uint32_t k = ea & 15;
  if (!k) return;
  uint8_t b[16];
  GuestBytes(u8, b);
  StoreBytes(block, b + 16 - k, k);
}

}  // namespace rex::ppc
