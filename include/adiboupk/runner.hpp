#pragma once

#include "adiboupk/config.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace adiboupk {
namespace runner {

namespace fs = std::filesystem;

// Run a Python script using the correct venv for its group.
// This replaces the current process (exec) — does not return on success.
// script_path: path to the .py file
// script_args: arguments to pass to the script
// cfg: loaded project config
// Returns exit code only on failure (exec_replace doesn't return on success).
[[noreturn]] void run(const fs::path& script_path,
                      const std::vector<std::string>& script_args,
                      const Config& cfg);

// Resolve which python binary should be used for a given script.
// Returns empty path if no matching group found.
fs::path resolve_python(const fs::path& script_path, const Config& cfg);

} // namespace runner
} // namespace adiboupk
