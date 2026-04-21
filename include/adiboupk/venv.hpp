#pragma once

#include "adiboupk/config.hpp"
#include <filesystem>
#include <string>

namespace adiboupk {
namespace venv {

namespace fs = std::filesystem;

// Create a virtual environment for a group.
// venv_dir is typically: config.venvs_dir / group.name
// python_cmd is the python executable to use (from config or platform default).
// Returns true on success.
bool create(const fs::path& venv_dir, const std::string& python_cmd);

// Check if a venv exists and has a valid python binary.
bool exists(const fs::path& venv_dir);

// Destroy a venv directory.
bool destroy(const fs::path& venv_dir);

// Get the venv directory for a group given the config.
fs::path venv_dir_for(const Config& cfg, const Group& group);

} // namespace venv
} // namespace adiboupk
