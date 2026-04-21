#pragma once

#include "adiboupk/config.hpp"
#include <filesystem>
#include <string>

namespace adiboupk {
namespace installer {

namespace fs = std::filesystem;

// Install dependencies from requirements.txt into a venv.
// Returns true on success.
bool install(const fs::path& venv_dir, const fs::path& requirements_path);

// Upgrade pip in a venv (optional, called before install).
bool upgrade_pip(const fs::path& venv_dir);

// Install all groups that need installation (hash mismatch with lock).
// Returns number of groups that failed.
int install_all(Config& cfg, LockFile& lock);

} // namespace installer
} // namespace adiboupk
