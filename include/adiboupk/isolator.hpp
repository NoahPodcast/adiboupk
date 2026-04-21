#pragma once

#include "adiboupk/config.hpp"
#include <filesystem>
#include <string>

namespace adiboupk {
namespace isolator {

namespace fs = std::filesystem;

// Install each package from a requirements.txt into its own --target directory.
// Creates <deps_dir>/<package>/ for each package.
// Generates <deps_dir>/package_map.json for the import hook.
bool install_isolated(const fs::path& deps_dir,
                      const fs::path& requirements_path,
                      const std::string& python_cmd);

// Ensure the Python import hook and runner script exist in deps_dir.
// Written once, reused on subsequent runs.
void ensure_runtime_files(const fs::path& deps_dir);

// Get the deps directory for a group
fs::path deps_dir_for(const Config& cfg, const Group& group);

// Get the package_map.json path for a group
fs::path package_map_for(const Config& cfg, const Group& group);

// Check if isolated deps exist for a group
bool exists(const Config& cfg, const Group& group);

// Clean isolated deps for a group (remove the entire deps directory)
bool clean(const Config& cfg, const Group& group);

} // namespace isolator
} // namespace adiboupk
