// Dark Souls II: the game's own art for the trophy screen and the unlock notice (docs/trophies.md): pieces of its menu
// pages (the window, the strips, the selection glow, the item tiles, the dialog frame) and its font (FeFont, the
// port's copy with the Nintendo button pictures), read from the player's files at start. Nothing is shipped.
#pragma once

#include <cstdint>
#include <string>

#include <imgui.h>

namespace rex {
class Runtime;
namespace ui {
class ImmediateDrawer;
}  // namespace ui
}  // namespace rex

namespace ds2::trophy_art {

enum class Piece {
  kWindow,       // the menu window: leather, gold border, blue title strip (In-game_01)
  kStrip,        // a blue sub-header strip ("Attributes" in Player Status; In-game_02)
  kNameStrip,    // the strip of a description box, a slot for the icon at its left (waku_02)
  kRowBand,      // a list row's dark band, fading to the right (waku_02)
  kGlow,         // the selected row's orange glow (waku_02)
  kTrack,        // a framed dark bar (a gauge's track; In-game_01)
  kFill,         // the orange gauge (In-game_01)
  kTile,         // an item tile (IconBase)
  kTileLit,      // the selected item tile, orange
  kLeather,      // a leather strip (the item windows' panels; ItemSellect_02)
  kFrameTop,     // the dialog frame's top line with its ornament (waku)
  kFrameBottom,  // and its bottom line
  kCount
};

// Once, on the UI thread, after the button pictures are installed (the font is theirs): reads the game's menu pages
// and its font and common texts for the language (user_language). The pieces missing draw as plain shapes.
void Load(rex::Runtime* runtime, rex::ui::ImmediateDrawer* drawer, uint32_t language);

// Draws a piece stretched over [a, b]; with cap > 0, its ends keep cap texture pixels each (scaled to the height).
void Draw(ImDrawList* dl, Piece piece, ImVec2 a, ImVec2 b, ImU32 color = IM_COL32_WHITE, float cap = 0.0f);

// The game's font. 'scale' 1 is its own size (28 pixels a line); 'pos' is the line's top left. The button pictures
// keep their colours (the colour's alpha applies). Without the font, ImGui's.
float TextWidth(const std::string& utf8, float scale);
void Text(ImDrawList* dl, ImVec2 pos, const std::string& utf8, float scale, ImU32 color, bool shadow = true);
// Text broken into lines no wider than 'width'; returns the number of lines.
int TextWrapped(ImDrawList* dl, ImVec2 pos, const std::string& utf8, float scale, ImU32 color, float width,
                float line_step);

// PC tests (ds2_trophy_dump): the RGBA of the textures this module made, by ImGui texture id; false for others.
bool CpuPixels(ImTextureID id, int& w, int& h, const uint8_t*& rgba);

// A text of the game's common.fmg in the game's language (the key guides: 10000 "⑤：Select", 10002 "⑳：Back"), or "".
std::string Message(uint32_t id);

}  // namespace ds2::trophy_art
