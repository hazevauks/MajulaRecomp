#pragma once

namespace rex {
class Runtime;
}

// Dark Souls II: the button pictures of the game's prompts in the Nintendo style (docs/button-icons.md).
namespace ds2::button_icons {

// From OnPostSetup, before the game starts: rebuilds the font's button page from the user's copy (once; then from the
// cache next to the program) and points the game's font folders at the rebuilt ones. Does nothing with
// ds2_button_icons = xbox (the PC's default).
void Install(rex::Runtime* runtime);

}  // namespace ds2::button_icons
