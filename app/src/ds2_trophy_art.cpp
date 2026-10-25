// Dark Souls II: the game's own art for the trophy screen and the unlock notice. See ds2_trophy_art.h.
//
// The menu pages are TPFs inside menu\40.febnd.dcx (In-game_01, In-game_02) and menu\99.febnd.dcx (waku, waku_02,
// ItemSellect_02, IconBase), entries of the GameData archives (BHF4 headers, not encrypted; each entry a DCX: zlib
// around a BND4 of files). A page is DXT1/3/5, tiled the Xbox 360 way and 16-bit swapped: decoded once, and only the
// rectangles used become textures. The font is font\<language>\FeFont_Small.fontbnd.dcx (a loose file; the button
// pictures' module links it to the port's copy): FeFont_Small.ccm (the glyphs) and its pages. The common texts are
// menu\Text\<language>\common.fmg.

#include "ds2_trophy_art.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

#include <rex/cvar.h>
#include <rex/filesystem/entry.h>
#include <rex/filesystem/file.h>
#include <rex/filesystem/vfs.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/ui/immediate_drawer.h>

// The zlib decoder of stb_image (compiled in the SDK: sdk/src/ui/image_decode.cpp).
extern "C" int stbi_zlib_decode_buffer(char* obuffer, int olen, const char* ibuffer, int ilen);

REXCVAR_DEFINE_STRING(ds2_trophy_dump, "", "DS2",
                      "PC test: a folder where the trophy screen and the notice write what they draw (their ImGui "
                      "draw lists and textures), once each, for tools/features/raster_dump.py");

namespace ds2::trophy_art {
namespace {

using rex::X_STATUS;  // X_STATUS_SUCCESS names it unqualified

using Bytes = std::vector<uint8_t>;

uint16_t Be16(const uint8_t* p) { return uint16_t(p[0] << 8 | p[1]); }
uint32_t Be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
uint64_t Be64(const uint8_t* p) { return uint64_t(Be32(p)) << 32 | Be32(p + 4); }

// --- files ----------------------------------------------------------------------------------------------------------

bool ReadRange(rex::filesystem::VirtualFileSystem* vfs, const std::string& path, uint64_t offset, uint64_t size,
               Bytes& out) {
  auto* entry = vfs->ResolvePath(path);
  if (!entry) return false;
  if (size == UINT64_MAX) size = entry->size();
  if (offset + size > entry->size() || size > (256u << 20)) return false;
  rex::filesystem::File* file = nullptr;
  if (entry->Open(rex::filesystem::FileAccess::kFileReadData, &file) != X_STATUS_SUCCESS || !file) return false;
  out.resize(size_t(size));
  size_t read = 0;
  const bool ok = file->ReadSync(std::span<uint8_t>(out.data(), out.size()), offset, &read) == X_STATUS_SUCCESS &&
                  read == out.size();
  file->Destroy();
  return ok;
}

std::string Lower(std::string s) {
  for (char& c : s) c = char(std::tolower(uint8_t(c)));
  return s;
}

// An entry of the GameData archives, by its name (menu\40.febnd.dcx).
bool ReadArchived(rex::filesystem::VirtualFileSystem* vfs, const std::string& name, Bytes& out) {
  const std::string want = Lower(name);
  for (const char* n : {"1", "2"}) {
    Bytes bhd;
    if (!ReadRange(vfs, std::string("game:\\GameData") + n + ".bhd", 0, UINT64_MAX, bhd)) continue;
    if (bhd.size() < 0x40 || std::memcmp(bhd.data(), "BHF4", 4) != 0) continue;
    const uint32_t count = Be32(&bhd[0x0C]);
    const uint64_t stride = Be64(&bhd[0x20]);
    if (stride < 24 || 0x40 + uint64_t(count) * stride > bhd.size()) continue;
    for (uint32_t i = 0; i < count; ++i) {
      const uint8_t* e = &bhd[0x40 + size_t(i) * stride];
      const uint32_t name_off = Be32(e + stride - 4);
      if (name_off >= bhd.size()) continue;
      const char* s = reinterpret_cast<const char*>(&bhd[name_off]);
      const size_t len = strnlen(s, bhd.size() - name_off);
      if (len != want.size() || Lower(std::string(s, len)) != want) continue;
      return ReadRange(vfs, std::string("game:\\GameData") + n + ".bdt", Be32(e + 16), Be64(e + 8), out);
    }
  }
  return false;
}

// DCX: 'DCX\0' ... 'DCS\0' uncompressed size, compressed size ... 'DCA\0' the zlib stream.
bool UnDcx(const Bytes& src, Bytes& out) {
  if (src.size() < 0x50 || std::memcmp(src.data(), "DCX\0", 4) != 0) {
    out = src;
    return true;
  }
  size_t dca = 0;
  for (size_t p = 0x24; p + 8 <= std::min<size_t>(src.size(), 0x80); p += 4)
    if (std::memcmp(&src[p], "DCA\0", 4) == 0) dca = p;
  if (!dca) return false;
  const size_t data_off = dca + Be32(&src[dca + 4]);
  const uint32_t usize = Be32(&src[0x1C]), csize = Be32(&src[0x20]);
  if (data_off + csize > src.size() || !usize || usize > (64u << 20)) return false;
  out.resize(usize);
  return stbi_zlib_decode_buffer(reinterpret_cast<char*>(out.data()), int(usize),
                                 reinterpret_cast<const char*>(&src[data_off]), int(csize)) == int(usize);
}

// The files of a BND4: name (the part after the last '\') -> bytes.
std::unordered_map<std::string, Bytes> Bnd4Files(const Bytes& b) {
  std::unordered_map<std::string, Bytes> files;
  if (b.size() < 0x40 || std::memcmp(b.data(), "BND4", 4) != 0) return files;
  const uint32_t count = Be32(&b[0x0C]);
  const uint64_t stride = Be64(&b[0x20]);
  const bool unicode = b[0x30] != 0;
  for (uint32_t i = 0; i < count; ++i) {
    const size_t o = 0x40 + size_t(i) * stride;
    if (o + stride > b.size()) break;
    const uint64_t size = Be64(&b[o + 8]);
    const uint32_t offset = stride >= 36 ? Be32(&b[o + 24]) : Be32(&b[o + 16]);
    const uint32_t name_off = stride >= 36 ? Be32(&b[o + 32]) : Be32(&b[o + 20]);
    if (offset + size > b.size() || name_off >= b.size()) continue;
    std::string name;
    if (unicode) {
      for (size_t p = name_off; p + 1 < b.size() && (b[p] || b[p + 1]); p += 2) name += char(b[p + 1]);
    } else {
      for (size_t p = name_off; p < b.size() && b[p]; ++p) name += char(b[p]);
    }
    const size_t slash = name.find_last_of("\\/");
    files[Lower(slash == std::string::npos ? name : name.substr(slash + 1))] =
        Bytes(b.begin() + offset, b.begin() + offset + size);
  }
  return files;
}

// --- textures -------------------------------------------------------------------------------------------------------

struct Image {
  int w = 0, h = 0;
  std::vector<uint8_t> rgba;
};

// The byte offset of block (x, y) in a tiled 2D texture `width` blocks wide, `1 << log_bpp` bytes a block.
uint32_t TiledOffset(uint32_t x, uint32_t y, uint32_t width, uint32_t log_bpp) {
  const uint32_t aligned = (width + 31) & ~31u;
  const uint32_t macro = ((x >> 5) + (y >> 5) * (aligned >> 5)) << (log_bpp + 7);
  const uint32_t micro = ((x & 7) + ((y & 0xE) << 2)) << log_bpp;
  const uint32_t off = macro + ((micro & ~0xFu) << 1) + (micro & 0xF) + ((y & 1) << 4);
  return ((off & ~0x1FFu) << 3) + ((y & 16) << 7) + ((off & 0x1C0) << 2) + ((((y & 8) >> 2) + (x >> 3)) & 3) * 64 +
         (off & 0x3F);
}

void Rgb565(uint16_t c, int* out) {
  const int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
  out[0] = (r << 3) | (r >> 2);
  out[1] = (g << 2) | (g >> 4);
  out[2] = (b << 3) | (b >> 2);
}

// A one-texture TPF (DXT1, DXT3 or DXT5) to RGBA, its top level.
bool DecodeTpf(const Bytes& tpf, Image& img) {
  if (tpf.size() < 0x40 || std::memcmp(tpf.data(), "TPF\0", 4) != 0) return false;
  const uint32_t off = Be32(&tpf[0x10]), size = Be32(&tpf[0x14]);
  const int format = tpf[0x18];
  const int w = Be16(&tpf[0x1C]), h = Be16(&tpf[0x1E]);
  const uint32_t block_bytes = format == 1 ? 8 : 16;
  if ((format != 1 && format != 3 && format != 5) || w < 4 || h < 4 || off + size > tpf.size()) return false;
  const uint32_t bw = uint32_t(w) / 4, bh = uint32_t(h) / 4;
  if (uint64_t(bw) * bh * block_bytes > size) return false;
  img.w = w, img.h = h;
  img.rgba.assign(size_t(w) * h * 4, 0);
  const uint8_t* data = &tpf[off];
  for (uint32_t by = 0; by < bh; ++by) {
    for (uint32_t bx = 0; bx < bw; ++bx) {
      const uint32_t o = TiledOffset(bx, by, bw, block_bytes == 8 ? 3 : 4);
      if (o + block_bytes > size) continue;
      uint8_t b[16];
      std::memcpy(b, data + o, block_bytes);
      for (uint32_t i = 0; i < block_bytes; i += 2) std::swap(b[i], b[i + 1]);
      const uint8_t* cb = b + block_bytes - 8;
      const uint16_t c0 = uint16_t(cb[0] | cb[1] << 8), c1 = uint16_t(cb[2] | cb[3] << 8);
      int pal[4][3];
      Rgb565(c0, pal[0]);
      Rgb565(c1, pal[1]);
      const bool four = format != 1 || c0 > c1;
      for (int c = 0; c < 3; ++c) {
        pal[2][c] = four ? (2 * pal[0][c] + pal[1][c]) / 3 : (pal[0][c] + pal[1][c]) / 2;
        pal[3][c] = four ? (pal[0][c] + 2 * pal[1][c]) / 3 : 0;
      }
      const uint32_t bits = uint32_t(cb[4]) | uint32_t(cb[5]) << 8 | uint32_t(cb[6]) << 16 | uint32_t(cb[7]) << 24;
      int alpha[16];
      if (format == 5) {
        const int a0 = b[0], a1 = b[1];
        int pa[8] = {a0, a1};
        if (a0 > a1) {
          for (int i = 0; i < 6; ++i) pa[2 + i] = ((6 - i) * a0 + (i + 1) * a1) / 7;
        } else {
          for (int i = 0; i < 4; ++i) pa[2 + i] = ((4 - i) * a0 + (i + 1) * a1) / 5;
          pa[6] = 0, pa[7] = 255;
        }
        uint64_t ab = 0;
        for (int i = 0; i < 6; ++i) ab |= uint64_t(b[2 + i]) << (8 * i);
        for (int i = 0; i < 16; ++i) alpha[i] = pa[(ab >> (3 * i)) & 7];
      } else if (format == 3) {
        uint64_t ab = 0;
        for (int i = 0; i < 8; ++i) ab |= uint64_t(b[i]) << (8 * i);
        for (int i = 0; i < 16; ++i) alpha[i] = int((ab >> (4 * i)) & 15) * 17;
      } else {
        for (int i = 0; i < 16; ++i) alpha[i] = (!four && ((bits >> (2 * i)) & 3) == 3) ? 0 : 255;
      }
      for (int i = 0; i < 16; ++i) {
        const int* p = pal[(bits >> (2 * i)) & 3];
        uint8_t* d = &img.rgba[(size_t(by * 4 + (i >> 2)) * w + bx * 4 + (i & 3)) * 4];
        d[0] = uint8_t(p[0]), d[1] = uint8_t(p[1]), d[2] = uint8_t(p[2]), d[3] = uint8_t(alpha[i]);
      }
    }
  }
  return true;
}

struct Sprite {
  std::unique_ptr<rex::ui::ImmediateTexture> texture;
  float w = 0, h = 0;  // texture pixels
};

std::array<Sprite, size_t(Piece::kCount)> g_sprites;

struct Glyph {
  uint16_t page;
  int16_t x1, y1, x2, y2, pre, advance;
};
std::unordered_map<uint32_t, Glyph> g_glyphs;
std::vector<std::unique_ptr<rex::ui::ImmediateTexture>> g_pages;
std::vector<ImVec2> g_page_size;
float g_line = 28.0f;
std::unordered_map<uint32_t, std::string> g_common;
struct CpuImage {
  int w, h;
  std::vector<uint8_t> rgba;
};
std::unordered_map<ImTextureID, CpuImage> g_cpu;  // ds2_trophy_dump only

std::unique_ptr<rex::ui::ImmediateTexture> MakeTexture(rex::ui::ImmediateDrawer* drawer, int w, int h,
                                                       const uint8_t* rgba) {
  auto t = drawer->CreateTexture(uint32_t(w), uint32_t(h), rex::ui::ImmediateTextureFilter::kLinear, false, rgba);
  if (t && !REXCVAR_GET(ds2_trophy_dump).empty())
    g_cpu[reinterpret_cast<ImTextureID>(t.get())] = {w, h, std::vector<uint8_t>(rgba, rgba + size_t(w) * h * 4)};
  return t;
}

void MakeSprite(rex::ui::ImmediateDrawer* drawer, const Image& img, Piece piece, int x, int y, int w, int h) {
  if (img.rgba.empty() || x < 0 || y < 0 || x + w > img.w || y + h > img.h) return;
  std::vector<uint8_t> crop(size_t(w) * h * 4);
  for (int r = 0; r < h; ++r)
    std::memcpy(&crop[size_t(r) * w * 4], &img.rgba[(size_t(y + r) * img.w + x) * 4], size_t(w) * 4);
  Sprite& s = g_sprites[size_t(piece)];
  s.texture = MakeTexture(drawer, w, h, crop.data());
  s.w = float(w), s.h = float(h);
}

Image PageOf(const std::unordered_map<std::string, Bytes>& files, const char* name) {
  Image img;
  auto it = files.find(Lower(name));
  if (it == files.end() || !DecodeTpf(it->second, img)) REXLOG_WARN("[trophies] the game's {} not found", name);
  return img;
}

// UTF-16 big-endian text to UTF-8.
std::string Utf8(const uint8_t* p, const uint8_t* end) {
  std::string out;
  for (; p + 1 < end && (p[0] || p[1]); p += 2) {
    uint32_t c = Be16(p);
    if (c >= 0xD800 && c < 0xDC00 && p + 3 < end) {
      c = 0x10000 + ((c - 0xD800) << 10) + (Be16(p + 2) - 0xDC00);
      p += 2;
    }
    if (c < 0x80) {
      out += char(c);
    } else if (c < 0x800) {
      out += char(0xC0 | c >> 6), out += char(0x80 | (c & 63));
    } else if (c < 0x10000) {
      out += char(0xE0 | c >> 12), out += char(0x80 | ((c >> 6) & 63)), out += char(0x80 | (c & 63));
    } else {
      out += char(0xF0 | c >> 18), out += char(0x80 | ((c >> 12) & 63)), out += char(0x80 | ((c >> 6) & 63));
      out += char(0x80 | (c & 63));
    }
  }
  return out;
}

// An FMG (version 1, big-endian on the 360): its texts by id.
void ReadFmg(const Bytes& d, std::unordered_map<uint32_t, std::string>& out) {
  if (d.size() < 0x1C || d[9] != 0xFF) return;
  const uint32_t groups = Be32(&d[0x0C]), offsets = Be32(&d[0x14]);
  for (uint32_t g = 0; g < groups && 0x1C + g * 12 + 12 <= d.size(); ++g) {
    const uint8_t* e = &d[0x1C + g * 12];
    const int32_t index = int32_t(Be32(e)), first = int32_t(Be32(e + 4)), last = int32_t(Be32(e + 8));
    for (int32_t k = 0; k <= last - first && k < 100000; ++k) {
      const size_t at = offsets + size_t(index + k) * 4;
      if (at + 4 > d.size()) break;
      const uint32_t so = Be32(&d[at]);
      if (so && so < d.size()) out[uint32_t(first + k)] = Utf8(&d[so], d.data() + d.size());
    }
  }
}

// The folder names of the game's languages (XLanguage numbers, user_language).
const char* LanguageFolder(uint32_t language) {
  switch (language) {
    case 2: return "Japanese";
    case 3: return "Germany";
    case 4: return "French";
    case 5: return "Spanish";
    case 6: return "Italian";
    case 11: return "Polish";
    case 12: return "Russian";
    default: return "English";
  }
}

void LoadFont(rex::ui::ImmediateDrawer* drawer, rex::filesystem::VirtualFileSystem* vfs, const char* folder) {
  Bytes dcx, bnd;
  if (!ReadRange(vfs, std::string("game:\\font\\") + folder + "\\FeFont_Small.fontbnd.dcx", 0, UINT64_MAX, dcx) ||
      !UnDcx(dcx, bnd)) {
    REXLOG_WARN("[trophies] the game's font for {} not found: ImGui's font", folder);
    return;
  }
  const auto files = Bnd4Files(bnd);
  auto ccm = files.find("fefont_small.ccm");
  if (ccm == files.end() || ccm->second.size() < 0x20) return;
  const Bytes& c = ccm->second;
  g_line = float(Be16(&c[8]));
  const uint32_t count = Be32(&c[0x10]), table = Be32(&c[0x18]);
  uint16_t pages = 0;
  for (uint32_t i = 0; i < count && table + i * 24 + 16 <= c.size(); ++i) {
    const uint8_t* g = &c[table + i * 24];
    const uint32_t region = Be32(g + 4);
    if (region + 8 > c.size()) continue;
    const Glyph glyph{Be16(g + 8), int16_t(Be16(&c[region])), int16_t(Be16(&c[region + 2])),
                      int16_t(Be16(&c[region + 4])), int16_t(Be16(&c[region + 6])), int16_t(Be16(g + 10)),
                      int16_t(Be16(g + 14))};
    g_glyphs[Be32(g)] = glyph;
    pages = std::max<uint16_t>(pages, uint16_t(glyph.page + 1));
  }
  g_pages.resize(pages);
  g_page_size.assign(pages, ImVec2(1, 1));
  for (uint16_t p = 0; p < pages; ++p) {
    char name[48];
    std::snprintf(name, sizeof(name), p == 0 ? "fefont_small_%04u_x360.tpf" : "fefont_small_%04u.tpf", unsigned(p));
    auto it = files.find(name);
    Image img;
    if (it == files.end() || !DecodeTpf(it->second, img)) continue;
    g_pages[p] = MakeTexture(drawer, img.w, img.h, img.rgba.data());
    g_page_size[p] = ImVec2(float(img.w), float(img.h));
  }
  REXLOG_INFO("[trophies] the game's font ({}): {} glyphs on {} pages", folder, g_glyphs.size(), pages);
}

// The next code point of a UTF-8 string.
uint32_t Next(const std::string& s, size_t& i) {
  const uint8_t c = uint8_t(s[i++]);
  if (c < 0x80) return c;
  int n = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : 1;
  uint32_t cp = c & (0x3F >> n);
  while (n-- && i < s.size()) cp = cp << 6 | (uint8_t(s[i++]) & 0x3F);
  return cp;
}

ImU32 Alpha(ImU32 color, float a) {
  return (color & 0x00FFFFFFu) | (ImU32(float(color >> 24) * a) << 24);
}

}  // namespace

void Load(rex::Runtime* runtime, rex::ui::ImmediateDrawer* drawer, uint32_t language) {
  auto* vfs = runtime ? runtime->file_system() : nullptr;
  if (!vfs || !drawer) return;
  Bytes raw, bnd;
  if (ReadArchived(vfs, "menu\\40.febnd.dcx", raw) && UnDcx(raw, bnd)) {
    const auto files = Bnd4Files(bnd);
    const Image page = PageOf(files, "In-game_01.tpf");
    MakeSprite(drawer, page, Piece::kWindow, 15, 6, 880, 543);
    MakeSprite(drawer, page, Piece::kTrack, 0, 986, 348, 38);
    MakeSprite(drawer, page, Piece::kFill, 794, 684, 224, 36);
    const Image strip = PageOf(files, "In-game_02.tpf");
    MakeSprite(drawer, strip, Piece::kStrip, 0, 2, 788, 30);
  } else {
    REXLOG_WARN("[trophies] the game's menu\\40.febnd.dcx not read: plain shapes");
  }
  if (ReadArchived(vfs, "menu\\99.febnd.dcx", raw) && UnDcx(raw, bnd)) {
    const auto files = Bnd4Files(bnd);
    const Image waku2 = PageOf(files, "waku_02.tpf");
    MakeSprite(drawer, waku2, Piece::kNameStrip, 2, 4, 788, 36);
    MakeSprite(drawer, waku2, Piece::kRowBand, 182, 48, 632, 30);
    MakeSprite(drawer, waku2, Piece::kGlow, 4, 132, 780, 30);
    const Image icons = PageOf(files, "IconBase.tpf");
    MakeSprite(drawer, icons, Piece::kTile, 4, 10, 66, 74);
    MakeSprite(drawer, icons, Piece::kTileLit, 108, 10, 67, 75);
    const Image leather = PageOf(files, "ItemSellect_02.tpf");
    MakeSprite(drawer, leather, Piece::kLeather, 4, 382, 502, 104);
    const Image frame = PageOf(files, "waku.tpf");
    MakeSprite(drawer, frame, Piece::kFrameTop, 4, 0, 506, 54);
    MakeSprite(drawer, frame, Piece::kFrameBottom, 4, 82, 506, 44);
  } else {
    REXLOG_WARN("[trophies] the game's menu\\99.febnd.dcx not read: plain shapes");
  }
  const char* folder = LanguageFolder(language);
  LoadFont(drawer, vfs, folder);
  Bytes fmg_raw, fmg;
  if (ReadArchived(vfs, std::string("menu\\Text\\") + folder + "\\common.fmg", fmg_raw) && UnDcx(fmg_raw, fmg))
    ReadFmg(fmg, g_common);
  int pieces = 0;
  for (const Sprite& s : g_sprites) pieces += s.texture != nullptr;
  REXLOG_INFO("[trophies] the game's art: {} of {} pieces, {} common texts", pieces, int(Piece::kCount),
              g_common.size());
}

void Draw(ImDrawList* dl, Piece piece, ImVec2 a, ImVec2 b, ImU32 color, float cap) {
  const Sprite& s = g_sprites[size_t(piece)];
  if (!s.texture) {
    const ImU32 plain = piece == Piece::kGlow || piece == Piece::kFill || piece == Piece::kTileLit
                            ? IM_COL32(170, 90, 30, 200)
                        : piece == Piece::kStrip || piece == Piece::kNameStrip ? IM_COL32(30, 48, 60, 230)
                                                                                : IM_COL32(24, 22, 20, 235);
    if (piece != Piece::kFrameTop && piece != Piece::kFrameBottom) dl->AddRectFilled(a, b, Alpha(plain, (color >> 24) / 255.0f));
    return;
  }
  const ImTextureID tex = reinterpret_cast<ImTextureID>(s.texture.get());
  if (cap <= 0.0f || cap * 2 >= s.w) {
    dl->AddImage(tex, a, b, ImVec2(0, 0), ImVec2(1, 1), color);
    return;
  }
  const float px = (b.y - a.y) / s.h;  // screen pixels per texture pixel, from the height
  const float end = std::min(cap * px, (b.x - a.x) * 0.5f);
  const float u = cap / s.w;
  dl->AddImage(tex, a, ImVec2(a.x + end, b.y), ImVec2(0, 0), ImVec2(u, 1), color);
  dl->AddImage(tex, ImVec2(a.x + end, a.y), ImVec2(b.x - end, b.y), ImVec2(u, 0), ImVec2(1 - u, 1), color);
  dl->AddImage(tex, ImVec2(b.x - end, a.y), b, ImVec2(1 - u, 0), ImVec2(1, 1), color);
}

float TextWidth(const std::string& utf8, float scale) {
  if (g_glyphs.empty()) return ImGui::CalcTextSize(utf8.c_str()).x * (g_line * scale / ImGui::GetFontSize());
  float w = 0.0f;
  for (size_t i = 0; i < utf8.size();) {
    auto it = g_glyphs.find(Next(utf8, i));
    if (it != g_glyphs.end()) w += float(it->second.advance);
  }
  return w * scale;
}

void Text(ImDrawList* dl, ImVec2 pos, const std::string& utf8, float scale, ImU32 color, bool shadow) {
  if (g_glyphs.empty()) {
    const float size = g_line * scale * 0.72f;
    if (shadow) dl->AddText(nullptr, size, ImVec2(pos.x + scale * 1.5f, pos.y + scale * 1.5f), Alpha(IM_COL32_BLACK, (color >> 24) / 320.0f), utf8.c_str());
    dl->AddText(nullptr, size, pos, color, utf8.c_str());
    return;
  }
  const float a = float(color >> 24) / 255.0f;
  for (int pass = shadow ? 0 : 1; pass < 2; ++pass) {
    const float off = pass == 0 ? 1.5f * scale + 0.5f : 0.0f;
    float x = pos.x + off;
    for (size_t i = 0; i < utf8.size();) {
      auto it = g_glyphs.find(Next(utf8, i));
      if (it == g_glyphs.end()) continue;
      const Glyph& g = it->second;
      if (g.page < g_pages.size() && g_pages[g.page] && g.x2 > g.x1) {
        const ImVec2 size = g_page_size[g.page];
        const ImVec2 p0(x + g.pre * scale, pos.y + off), p1(p0.x + (g.x2 - g.x1) * scale, p0.y + (g.y2 - g.y1) * scale);
        // The button pictures (page 0) keep their colours; the letters take the text's.
        const ImU32 c = pass == 0 ? Alpha(IM_COL32_BLACK, a * 0.8f) : g.page == 0 ? Alpha(IM_COL32_WHITE, a) : color;
        dl->AddImage(reinterpret_cast<ImTextureID>(g_pages[g.page].get()), p0, p1,
                     ImVec2(g.x1 / size.x, g.y1 / size.y), ImVec2(g.x2 / size.x, g.y2 / size.y), c);
      }
      x += g.advance * scale;
    }
  }
}

int TextWrapped(ImDrawList* dl, ImVec2 pos, const std::string& utf8, float scale, ImU32 color, float width,
                float line_step) {
  int lines = 0;
  std::string line;
  size_t i = 0;
  while (i <= utf8.size()) {
    size_t j = utf8.find(' ', i);
    if (j == std::string::npos) j = utf8.size();
    const std::string word = utf8.substr(i, j - i);
    const std::string candidate = line.empty() ? word : line + " " + word;
    if (!line.empty() && TextWidth(candidate, scale) > width) {
      Text(dl, ImVec2(pos.x, pos.y + lines * line_step), line, scale, color);
      ++lines;
      line = word;
    } else {
      line = candidate;
    }
    i = j + 1;
  }
  if (!line.empty()) {
    Text(dl, ImVec2(pos.x, pos.y + lines * line_step), line, scale, color);
    ++lines;
  }
  return lines;
}

bool CpuPixels(ImTextureID id, int& w, int& h, const uint8_t*& rgba) {
  auto it = g_cpu.find(id);
  if (it == g_cpu.end()) return false;
  w = it->second.w, h = it->second.h, rgba = it->second.rgba.data();
  return true;
}

std::string Message(uint32_t id) {
  auto it = g_common.find(id);
  return it == g_common.end() ? std::string() : it->second;
}

}  // namespace ds2::trophy_art
