#pragma once

#include "adiboupk/config.hpp"
#include <filesystem>
#include <vector>

namespace adiboupk {
namespace discovery {

namespace fs = std::filesystem;

// Scan project_root for directories containing requirements.txt.
// Only scans one level deep (immediate subdirectories).
// Ignores hidden directories, .venvs, node_modules, __pycache__.
std::vector<Group> scan(const fs::path& project_root);

// Find which group a given script belongs to, based on its path.
// Returns nullptr if no group matches.
const Group* find_group_for_script(const std::vector<Group>& groups,
                                    const fs::path& script_path);

} // namespace discovery
} // namespace adiboupk
