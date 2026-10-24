// Dark Souls II: the port's gameplay options (docs/features.md): "Early Dull Ember" and "Better fog walls", two rows
// at the end of the game's Screen Options page (gear tab), kept in ds2.toml.

#pragma once

#include <filesystem>

namespace ds2::features {

// The config file the menu rows write (ds2.toml next to the executable). Called once while the paths are set up.
void SetConfigPath(const std::filesystem::path& path);

}  // namespace ds2::features
