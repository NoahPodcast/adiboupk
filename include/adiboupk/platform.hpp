#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace adiboupk {
namespace platform {

namespace fs = std::filesystem;

// Returns the Python executable name for this platform ("python3" on Linux, "python" on Windows)
std::string python_command();

// Returns the path to the python binary inside a venv
fs::path venv_python(const fs::path& venv_dir);

// Returns the path to the pip binary inside a venv
fs::path venv_pip(const fs::path& venv_dir);

// Execute a command and wait for completion. Returns exit code.
int run_process(const std::string& executable,
                const std::vector<std::string>& args,
                bool capture_output = false,
                std::string* stdout_buf = nullptr);

// Replace current process with the given command (execvp on Linux, CreateProcess+exit on Windows).
// Does not return on success.
[[noreturn]] void exec_replace(const std::string& executable,
                               const std::vector<std::string>& args);

// Recursively remove a directory, handling platform quirks (read-only files on Windows)
bool remove_directory(const fs::path& dir);

// Get an environment variable value. Returns empty string if not set.
std::string get_env(const std::string& name);

// Normalize a path for consistent comparison
fs::path normalize_path(const fs::path& p);

// Check if a path is under a given base directory
bool is_under(const fs::path& path, const fs::path& base);

} // namespace platform
} // namespace adiboupk
