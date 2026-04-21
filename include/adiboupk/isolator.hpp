#pragma once

#include "adiboupk/config.hpp"
#include <filesystem>
#include <map>
#include <string>

namespace adiboupk {
namespace isolator {

namespace fs = std::filesystem;

// Install each package from a requirements.txt into its own --target directory.
// Creates .deps/<group>/<package>/ for each package.
// Generates .deps/<group>/package_map.json for the import hook.
// Returns true on success.
bool install_isolated(const fs::path& deps_dir,
                      const fs::path& requirements_path,
                      const std::string& python_cmd);

// Get the deps directory for a group
fs::path deps_dir_for(const Config& cfg, const Group& group);

// Get the package_map.json path for a group
fs::path package_map_for(const Config& cfg, const Group& group);

// Check if isolated deps exist for a group
bool exists(const Config& cfg, const Group& group);

} // namespace isolator
} // namespace adiboupk
