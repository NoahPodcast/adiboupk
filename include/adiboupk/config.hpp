#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace adiboupk {

namespace fs = std::filesystem;

// A group of modules that share a venv
struct Group {
    std::string name;                    // e.g., "Enrichments" or "Enrichments/vt"
    fs::path directory;                  // e.g., "./Enrichments"
    fs::path requirements_path;          // e.g., "./Enrichments/requirements.txt"
    std::string requirements_hash;       // SHA-256 of requirements.txt content
    std::vector<std::string> scripts;    // Explicit script mappings (optional)
                                         // e.g., ["script_vt.py", "script_vt2.py"]
                                         // If empty, all scripts in directory use this group
};

// Project configuration (adiboupk.json)
struct Config {
    fs::path project_root;               // Root directory of the project
    fs::path venvs_dir;                  // Where venvs are stored (default: .venvs)
    std::vector<Group> groups;           // Defined groups
    std::string python_path;             // Custom python path (optional)
};

// Lock file state (adiboupk.lock)
struct LockEntry {
    std::string name;
    std::string requirements_hash;       // Hash when last installed
    bool installed = false;
};

struct LockFile {
    std::map<std::string, LockEntry> entries;
};

namespace config {

// Default config file name
constexpr const char* CONFIG_FILE = "adiboupk.json";
constexpr const char* LOCK_FILE = "adiboupk.lock";
constexpr const char* DEFAULT_VENVS_DIR = ".venvs";

// Load config from file. Returns default config if file doesn't exist.
Config load(const fs::path& project_root);

// Save config to file.
bool save(const Config& cfg);

// Load lock file.
LockFile load_lock(const fs::path& project_root);

// Save lock file.
bool save_lock(const LockFile& lock, const fs::path& project_root);

// Check if a group needs reinstallation (hash mismatch with lock)
bool needs_install(const Group& group, const LockFile& lock);

// Initialize a default config with auto-discovered groups
Config init(const fs::path& project_root, const std::vector<Group>& groups);

} // namespace config
} // namespace adiboupk
