#include "adiboupk/auditor.hpp"
#include "adiboupk/cli.hpp"
#include "adiboupk/config.hpp"
#include "adiboupk/discovery.hpp"
#include "adiboupk/installer.hpp"
#include "adiboupk/platform.hpp"
#include "adiboupk/runner.hpp"
#include "adiboupk/venv.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

static fs::path resolve_root(const adiboupk::cli::ParsedArgs& args) {
    if (!args.project_root.empty()) {
        return fs::absolute(args.project_root);
    }
    return fs::current_path();
}

static int cmd_init(const adiboupk::cli::ParsedArgs& args) {
    auto root = resolve_root(args);
    std::cout << "Scanning " << root.string() << " for module groups..." << std::endl;

    auto groups = adiboupk::discovery::scan(root);
    if (groups.empty()) {
        std::cerr << "No directories with requirements.txt found." << std::endl;
        return 1;
    }

    std::cout << "Found " << groups.size() << " group(s):" << std::endl;
    for (const auto& g : groups) {
        std::cout << "  - " << g.name << " (" << g.requirements_path.string() << ")" << std::endl;
    }

    auto cfg = adiboupk::config::init(root, groups);
    if (!adiboupk::config::save(cfg)) {
        std::cerr << "Failed to write " << adiboupk::config::CONFIG_FILE << std::endl;
        return 1;
    }

    std::cout << "Created " << adiboupk::config::CONFIG_FILE << std::endl;
    return 0;
}

static int cmd_install(const adiboupk::cli::ParsedArgs& args) {
    auto root = resolve_root(args);
    auto cfg = adiboupk::config::load(root);

    if (cfg.groups.empty()) {
        std::cerr << "No groups configured. Run 'adiboupk init' first." << std::endl;
        return 1;
    }

    auto lock = adiboupk::config::load_lock(root);

    // If --force, clear the lock so everything reinstalls
    if (args.force) {
        lock.entries.clear();
    }

    std::cout << "Installing dependencies for " << cfg.groups.size() << " group(s)..." << std::endl;

    int failures = adiboupk::installer::install_all(cfg, lock);

    if (!adiboupk::config::save_lock(lock, root)) {
        std::cerr << "Warning: failed to write lock file" << std::endl;
    }

    if (failures > 0) {
        std::cerr << failures << " group(s) failed to install." << std::endl;
        return 1;
    }

    std::cout << "All groups installed successfully." << std::endl;
    return 0;
}

static int cmd_setup(const adiboupk::cli::ParsedArgs& args) {
    auto root = resolve_root(args);

    // Step 1: Init — scan and create config
    std::cout << "==> Scanning " << root.string() << " for module groups..." << std::endl;

    auto groups = adiboupk::discovery::scan(root);
    if (groups.empty()) {
        std::cerr << "No directories with requirements.txt found." << std::endl;
        return 1;
    }

    std::cout << "Found " << groups.size() << " group(s):" << std::endl;
    for (const auto& g : groups) {
        std::cout << "  - " << g.name << " (" << g.requirements_path.string() << ")" << std::endl;
    }

    auto cfg = adiboupk::config::init(root, groups);
    if (!adiboupk::config::save(cfg)) {
        std::cerr << "Failed to write " << adiboupk::config::CONFIG_FILE << std::endl;
        return 1;
    }
    std::cout << "Created " << adiboupk::config::CONFIG_FILE << std::endl;
    std::cout << std::endl;

    // Step 2: Install — create venvs and install dependencies
    std::cout << "==> Installing dependencies..." << std::endl;

    // Reload config to pick up computed hashes
    cfg = adiboupk::config::load(root);
    auto lock = adiboupk::config::load_lock(root);

    if (args.force) {
        lock.entries.clear();
    }

    int failures = adiboupk::installer::install_all(cfg, lock);

    if (!adiboupk::config::save_lock(lock, root)) {
        std::cerr << "Warning: failed to write lock file" << std::endl;
    }

    if (failures > 0) {
        std::cerr << failures << " group(s) failed to install." << std::endl;
        return 1;
    }
    std::cout << std::endl;

    // Step 3: Audit — check for conflicts
    std::cout << "==> Auditing for cross-group conflicts..." << std::endl;

    auto conflicts = adiboupk::auditor::audit(cfg.groups);
    std::cout << adiboupk::auditor::format_conflicts(conflicts);

    std::cout << "Setup complete. Use 'adiboupk run <script.py>' to execute scripts." << std::endl;
    return 0;
}

static int cmd_run(const adiboupk::cli::ParsedArgs& args) {
    if (args.script_path.empty()) {
        std::cerr << "Usage: adiboupk run <script.py> [args...]" << std::endl;
        return 1;
    }

    auto root = resolve_root(args);
    auto cfg = adiboupk::config::load(root);

    if (cfg.groups.empty()) {
        std::cerr << "No groups configured. Run 'adiboupk init' first." << std::endl;
        return 1;
    }

    fs::path script = fs::absolute(args.script_path);
    if (!fs::exists(script)) {
        std::cerr << "Script not found: " << args.script_path << std::endl;
        return 1;
    }

    // This does not return on success
    adiboupk::runner::run(script, args.script_args, cfg);
    // Unreachable if exec_replace succeeds
}

static int cmd_audit(const adiboupk::cli::ParsedArgs& args) {
    auto root = resolve_root(args);
    auto cfg = adiboupk::config::load(root);

    if (cfg.groups.empty()) {
        std::cerr << "No groups configured. Run 'adiboupk init' first." << std::endl;
        return 1;
    }

    auto conflicts = adiboupk::auditor::audit(cfg.groups);
    std::cout << adiboupk::auditor::format_conflicts(conflicts);

    return conflicts.empty() ? 0 : 1;
}

static int cmd_status(const adiboupk::cli::ParsedArgs& args) {
    auto root = resolve_root(args);
    auto cfg = adiboupk::config::load(root);
    auto lock = adiboupk::config::load_lock(root);

    if (cfg.groups.empty()) {
        std::cout << "No groups configured. Run 'adiboupk init' first." << std::endl;
        return 0;
    }

    std::cout << "Project: " << root.string() << std::endl;
    std::cout << "Venvs:   " << cfg.venvs_dir.string() << std::endl;
    std::cout << std::endl;

    for (const auto& g : cfg.groups) {
        auto vdir = adiboupk::venv::venv_dir_for(cfg, g);
        bool venv_ok = adiboupk::venv::exists(vdir);
        bool needs = adiboupk::config::needs_install(g, lock);

        std::cout << "  " << g.name << std::endl;
        std::cout << "    Directory:    " << g.directory.string() << std::endl;
        std::cout << "    Requirements: " << g.requirements_path.string() << std::endl;
        std::cout << "    Hash:         " << g.requirements_hash.substr(0, 12) << "..." << std::endl;
        std::cout << "    Venv:         " << (venv_ok ? "OK" : "MISSING") << std::endl;
        std::cout << "    Status:       " << (needs ? "NEEDS INSTALL" : "UP TO DATE") << std::endl;
        std::cout << std::endl;
    }

    return 0;
}

static int cmd_clean(const adiboupk::cli::ParsedArgs& args) {
    auto root = resolve_root(args);
    auto cfg = adiboupk::config::load(root);

    std::cout << "Removing all venvs in " << cfg.venvs_dir.string() << "..." << std::endl;

    int removed = 0;
    for (const auto& g : cfg.groups) {
        auto vdir = adiboupk::venv::venv_dir_for(cfg, g);
        if (adiboupk::venv::destroy(vdir)) {
            std::cout << "  Removed " << g.name << std::endl;
            removed++;
        }
    }

    // Also remove the .venvs directory if empty
    std::error_code ec;
    fs::remove(cfg.venvs_dir, ec);

    // Clear lock file
    adiboupk::LockFile empty_lock;
    adiboupk::config::save_lock(empty_lock, root);

    std::cout << "Cleaned " << removed << " venv(s)." << std::endl;
    return 0;
}

static int cmd_which(const adiboupk::cli::ParsedArgs& args) {
    if (args.script_path.empty()) {
        std::cerr << "Usage: adiboupk which <script.py>" << std::endl;
        return 1;
    }

    auto root = resolve_root(args);
    auto cfg = adiboupk::config::load(root);

    if (cfg.groups.empty()) {
        std::cerr << "No groups configured. Run 'adiboupk init' first." << std::endl;
        return 1;
    }

    fs::path script = fs::absolute(args.script_path);
    auto python = adiboupk::runner::resolve_python(script, cfg);

    if (python.empty()) {
        std::string sys_python = cfg.python_path.empty()
            ? adiboupk::platform::python_command()
            : cfg.python_path;
        std::cout << sys_python << " (system — no matching group)" << std::endl;
        return 1;
    }

    // Find the group name for display
    const auto* group = adiboupk::discovery::find_group_for_script(cfg.groups, script);
    std::cout << python.string();
    if (group) {
        std::cout << " (group: " << group->name << ")";
    }
    std::cout << std::endl;

    return 0;
}

int main(int argc, char* argv[]) {
    auto args = adiboupk::cli::parse(argc, argv);

    switch (args.command) {
        case adiboupk::cli::Command::INIT:    return cmd_init(args);
        case adiboupk::cli::Command::INSTALL: return cmd_install(args);
        case adiboupk::cli::Command::SETUP:   return cmd_setup(args);
        case adiboupk::cli::Command::RUN:     return cmd_run(args);
        case adiboupk::cli::Command::AUDIT:   return cmd_audit(args);
        case adiboupk::cli::Command::STATUS:  return cmd_status(args);
        case adiboupk::cli::Command::CLEAN:   return cmd_clean(args);
        case adiboupk::cli::Command::WHICH:   return cmd_which(args);
        case adiboupk::cli::Command::VERSION:
            adiboupk::cli::print_version();
            return 0;
        case adiboupk::cli::Command::HELP:
            adiboupk::cli::print_help();
            return 0;
        case adiboupk::cli::Command::UNKNOWN:
            std::cerr << "Unknown command. Run 'adiboupk help' for usage." << std::endl;
            return 1;
    }

    return 0;
}
