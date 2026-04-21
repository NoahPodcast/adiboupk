#include "adiboupk/installer.hpp"
#include "adiboupk/platform.hpp"
#include "adiboupk/venv.hpp"

#include <iostream>

namespace adiboupk {
namespace installer {

bool install(const fs::path& venv_dir, const fs::path& requirements_path) {
    auto pip = platform::venv_pip(venv_dir);
    if (!fs::exists(pip)) {
        std::cerr << "adiboupk: pip not found at " << pip.string() << std::endl;
        return false;
    }

    std::vector<std::string> args = {
        "install", "-r", requirements_path.string(), "--quiet"
    };

    int rc = platform::run_process(pip.string(), args);
    if (rc != 0) {
        std::cerr << "adiboupk: pip install failed for "
                  << requirements_path.string()
                  << " (exit code " << rc << ")" << std::endl;
        return false;
    }

    return true;
}

bool upgrade_pip(const fs::path& venv_dir) {
    auto pip = platform::venv_pip(venv_dir);
    if (!fs::exists(pip)) return false;

    std::vector<std::string> args = {
        "install", "--upgrade", "pip", "--quiet"
    };

    int rc = platform::run_process(pip.string(), args);
    return rc == 0;
}

int install_all(Config& cfg, LockFile& lock) {
    int failures = 0;
    std::string python_cmd = cfg.python_path.empty()
        ? platform::python_command()
        : cfg.python_path;

    for (auto& group : cfg.groups) {
        if (!config::needs_install(group, lock)) {
            std::cout << "  [skip] " << group.name << " (up to date)" << std::endl;
            continue;
        }

        std::cout << "  [install] " << group.name << std::endl;

        auto vdir = venv::venv_dir_for(cfg, group);

        // Create venv if it doesn't exist
        if (!venv::exists(vdir)) {
            std::cout << "    Creating venv..." << std::endl;
            if (!venv::create(vdir, python_cmd)) {
                std::cerr << "    Failed to create venv for " << group.name << std::endl;
                failures++;
                continue;
            }
        }

        // Install dependencies
        std::cout << "    Installing dependencies..." << std::endl;
        if (!install(vdir, group.requirements_path)) {
            std::cerr << "    Failed to install dependencies for " << group.name << std::endl;
            failures++;
            continue;
        }

        // Update lock
        LockEntry entry;
        entry.name = group.name;
        entry.requirements_hash = group.requirements_hash;
        entry.installed = true;
        lock.entries[group.name] = entry;

        std::cout << "    Done." << std::endl;
    }

    return failures;
}

} // namespace installer
} // namespace adiboupk
