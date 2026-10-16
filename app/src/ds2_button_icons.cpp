// Dark Souls II: the button pictures in the Nintendo style.
//
// The game draws every button prompt ("Confirm", "Back", the key guide, the tutorials, the character creation) with
// characters of its font whose pictures sit on one 512x512 DXT5 page made for the Xbox 360 pad:
// FeFont_Small_0000_x360.tpf inside font/<language>/FeFont_Small.fontbnd.dcx, the same page in every language. At start
// this module redraws the button pictures on that page from the game's own art:
//
//   - A/B/X/Y: the coloured spheres become the dark grey of a Switch controller's buttons, keeping their gloss and the
//     game's white letters, and swap places so that each position shows the Switch's letter (by position, the SDK's
//     default, the bottom button acts as A: the Xbox A becomes B, B becomes A, X becomes Y, Y becomes X);
//   - BACK / START: the same silver pills with a minus and a plus instead of the arrows;
//   - LB/RB/LT/RT: the same shoulder plates with L, R, ZL and ZR, drawn with a real typeface (on the console the Switch's
//     own system font) in the game's letter style: white, a dark outline and a soft shadow;
//   - the plain-text LB / RB of the equipment tabs: only their L / R.
//
// Only the 4x4 blocks that change are compressed again; the rest of the page keeps its original bytes, so the file
// keeps its layout. The result goes to cache/ds2_fonts/<language>/ next to the program and a symbolic link of the
// file system points the game's font folder there. Nothing of the game is shipped: everything is rebuilt from the
// user's copy, and a page that is not byte for byte the expected one is left alone.

#include "ds2_button_icons.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/filesystem/devices/host_path_device.h>
#include <rex/filesystem/entry.h>
#include <rex/filesystem/file.h>
#include <rex/filesystem/vfs.h>
#include <rex/logging.h>
#include <rex/runtime.h>

#ifdef __SWITCH__
#include <switch.h>
REXCVAR_DECLARE(bool, input_xbox_layout);  // sdk/src/input/switch/switch_input_driver.cpp
#endif

// The typeface rasteriser that ImGui already carries, as a private copy in this file.
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include <imstb_truetype.h>

// The zlib decoder of stb_image (compiled in the SDK: sdk/src/ui/image_decode.cpp).
extern "C" int stbi_zlib_decode_buffer(char* obuffer, int olen, const char* ibuffer, int ilen);

REXCVAR_DEFINE_STRING(ds2_button_icons, "auto", "DS2",
                      "Button pictures in the game's prompts: auto (Nintendo on the Switch, Xbox on the PC), nintendo "
                      "or xbox. Read at start");

namespace ds2::button_icons {
namespace {

using rex::X_STATUS;  // X_STATUS_SUCCESS names it unqualified

// The page as it is on the disc (FNV-1a 64 of its 0x40000 bytes); any other page is left alone.
constexpr uint64_t kPageHash = 0x1c8907266662e9f4ull;
constexpr int kPage = 512;
constexpr uint32_t kPageBytes = kPage * kPage;  // DXT5: one byte per texel
constexpr const char* kPageName = "FeFont_Small_0000_x360";
// Raised whenever the drawing changes, so that the cache is rebuilt.
constexpr int kVersion = 3;

constexpr const char* kLanguages[] = {"English", "Japanese", "French", "Germany", "Spanish", "Italian",
                                      "Polish",  "Russian",  "Korean", "Chinese", "Portuguese"};

uint32_t Be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
uint16_t Be16(const uint8_t* p) { return uint16_t(p[0] << 8 | p[1]); }
void PutBe32(uint8_t* p, uint32_t v) {
  p[0] = uint8_t(v >> 24);
  p[1] = uint8_t(v >> 16);
  p[2] = uint8_t(v >> 8);
  p[3] = uint8_t(v);
}

uint64_t Fnv1a(const uint8_t* data, size_t size) {
  uint64_t h = 0xcbf29ce484222325ull;
  for (size_t i = 0; i < size; ++i) {
    h ^= data[i];
    h *= 0x100000001b3ull;
  }
  return h;
}

// ---------------------------------------------------------------------------------------------------------------------
// The page: Xbox 360 tiling and DXT5.

// The byte offset of block (x, y) in a tiled 2D texture `width` blocks wide, `1 << log_bpp` bytes a block.
uint32_t TiledOffset(uint32_t x, uint32_t y, uint32_t width, uint32_t log_bpp) {
  const uint32_t aligned = (width + 31) & ~31u;
  const uint32_t macro = ((x >> 5) + (y >> 5) * (aligned >> 5)) << (log_bpp + 7);
  const uint32_t micro = ((x & 7) + ((y & 0xE) << 2)) << log_bpp;
  const uint32_t off = macro + ((micro & ~0xFu) << 1) + (micro & 0xF) + ((y & 1) << 4);
  return ((off & ~0x1FFu) << 3) + ((y & 16) << 7) + ((off & 0x1C0) << 2) + ((((y & 8) >> 2) + (x >> 3)) & 3) * 64 +
         (off & 0x3F);
}

struct Image {
  int w = 0, h = 0;
  std::vector<float> px;  // RGBA, 0..255
  Image() = default;
  Image(int w_, int h_) : w(w_), h(h_), px(size_t(w_) * h_ * 4, 0.0f) {}
  float* at(int x, int y) { return &px[(size_t(y) * w + x) * 4]; }
  const float* at(int x, int y) const { return &px[(size_t(y) * w + x) * 4]; }
};

struct Rect {
  int x1, y1, x2, y2;  // x2 and y2 exclusive
};

void Rgb565(uint16_t c, float* out) {
  const int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
  out[0] = float((r << 3) | (r >> 2));
  out[1] = float((g << 2) | (g >> 4));
  out[2] = float((b << 3) | (b >> 2));
}

// One BC3 block (little-endian bytes) into 16 RGBA texels.
void DecodeBlock(const uint8_t* b, float out[16][4]) {
  const int a0 = b[0], a1 = b[1];
  int pal_a[8] = {a0, a1};
  if (a0 > a1) {
    for (int i = 0; i < 6; ++i) pal_a[2 + i] = ((6 - i) * a0 + (i + 1) * a1) / 7;
  } else {
    for (int i = 0; i < 4; ++i) pal_a[2 + i] = ((4 - i) * a0 + (i + 1) * a1) / 5;
    pal_a[6] = 0;
    pal_a[7] = 255;
  }
  uint64_t abits = 0;
  for (int i = 0; i < 6; ++i) abits |= uint64_t(b[2 + i]) << (8 * i);
  const uint16_t c0 = uint16_t(b[8] | b[9] << 8), c1 = uint16_t(b[10] | b[11] << 8);
  float pal[4][3];
  Rgb565(c0, pal[0]);
  Rgb565(c1, pal[1]);
  for (int c = 0; c < 3; ++c) {
    pal[2][c] = float(int(2 * pal[0][c] + pal[1][c]) / 3);
    pal[3][c] = float(int(pal[0][c] + 2 * pal[1][c]) / 3);
  }
  const uint32_t cbits = uint32_t(b[12]) | uint32_t(b[13]) << 8 | uint32_t(b[14]) << 16 | uint32_t(b[15]) << 24;
  for (int i = 0; i < 16; ++i) {
    const int ci = (cbits >> (2 * i)) & 3;
    out[i][0] = pal[ci][0];
    out[i][1] = pal[ci][1];
    out[i][2] = pal[ci][2];
    out[i][3] = float(pal_a[(abits >> (3 * i)) & 7]);
  }
}

uint16_t To565(const float* c) {
  const int r = std::clamp(int(std::lround(c[0] * 31.0f / 255.0f)), 0, 31);
  const int g = std::clamp(int(std::lround(c[1] * 63.0f / 255.0f)), 0, 63);
  const int b = std::clamp(int(std::lround(c[2] * 31.0f / 255.0f)), 0, 31);
  return uint16_t(r << 11 | g << 5 | b);
}

// The colour half of a BC3 block (always the four-colour mode): endpoints along the main axis of the opaque texels,
// then two rounds of least squares on the chosen indices.
void EncodeColour(const float in[16][4], uint8_t* out) {
  int n = 0;
  float mean[3] = {0, 0, 0};
  bool use[16];
  for (int i = 0; i < 16; ++i) use[i] = in[i][3] > 0.5f;
  for (int i = 0; i < 16; ++i) n += use[i];
  if (n == 0) {
    for (int i = 0; i < 16; ++i) use[i] = true;
    n = 16;
  }
  for (int i = 0; i < 16; ++i)
    if (use[i])
      for (int c = 0; c < 3; ++c) mean[c] += in[i][c] / n;
  float cov[6] = {0, 0, 0, 0, 0, 0};
  for (int i = 0; i < 16; ++i) {
    if (!use[i]) continue;
    const float r = in[i][0] - mean[0], g = in[i][1] - mean[1], b = in[i][2] - mean[2];
    cov[0] += r * r, cov[1] += r * g, cov[2] += r * b, cov[3] += g * g, cov[4] += g * b, cov[5] += b * b;
  }
  float axis[3] = {1, 1, 1};
  for (int k = 0; k < 8; ++k) {
    const float x = cov[0] * axis[0] + cov[1] * axis[1] + cov[2] * axis[2];
    const float y = cov[1] * axis[0] + cov[3] * axis[1] + cov[4] * axis[2];
    const float z = cov[2] * axis[0] + cov[4] * axis[1] + cov[5] * axis[2];
    const float len = std::sqrt(x * x + y * y + z * z);
    if (len < 1e-6f) break;
    axis[0] = x / len, axis[1] = y / len, axis[2] = z / len;
  }
  float lo = 1e9f, hi = -1e9f;
  for (int i = 0; i < 16; ++i) {
    if (!use[i]) continue;
    const float t = (in[i][0] - mean[0]) * axis[0] + (in[i][1] - mean[1]) * axis[1] + (in[i][2] - mean[2]) * axis[2];
    lo = std::min(lo, t), hi = std::max(hi, t);
  }
  float e0[3], e1[3];
  for (int c = 0; c < 3; ++c) {
    e0[c] = std::clamp(mean[c] + axis[c] * hi, 0.0f, 255.0f);
    e1[c] = std::clamp(mean[c] + axis[c] * lo, 0.0f, 255.0f);
  }

  uint16_t best0 = 0, best1 = 0;
  uint32_t best_bits = 0;
  float best_err = 1e30f;
  for (int round = 0; round < 3; ++round) {
    uint16_t c0 = To565(e0), c1 = To565(e1);
    if (c0 < c1) std::swap(c0, c1), std::swap(e0, e1);
    float pal[4][3];
    Rgb565(c0, pal[0]);
    Rgb565(c1, pal[1]);
    for (int c = 0; c < 3; ++c) {
      pal[2][c] = float(int(2 * pal[0][c] + pal[1][c]) / 3);
      pal[3][c] = float(int(pal[0][c] + 2 * pal[1][c]) / 3);
    }
    uint32_t bits = 0;
    float err = 0;
    int idx[16];
    for (int i = 0; i < 16; ++i) {
      int bi = 0;
      float bd = 1e30f;
      const int count = c0 == c1 ? 1 : 4;
      for (int k = 0; k < count; ++k) {
        const float dr = in[i][0] - pal[k][0], dg = in[i][1] - pal[k][1], db = in[i][2] - pal[k][2];
        const float d = dr * dr + dg * dg + db * db;
        if (d < bd) bd = d, bi = k;
      }
      idx[i] = bi;
      bits |= uint32_t(bi) << (2 * i);
      if (use[i]) err += bd;
    }
    if (err < best_err) best_err = err, best0 = c0, best1 = c1, best_bits = bits;
    // Least squares: each texel is w * e0 + (1 - w) * e1 with w from its index.
    static constexpr float kW[4] = {1.0f, 0.0f, 2.0f / 3.0f, 1.0f / 3.0f};
    float aa = 0, ab = 0, bb = 0, ax[3] = {0, 0, 0}, bx[3] = {0, 0, 0};
    for (int i = 0; i < 16; ++i) {
      if (!use[i]) continue;
      const float w = kW[idx[i]], v = 1.0f - w;
      aa += w * w, ab += w * v, bb += v * v;
      for (int c = 0; c < 3; ++c) ax[c] += w * in[i][c], bx[c] += v * in[i][c];
    }
    const float det = aa * bb - ab * ab;
    if (std::fabs(det) < 1e-6f) break;
    for (int c = 0; c < 3; ++c) {
      e0[c] = std::clamp((ax[c] * bb - bx[c] * ab) / det, 0.0f, 255.0f);
      e1[c] = std::clamp((bx[c] * aa - ax[c] * ab) / det, 0.0f, 255.0f);
    }
  }
  out[0] = uint8_t(best0), out[1] = uint8_t(best0 >> 8);
  out[2] = uint8_t(best1), out[3] = uint8_t(best1 >> 8);
  for (int i = 0; i < 4; ++i) out[4 + i] = uint8_t(best_bits >> (8 * i));
}

// The alpha half: the eight-value mode between the extremes, or the six-value mode (with exact 0 and 255) when it is
// closer.
void EncodeAlpha(const float in[16][4], uint8_t* out) {
  int a[16];
  for (int i = 0; i < 16; ++i) a[i] = std::clamp(int(std::lround(in[i][3])), 0, 255);
  auto fit = [&](int a0, int a1, uint64_t& bits) {
    int pal[8] = {a0, a1};
    if (a0 > a1) {
      for (int i = 0; i < 6; ++i) pal[2 + i] = ((6 - i) * a0 + (i + 1) * a1) / 7;
    } else {
      for (int i = 0; i < 4; ++i) pal[2 + i] = ((4 - i) * a0 + (i + 1) * a1) / 5;
      pal[6] = 0, pal[7] = 255;
    }
    int err = 0;
    bits = 0;
    for (int i = 0; i < 16; ++i) {
      int bi = 0, bd = 1 << 30;
      for (int k = 0; k < 8; ++k) {
        const int d = (a[i] - pal[k]) * (a[i] - pal[k]);
        if (d < bd) bd = d, bi = k;
      }
      bits |= uint64_t(bi) << (3 * i);
      err += bd;
    }
    return err;
  };
  int mn = 255, mx = 0, mn6 = 255, mx6 = 0;
  for (int v : a) {
    mn = std::min(mn, v), mx = std::max(mx, v);
    if (v != 0 && v != 255) mn6 = std::min(mn6, v), mx6 = std::max(mx6, v);
  }
  uint64_t bits8 = 0, bits6 = 0;
  const int e8 = fit(mx, mn, bits8);
  if (mn6 > mx6) mn6 = mx6 = mn;
  const int e6 = fit(mn6, mx6, bits6);
  const bool six = e6 < e8;
  out[0] = uint8_t(six ? mn6 : mx), out[1] = uint8_t(six ? mx6 : mn);
  const uint64_t bits = six ? bits6 : bits8;
  for (int i = 0; i < 6; ++i) out[2 + i] = uint8_t(bits >> (8 * i));
}

void Swap16(uint8_t* b) {
  for (int i = 0; i < 16; i += 2) std::swap(b[i], b[i + 1]);
}

Image DecodePage(const uint8_t* data) {
  Image img(kPage, kPage);
  const uint32_t bw = kPage / 4;
  for (uint32_t by = 0; by < bw; ++by) {
    for (uint32_t bx = 0; bx < bw; ++bx) {
      uint8_t blk[16];
      std::memcpy(blk, data + TiledOffset(bx, by, bw, 4), 16);
      Swap16(blk);
      float texels[16][4];
      DecodeBlock(blk, texels);
      for (int i = 0; i < 16; ++i) std::memcpy(img.at(bx * 4 + (i & 3), by * 4 + (i >> 2)), texels[i], sizeof(texels[i]));
    }
  }
  return img;
}

// Compresses again only the blocks where `now` differs from `before`; returns how many.
int EncodeChanged(const Image& before, const Image& now, uint8_t* data) {
  const uint32_t bw = kPage / 4;
  int changed = 0;
  for (uint32_t by = 0; by < bw; ++by) {
    for (uint32_t bx = 0; bx < bw; ++bx) {
      float texels[16][4];
      bool differs = false;
      for (int i = 0; i < 16; ++i) {
        const float* a = before.at(bx * 4 + (i & 3), by * 4 + (i >> 2));
        const float* b = now.at(bx * 4 + (i & 3), by * 4 + (i >> 2));
        for (int c = 0; c < 4; ++c) {
          texels[i][c] = std::clamp(b[c], 0.0f, 255.0f);
          differs |= std::fabs(a[c] - b[c]) > 0.5f;
        }
      }
      if (!differs) continue;
      uint8_t blk[16];
      EncodeAlpha(texels, blk);
      EncodeColour(texels, blk + 8);
      Swap16(blk);
      std::memcpy(data + TiledOffset(bx, by, bw, 4), blk, 16);
      ++changed;
    }
  }
  return changed;
}

// ---------------------------------------------------------------------------------------------------------------------
// Drawing helpers.

float Lum(const float* p) { return 0.3f * p[0] + 0.59f * p[1] + 0.11f * p[2]; }

void Hsv(const float* p, float& s, float& v) {
  const float mx = std::max({p[0], p[1], p[2]}), mn = std::min({p[0], p[1], p[2]});
  v = mx / 255.0f;
  s = mx > 0 ? (mx - mn) / mx : 0.0f;
}

// Paints `colour` with `coverage` (0..1) over the pixel, keeping the pixel's own alpha (the art's silhouette).
void Paint(float* p, const float* colour, float coverage) {
  coverage = std::clamp(coverage, 0.0f, 1.0f);
  for (int c = 0; c < 3; ++c) p[c] = p[c] * (1.0f - coverage) + colour[c] * coverage;
}

Image Crop(const Image& img, Rect r) {
  Image out(r.x2 - r.x1, r.y2 - r.y1);
  for (int y = r.y1; y < r.y2; ++y)
    for (int x = r.x1; x < r.x2; ++x) std::memcpy(out.at(x - r.x1, y - r.y1), img.at(x, y), 4 * sizeof(float));
  return out;
}

void Paste(Image& img, const Image& part, int x0, int y0) {
  for (int y = 0; y < part.h; ++y)
    for (int x = 0; x < part.w; ++x) std::memcpy(img.at(x0 + x, y0 + y), part.at(x, y), 4 * sizeof(float));
}

// ---------------------------------------------------------------------------------------------------------------------
// A/B/X/Y.

constexpr Rect kY{0, 0, 26, 26}, kX{28, 0, 54, 26}, kB{56, 0, 82, 26}, kA{84, 0, 110, 26};
constexpr Rect kA2{84, 27, 110, 53}, kB2{112, 27, 138, 53};  // the second A and B (Confirm / Back)

// The sphere's brightness values, sorted (pixels that are clearly coloured; the letter and its outline are not).
std::vector<float> SphereValues(const Image& disc) {
  std::vector<float> v;
  for (int i = 0; i < disc.w * disc.h; ++i) {
    const float* p = &disc.px[size_t(i) * 4];
    float s, val;
    Hsv(p, s, val);
    if (p[3] > 128 && s > 0.45f) v.push_back(val);
  }
  std::sort(v.begin(), v.end());
  return v;
}

// The coloured sphere becomes a dark grey one. Each disc's brightness is matched, rank for rank, to the green A's, so
// the four come out in the same tone whatever their colour was; the gloss stays where it was. The white letter and the
// dark outline (unsaturated) keep their brightness.
Image NeutralDisc(const Image& disc, const std::vector<float>& reference) {
  const std::vector<float> own = SphereValues(disc);
  Image out = disc;
  if (own.size() < 2 || reference.size() < 2) return out;
  const float ref_lo = reference.front(), ref_hi = reference.back();
  for (int i = 0; i < disc.w * disc.h; ++i) {
    const float* p = &disc.px[size_t(i) * 4];
    float* q = &out.px[size_t(i) * 4];
    float s, v;
    Hsv(p, s, v);
    const float weight = std::clamp((s - 0.15f) / 0.30f, 0.0f, 1.0f);
    const float rank = float(std::lower_bound(own.begin(), own.end(), v) - own.begin()) / float(own.size() - 1);
    const float ref = reference[std::min(reference.size() - 1, size_t(std::lround(rank * (reference.size() - 1))))];
    const float t = ref_hi > ref_lo ? std::clamp((ref - ref_lo) / (ref_hi - ref_lo), 0.0f, 1.0f) : 0.5f;
    const float g = 28.0f + 92.0f * t;
    const float grey[3] = {g, g, g + 4.0f};
    const float l = Lum(p);
    for (int c = 0; c < 3; ++c) q[c] = l * (1.0f - weight) + grey[c] * weight;
  }
  return out;
}

void FaceButtons(const Image& page, Image& out, bool by_position) {
  const std::vector<float> reference = SphereValues(Crop(page, kA));
  struct Slot {
    Rect at;
    Rect from_position;  // the disc whose letter goes here when the buttons act by position
  };
  const Slot slots[] = {{kY, kX}, {kX, kY}, {kB, kA}, {kA, kB}, {kA2, kB2}, {kB2, kA2}};
  for (const Slot& s : slots) {
    const Rect from = by_position ? s.from_position : s.at;
    Paste(out, NeutralDisc(Crop(page, from), reference), s.at.x1, s.at.y1);
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// BACK / START: minus and plus on the silver pills.

constexpr Rect kBack{140, 0, 166, 26}, kStart{428, 0, 454, 26};

void Pill(const Image& page, Image& out, Rect r, bool plus) {
  Image g = Crop(page, r);
  // The arrow: the darker grey inside the light pill (the pill's own dark rim is at the edges).
  int x1 = g.w, y1 = g.h, x2 = -1, y2 = -1;
  for (int y = 5; y <= g.h - 6; ++y)
    for (int x = 6; x <= g.w - 7; ++x) {
      const float* p = g.at(x, y);
      if (p[3] > 200 && Lum(p) < 170) x1 = std::min(x1, x), y1 = std::min(y1, y), x2 = std::max(x2, x), y2 = std::max(y2, y);
    }
  if (x2 < 0) return;
  x1 = std::max(0, x1 - 1), y1 = std::max(0, y1 - 1), x2 = std::min(g.w, x2 + 2), y2 = std::min(g.h, y2 + 2);
  // The marking's colour: the mean of the darkest quarter of the arrow.
  std::vector<std::array<float, 3>> arrow;
  for (int y = y1; y < y2; ++y)
    for (int x = x1; x < x2; ++x) arrow.push_back({g.at(x, y)[0], g.at(x, y)[1], g.at(x, y)[2]});
  std::sort(arrow.begin(), arrow.end(), [](const auto& a, const auto& b) { return a[0] + a[1] + a[2] < b[0] + b[1] + b[2]; });
  const size_t quarter = std::max<size_t>(1, arrow.size() / 4);
  float colour[3] = {0, 0, 0};
  for (size_t i = 0; i < quarter; ++i)
    for (int c = 0; c < 3; ++c) colour[c] += arrow[i][c] / quarter;
  // The arrow goes: each row of the box blends the pill's pixels just outside it.
  Image clean = g;
  for (int y = y1; y < y2; ++y) {
    const float* a = g.at(std::max(0, x1 - 1), y);
    const float* b = g.at(std::min(g.w - 1, x2), y);
    for (int x = x1; x < x2; ++x) {
      const float t = float(x - x1 + 1) / float(x2 - x1 + 1);
      for (int c = 0; c < 4; ++c) clean.at(x, y)[c] = a[c] * (1 - t) + b[c] * t;
    }
  }
  const float cx = (x1 + x2) * 0.5f, cy = (y1 + y2) * 0.5f, half = 4.4f, bar = 2.2f;
  constexpr int ss = 6;
  for (int y = 0; y < g.h; ++y)
    for (int x = 0; x < g.w; ++x) {
      int hits = 0;
      for (int j = 0; j < ss; ++j)
        for (int i = 0; i < ss; ++i) {
          const float sx = x + (i + 0.5f) / ss, sy = y + (j + 0.5f) / ss;
          const bool h = std::fabs(sy - cy) <= bar / 2 && std::fabs(sx - cx) <= half;
          const bool v = plus && std::fabs(sx - cx) <= bar / 2 && std::fabs(sy - cy) <= half;
          hits += h || v;
        }
      if (hits) Paint(clean.at(x, y), colour, float(hits) / (ss * ss));
    }
  Paste(out, clean, r.x1, r.y1);
}

// ---------------------------------------------------------------------------------------------------------------------
// The shoulder plates.

// A typeface, from the console's shared memory or a Windows font file.
struct Face {
  std::vector<uint8_t> owned;
  const uint8_t* data = nullptr;
  stbtt_fontinfo info{};
  bool ok = false;
};

bool LoadFace(Face& face) {
#ifdef __SWITCH__
  PlFontData font{};
  if (R_SUCCEEDED(plGetSharedFontByType(&font, PlSharedFontType_Standard)) && font.address) {
    face.data = static_cast<const uint8_t*>(font.address);
  }
#else
  for (const char* path : {"C:\\Windows\\Fonts\\arialbd.ttf", "C:\\Windows\\Fonts\\arial.ttf",
                           "C:\\Windows\\Fonts\\segoeuib.ttf"}) {
    FILE* f = rex::filesystem::OpenFile(path, "rb");
    if (!f) continue;
    std::fseek(f, 0, SEEK_END);
    face.owned.resize(size_t(std::ftell(f)));
    std::fseek(f, 0, SEEK_SET);
    const bool read = std::fread(face.owned.data(), 1, face.owned.size(), f) == face.owned.size();
    std::fclose(f);
    if (read) {
      face.data = face.owned.data();
      break;
    }
  }
#endif
  if (!face.data) return false;
  face.ok = stbtt_InitFont(&face.info, face.data, stbtt_GetFontOffsetForIndex(face.data, 0)) != 0;
  return face.ok;
}

// The text as a signed distance field at 8x, so that the stroke can be made as bold as the game's letters and the
// outline drawn at an exact width.
struct Lettering {
  static constexpr int kSs = 8;
  int w = 0, h = 0;          // high-resolution size
  float left = 0, top = 0;   // the field's position relative to the ink's left edge and the baseline (high-res pixels)
  float ink_width = 0;       // in output pixels
  float per_px = 1;          // field units per high-resolution pixel
  float embolden = 0;        // output pixels added to each side of the stroke
  std::vector<uint8_t> field;

  // Signed distance in output pixels (positive inside) at a high-resolution point relative to the ink's left edge
  // and the baseline.
  float Distance(float hx, float hy) const {
    const int x = int(std::floor(hx - left)), y = int(std::floor(hy - top));
    if (x < 0 || y < 0 || x >= w || y >= h) return -100.0f;
    return (float(field[size_t(y) * w + x]) - 128.0f) / per_px / kSs;
  }
};

bool MakeLettering(const Face& face, const char* text, float cap_px, float stroke_px, Lettering& out) {
  constexpr int ss = Lettering::kSs;
  int x0, y0, x1, y1;
  if (!stbtt_GetCodepointBox(&face.info, 'H', &x0, &y0, &x1, &y1) || y1 <= 0) return false;
  const int cap_units = y1;
  float scale = cap_px * ss / float(cap_units);
  // How bold the typeface already is: the width of its I. The boldening grows the letters on every side, so the
  // typeface is set that much smaller to keep the white at cap_px.
  int ix0 = 0, iy0 = 0, ix1 = 0, iy1 = 0;
  const bool has_i = stbtt_GetCodepointBox(&face.info, 'I', &ix0, &iy0, &ix1, &iy1) != 0;
  for (int pass = 0; pass < 2; ++pass) {
    const float stem = has_i ? (ix1 - ix0) * scale / ss : stroke_px;
    out.embolden = std::clamp((stroke_px - stem) * 0.5f, 0.0f, 0.9f);
    scale = (cap_px - 2.0f * out.embolden) * ss / float(cap_units);
  }
  const int pad = int(std::ceil((out.embolden + 3.0f) * ss));
  out.per_px = 120.0f / pad;

  struct Glyph {
    unsigned char* sdf;
    int w, h, xoff, yoff;
    float pen;
  };
  std::vector<Glyph> glyphs;
  float pen = 0;
  float ink_l = 1e9f, ink_r = -1e9f;
  for (const char* c = text; *c; ++c) {
    Glyph g{};
    g.pen = pen;
    g.sdf = stbtt_GetCodepointSDF(&face.info, scale, *c, pad, 128, out.per_px, &g.w, &g.h, &g.xoff, &g.yoff);
    int advance, bearing;
    stbtt_GetCodepointHMetrics(&face.info, *c, &advance, &bearing);
    if (stbtt_GetCodepointBox(&face.info, *c, &x0, &y0, &x1, &y1)) {
      ink_l = std::min(ink_l, pen + x0 * scale);
      ink_r = std::max(ink_r, pen + x1 * scale);
    }
    pen += advance * scale + (c[1] ? stbtt_GetCodepointKernAdvance(&face.info, c[0], c[1]) * scale : 0.0f);
    // A little air between letters, as in the game's plates (the outline and the boldening eat into it).
    pen += (out.embolden * 2.0f + 0.6f) * ss;
    if (g.sdf) glyphs.push_back(g);
  }
  if (glyphs.empty()) return false;
  float fl = 1e9f, ft = 1e9f, fr = -1e9f, fb = -1e9f;
  for (const Glyph& g : glyphs) {
    fl = std::min(fl, g.pen + g.xoff), ft = std::min(ft, float(g.yoff));
    fr = std::max(fr, g.pen + g.xoff + g.w), fb = std::max(fb, float(g.yoff + g.h));
  }
  out.w = int(std::ceil(fr - fl)) + 1, out.h = int(std::ceil(fb - ft)) + 1;
  out.field.assign(size_t(out.w) * out.h, 0);
  for (const Glyph& g : glyphs) {
    const int gx = int(std::lround(g.pen + g.xoff - fl)), gy = int(std::lround(g.yoff - ft));
    for (int y = 0; y < g.h; ++y)
      for (int x = 0; x < g.w; ++x) {
        uint8_t& d = out.field[size_t(gy + y) * out.w + gx + x];
        d = std::max(d, g.sdf[y * g.w + x]);
      }
    stbtt_FreeSDF(g.sdf, nullptr);
  }
  out.left = fl - ink_l;
  out.top = ft;
  out.ink_width = (ink_r - ink_l) / ss + out.embolden * 2.0f;
  return true;
}

struct Plate {
  Rect glyph;    // on the page
  Rect letters;  // the old letters with their outline (glyph coordinates, both ends inclusive)
  int reference;  // a clean row of the plate (glyph coordinates): its rim is copied where the old letters touched it
  int cap_top;    // the first row of the letters' white (the game's are 10 rows high)
  float centre;   // where the new text is centred (glyph coordinates)
  const char* text;
};

// Measured on the page (its bytes are checked before anything is drawn): the old letters' white is 10 rows high, rows
// 8 to 17 (9 to 18 on RB), with a 1-pixel dark outline and a soft shadow below; the stroke is about 2 pixels wide.
constexpr Plate kPlates[] = {
    {{168, 0, 198, 26}, {2, 7, 24, 19}, 6, 9, 12.5f, "R"},
    {{200, 0, 226, 26}, {2, 7, 22, 19}, 5, 8, 12.0f, "ZR"},
    {{228, 0, 258, 26}, {7, 7, 26, 19}, 20, 8, 15.5f, "L"},
    {{260, 0, 286, 26}, {3, 7, 21, 19}, 5, 8, 12.0f, "ZL"},
};

// The plate without its letters. Inside the letters' rectangle every pixel that is not the plate's face colour goes
// (the white, the outline, and what the block compression smeared into the counters), and so does every pixel within
// 2 of the white; next to the plate's silhouette those pixels take the rim of the clean reference row instead, aligned
// on the silhouette's edge, so the border stays.
Image CleanPlate(const Image& page, const Plate& plate) {
  const Image g = Crop(page, plate.glyph);
  Image out = g;
  const Rect& l = plate.letters;
  std::vector<uint8_t> white(size_t(g.w) * g.h, 0);
  for (int y = l.y1; y <= l.y2; ++y)
    for (int x = l.x1; x <= l.x2; ++x) white[size_t(y) * g.w + x] = g.at(x, y)[3] > 200 && Lum(g.at(x, y)) >= 225;
  auto near_white = [&](int x, int y) {
    for (int yy = std::max(0, y - 2); yy <= std::min(g.h - 1, y + 2); ++yy)
      for (int xx = std::max(0, x - 2); xx <= std::min(g.w - 1, x + 2); ++xx)
        if ((xx - x) * (xx - x) + (yy - y) * (yy - y) <= 4 && white[size_t(yy) * g.w + xx]) return true;
    return false;
  };
  auto edges = [&](int y, int& xl, int& xr) {
    xl = -1, xr = -1;
    for (int x = 0; x < g.w; ++x)
      if (g.at(x, y)[3] > 128) xr = x, xl = xl < 0 ? x : xl;
    return xl >= 0;
  };
  auto face_of = [&](int y, float* colour) {
    // The median face colour of the row outside the letters, or of the whole band when the row has too little.
    std::vector<std::array<float, 3>> v;
    auto take = [&](int yy, bool outside_only) {
      for (int x = 0; x < g.w; ++x) {
        const float* p = g.at(x, yy);
        const bool in_letters = x >= l.x1 && x <= l.x2 && yy >= l.y1 && yy <= l.y2;
        if (p[3] > 200 && Lum(p) >= 176 && Lum(p) <= 200 && !(outside_only && in_letters)) v.push_back({p[0], p[1], p[2]});
      }
    };
    take(y, true);
    if (v.size() < 3) {
      v.clear();
      for (int yy = l.y1; yy <= l.y2; ++yy) take(yy, false);
    }
    if (v.empty()) return false;
    for (int c = 0; c < 3; ++c) {
      std::vector<float> ch;
      for (const auto& p : v) ch.push_back(p[c]);
      std::nth_element(ch.begin(), ch.begin() + ch.size() / 2, ch.end());
      colour[c] = ch[ch.size() / 2];
    }
    return true;
  };
  int rl, rr;
  if (!edges(plate.reference, rl, rr)) return out;
  for (int y = std::max(0, l.y1 - 1); y <= std::min(g.h - 1, l.y2 + 1); ++y) {
    int xl, xr;
    float face[3];
    if (!edges(y, xl, xr) || !face_of(y, face)) continue;
    for (int x = std::max(xl, l.x1 - 2); x <= std::min(xr, l.x2 + 2); ++x) {
      const float* p = g.at(x, y);
      const bool near = near_white(x, y);
      const bool inside = x >= l.x1 && x <= l.x2 && y >= l.y1 && y <= l.y2;
      const float* q = face;
      if (std::min(x - xl, xr - x) < 2) {
        if (!near) continue;
        q = x - xl < 2 ? g.at(std::min(rr, rl + (x - xl)), plate.reference)
                       : g.at(std::max(rl, rr - (xr - x)), plate.reference);
      } else if (!near && !(inside && std::fabs(Lum(p) - Lum(face)) > 16.0f)) {
        continue;
      }
      float* o = out.at(x, y);
      o[0] = q[0], o[1] = q[1], o[2] = q[2];
    }
  }
  return out;
}

void ShoulderPlate(const Image& page, Image& out, const Plate& plate, const Face& face) {
  Image g = CleanPlate(page, plate);
  Lettering text;
  if (face.ok && MakeLettering(face, plate.text, 10.0f, 2.0f, text)) {
    // The ink's left edge (without the boldening) and the baseline, in glyph pixels.
    const float ink_left = plate.centre - text.ink_width * 0.5f + text.embolden;
    const float baseline = float(plate.cap_top + 10) - text.embolden;
    constexpr int ss = Lettering::kSs;
    constexpr float kOutline = 1.0f, kShadowDrop = 1.5f;
    const float white[3] = {252, 252, 252}, edge[3] = {14, 14, 14}, shadow[3] = {0, 0, 0};
    for (int y = 0; y < g.h; ++y)
      for (int x = 0; x < g.w; ++x) {
        float* p = g.at(x, y);
        if (p[3] <= 0) continue;
        int fill = 0, line = 0, drop = 0;
        for (int j = 0; j < ss; ++j)
          for (int i = 0; i < ss; ++i) {
            const float hx = (x - ink_left) * ss + i + 0.5f;
            const float hy = (y - baseline) * ss + j + 0.5f;
            const float d = text.Distance(hx, hy);
            fill += d >= -text.embolden;
            line += d >= -(text.embolden + kOutline);
            drop += text.Distance(hx, hy - kShadowDrop * ss) >= -(text.embolden + kOutline);
          }
        const float n = float(ss * ss);
        Paint(p, shadow, 0.45f * drop / n);
        Paint(p, edge, line / n);
        Paint(p, white, fill / n);
      }
  }
  Paste(out, g, plate.glyph.x1, plate.glyph.y1);
}

// The plain-text LB / RB (the equipment tabs): only the first letter, centred.
constexpr Rect kLBText{142, 27, 175, 54}, kRBText{175, 27, 210, 54};

void FirstLetter(const Image& page, Image& out, Rect r) {
  Image g = Crop(page, r);
  std::vector<float> cols(g.w, 0.0f);
  int top = g.h;
  for (int y = 0; y < g.h; ++y)
    for (int x = 0; x < g.w; ++x) {
      cols[x] += g.at(x, y)[3];
      if (g.at(x, y)[3] > 20) top = std::min(top, y);
    }
  int first = -1, last = -1;
  for (int x = 0; x < g.w; ++x)
    if (cols[x] > 0) last = x, first = first < 0 ? x : first;
  if (first < 0) return;
  const int mid = (first + last) / 2;
  int cut = mid;
  for (int x = std::max(0, mid - 3); x <= std::min(g.w - 1, mid + 3); ++x)
    if (cols[x] < cols[cut]) cut = x;
  // The first letter's ink.
  int l1 = g.w, r1 = -1, b1 = -1;
  for (int y = 0; y < g.h; ++y)
    for (int x = 0; x <= cut; ++x)
      if (g.at(x, y)[3] > 20) l1 = std::min(l1, x), r1 = std::max(r1, x), b1 = std::max(b1, y);
  if (r1 < 0) return;
  Image blank(g.w, g.h);
  const int dx = (g.w - (r1 - l1 + 1)) / 2 - l1;
  for (int y = 0; y < g.h; ++y)
    for (int x = 0; x <= cut; ++x)
      if (x + dx >= 0 && x + dx < g.w) std::memcpy(blank.at(x + dx, y), g.at(x, y), 4 * sizeof(float));
  Paste(out, blank, r.x1, r.y1);
}

// The whole page; returns the number of blocks compressed again.
int RedrawPage(uint8_t* data, bool by_position, const Face& face) {
  const Image page = DecodePage(data);
  Image out = page;
  FaceButtons(page, out, by_position);
  Pill(page, out, kBack, false);
  Pill(page, out, kStart, true);
  for (const Plate& plate : kPlates) ShoulderPlate(page, out, plate, face);
  FirstLetter(page, out, kLBText);
  FirstLetter(page, out, kRBText);
  return EncodeChanged(page, out, data);
}

// ---------------------------------------------------------------------------------------------------------------------
// The font file: DCX (zlib) around a BND4 that holds the page's TPF. The page is patched in place, so nothing else
// in the file moves.

bool ReadGuestFile(rex::filesystem::VirtualFileSystem* vfs, const std::string& path, std::vector<uint8_t>& out) {
  auto* entry = vfs->ResolvePath(path);
  if (!entry) return false;
  rex::filesystem::File* file = nullptr;
  if (entry->Open(rex::filesystem::FileAccess::kFileReadData, &file) != X_STATUS_SUCCESS || !file) return false;
  out.resize(entry->size());
  size_t read = 0;
  const bool ok = file->ReadSync(std::span<uint8_t>(out.data(), out.size()), 0, &read) == X_STATUS_SUCCESS &&
                  read == out.size();
  file->Destroy();
  return ok;
}

// The DCX's zlib stream with stored (uncompressed) blocks: any inflater reads it.
std::vector<uint8_t> ZlibStored(const std::vector<uint8_t>& data) {
  std::vector<uint8_t> z = {0x78, 0x01};
  size_t pos = 0;
  do {
    const size_t len = std::min<size_t>(65535, data.size() - pos);
    const bool last = pos + len == data.size();
    z.push_back(last ? 1 : 0);
    z.push_back(uint8_t(len)), z.push_back(uint8_t(len >> 8));
    z.push_back(uint8_t(~len)), z.push_back(uint8_t(~len >> 8));
    z.insert(z.end(), data.begin() + pos, data.begin() + pos + len);
    pos += len;
  } while (pos < data.size());
  uint32_t a = 1, b = 0;
  for (uint8_t v : data) a = (a + v) % 65521, b = (b + a) % 65521;
  const uint32_t adler = b << 16 | a;
  z.push_back(uint8_t(adler >> 24)), z.push_back(uint8_t(adler >> 16));
  z.push_back(uint8_t(adler >> 8)), z.push_back(uint8_t(adler));
  return z;
}

// Finds the page inside the unpacked font; returns its offset, or 0.
size_t FindPage(const std::vector<uint8_t>& bnd) {
  const size_t name_len = std::strlen(kPageName);
  for (size_t p = 0; p + 0x40 < bnd.size(); p += 4) {
    if (std::memcmp(&bnd[p], "TPF\0", 4) != 0) continue;
    const uint8_t* tpf = &bnd[p];
    if (Be32(tpf + 8) != 1) continue;
    const uint32_t name = Be32(tpf + 0x24);
    if (p + name + name_len >= bnd.size() || std::memcmp(tpf + name, kPageName, name_len) != 0) continue;
    const uint32_t off = Be32(tpf + 0x10), size = Be32(tpf + 0x14);
    if (size != kPageBytes || tpf[0x18] != 5 || Be16(tpf + 0x1C) != kPage || Be16(tpf + 0x1E) != kPage) continue;
    if (p + off + size > bnd.size()) continue;
    return p + off;
  }
  return 0;
}

enum class Result { kBuilt, kCached, kMissing, kUnexpected };

Result BuildLanguage(rex::filesystem::VirtualFileSystem* vfs, const char* language, const std::filesystem::path& dir,
                     const std::string& style, bool by_position, const std::function<const Face&()>& face,
                     int* blocks) {
  std::vector<uint8_t> src;
  if (!ReadGuestFile(vfs, std::string("game:\\font\\") + language + "\\FeFont_Small.fontbnd.dcx", src)) {
    return Result::kMissing;
  }
  const std::filesystem::path out_dir = dir / language;
  const std::filesystem::path out_file = out_dir / "FeFont_Small.fontbnd.dcx";
  const std::filesystem::path stamp_file = out_dir / "FeFont_Small.stamp";
  char stamp[160];
  std::snprintf(stamp, sizeof(stamp), "v%d %s %zu %016llx", kVersion, style.c_str(), src.size(),
                static_cast<unsigned long long>(Fnv1a(src.data(), src.size())));
  {
    std::error_code ec;
    FILE* f = rex::filesystem::OpenFile(stamp_file, "rb");
    if (f) {
      char old[160] = {};
      const size_t n = std::fread(old, 1, sizeof(old) - 1, f);
      std::fclose(f);
      if (std::string(old, n) == stamp && std::filesystem::is_regular_file(out_file, ec)) return Result::kCached;
    }
  }

  if (src.size() < 0x50 || std::memcmp(src.data(), "DCX\0", 4) != 0 || std::memcmp(&src[0x18], "DCS\0", 4) != 0) {
    return Result::kUnexpected;
  }
  size_t dca = 0;
  for (size_t p = 0x24; p + 8 <= std::min<size_t>(src.size(), 0x80); p += 4)
    if (std::memcmp(&src[p], "DCA\0", 4) == 0) dca = p;
  if (!dca) return Result::kUnexpected;
  const size_t data_off = dca + Be32(&src[dca + 4]);
  const uint32_t usize = Be32(&src[0x1C]), csize = Be32(&src[0x20]);
  if (data_off + csize > src.size() || usize == 0 || usize > (64u << 20)) return Result::kUnexpected;
  std::vector<uint8_t> bnd(usize);
  if (stbi_zlib_decode_buffer(reinterpret_cast<char*>(bnd.data()), int(usize),
                              reinterpret_cast<const char*>(&src[data_off]), int(csize)) != int(usize)) {
    return Result::kUnexpected;
  }
  const size_t page = FindPage(bnd);
  if (!page || Fnv1a(&bnd[page], kPageBytes) != kPageHash) return Result::kUnexpected;

  *blocks = RedrawPage(&bnd[page], by_position, face());

  std::vector<uint8_t> dst(src.begin(), src.begin() + data_off);
  const std::vector<uint8_t> z = ZlibStored(bnd);
  PutBe32(&dst[0x20], uint32_t(z.size()));
  dst.insert(dst.end(), z.begin(), z.end());
  std::error_code ec;
  std::filesystem::create_directories(out_dir, ec);
  FILE* f = rex::filesystem::OpenFile(out_file, "wb");
  if (!f) return Result::kUnexpected;
  const bool written = std::fwrite(dst.data(), 1, dst.size(), f) == dst.size();
  std::fclose(f);
  if (!written) return Result::kUnexpected;
  f = rex::filesystem::OpenFile(stamp_file, "wb");
  if (f) {
    std::fwrite(stamp, 1, std::strlen(stamp), f);
    std::fclose(f);
  }
  return Result::kBuilt;
}

}  // namespace

void Install(rex::Runtime* runtime) {
  std::string mode = REXCVAR_GET(ds2_button_icons);
  if (mode == "auto") {
#ifdef __SWITCH__
    mode = "nintendo";
#else
    mode = "xbox";
#endif
  }
  if (mode != "nintendo") {
    REXLOG_INFO("[buttons] the game's own Xbox 360 button pictures (ds2_button_icons = {})", mode);
    return;
  }
  auto* vfs = runtime ? runtime->file_system() : nullptr;
  if (!vfs) return;
#ifdef __SWITCH__
  const bool by_position = REXCVAR_GET(input_xbox_layout);
  const bool pl = R_SUCCEEDED(plInitialize(PlServiceType_User));
#else
  const bool by_position = true;  // as the Switch's default
#endif
  const std::string style = by_position ? "nintendo-position" : "nintendo-letter";

  const auto start = std::chrono::steady_clock::now();
  const std::filesystem::path dir = rex::filesystem::GetExecutableFolder() / "cache" / "ds2_fonts";
  // The typeface is only needed when something is drawn; a start from the cache does not touch it.
  Face face;
  bool face_tried = false;
  const std::function<const Face&()> get_face = [&]() -> const Face& {
    if (!face_tried) {
      face_tried = true;
      if (!LoadFace(face)) REXLOG_WARN("[buttons] no typeface for L / R / ZL / ZR: the shoulder plates stay blank");
    }
    return face;
  };
  std::string built, cached, failed;
  int blocks = 0;
  std::vector<std::string> ready;
  for (const char* language : kLanguages) {
    int changed = 0;
    switch (BuildLanguage(vfs, language, dir, style, by_position, get_face, &changed)) {
      case Result::kBuilt:
        built += std::string(built.empty() ? "" : ", ") + language;
        blocks = changed;
        ready.push_back(language);
        break;
      case Result::kCached:
        cached += std::string(cached.empty() ? "" : ", ") + language;
        ready.push_back(language);
        break;
      case Result::kUnexpected:
        failed += std::string(failed.empty() ? "" : ", ") + language;
        break;
      case Result::kMissing:
        break;
    }
  }
#ifdef __SWITCH__
  if (pl) plExit();
#endif
  if (ready.empty()) {
    REXLOG_WARN("[buttons] no font page to redraw (unexpected: {}): the Xbox 360 pictures stay",
                failed.empty() ? "none" : failed);
    return;
  }

  // The rebuilt fonts as a device of their own, and each language's font folder pointed at it: the file system
  // resolves a file's folder and then looks the name up in it, so a link on the file itself is never consulted
  // (font\<language>\ holds only this file on the disc). The game asks for game:\font\<language>\..., which the game:
  // link turns into the game folder's device path; both are linked.
  constexpr const char* kMount = "\\Device\\DS2Fonts";
  auto device = std::make_unique<rex::filesystem::HostPathDevice>(kMount, dir, /*read_only=*/false);
  if (!device->Initialize() || !vfs->RegisterDevice(std::move(device))) {
    REXLOG_WARN("[buttons] could not mount {}: the Xbox 360 pictures stay", dir.string());
    return;
  }
  for (const std::string& language : ready) {
    const std::string folder = "\\font\\" + language;
    const std::string target = std::string(kMount) + "\\" + language;
    vfs->RegisterSymbolicLink("game:" + folder, target);
    vfs->RegisterSymbolicLink("\\Device\\Harddisk0\\Partition1" + folder, target);
  }
  const double ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  REXLOG_INFO("[buttons] Nintendo button pictures ({}), {:.0f} ms: built {} ({} blocks), from the cache {}{}{}",
              by_position ? "by position: the bottom button is B" : "by letter", ms,
              built.empty() ? "none" : built, blocks, cached.empty() ? "none" : cached,
              failed.empty() ? "" : "; left alone (not the expected page): ", failed);
}

}  // namespace ds2::button_icons
