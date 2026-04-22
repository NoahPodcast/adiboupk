#include "adiboupk/auditor.hpp"
#include "adiboupk/cli.hpp"
#include "adiboupk/config.hpp"
#include "adiboupk/discovery.hpp"
#include "adiboupk/installer.hpp"
#include "adiboupk/isolator.hpp"
#include "adiboupk/platform.hpp"
#include "adiboupk/runner.hpp"
#include "adiboupk/venv.hpp"

#include "nlohmann/json.hpp"

#include <filesystem>
#include <iostream>
#include <set>

#ifdef _WIN32
#include <windows.h>
#endif

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

// Shared: fetch current and latest version info
struct VersionCheck {
    std::string current;       // e.g., "1.0.0" or "1.0.0-3-gabcdef"
    std::string current_base;  // e.g., "1.0.0" (stripped of commit info)
    std::string latest;        // e.g., "1.1.0"
    bool is_dev_build = false; // true if current has commit info after version
    bool up_to_date = false;
};

static bool check_version(VersionCheck& vc) {
#ifdef ADIBOUPK_VERSION
    vc.current = ADIBOUPK_VERSION;
#else
    vc.current = "unknown";
#endif

    // Fetch latest version from GitHub API
    // Try curl first, then wget as fallback
    std::string api_output;
    std::string url = "https://api.github.com/repos/NoahPodcast/adiboupk/tags?per_page=1";

    int rc = adiboupk::platform::run_process(
        "/usr/bin/curl",
        {"-sSL", "--max-time", "10",
         "-H", "Accept: application/vnd.github.v3+json", url},
        true, &api_output
    );

    if (rc != 0 || api_output.empty()) {
        // Try curl without absolute path
        api_output.clear();
        rc = adiboupk::platform::run_process(
            "curl",
            {"-sSL", "--max-time", "10",
             "-H", "Accept: application/vnd.github.v3+json", url},
            true, &api_output
        );
    }

    if (rc != 0 || api_output.empty()) {
        // Try wget as fallback
        api_output.clear();
        rc = adiboupk::platform::run_process(
            "wget", {"-qO-", "--timeout=10", url},
            true, &api_output
        );
    }

    if (rc != 0 || api_output.empty()) {
        return false;
    }

    try {
        auto j = nlohmann::json::parse(api_output);
        if (j.is_array() && !j.empty() && j[0].contains("name")) {
            vc.latest = j[0]["name"].get<std::string>();
            if (!vc.latest.empty() && vc.latest[0] == 'v') {
                vc.latest = vc.latest.substr(1);
            }
        }
    } catch (...) {
        return false;
    }

    if (vc.latest.empty()) return false;

    // Strip commit info for comparison (e.g., "1.0.0-3-gabcdef" -> "1.0.0")
    vc.current_base = vc.current;
    auto dash_pos = vc.current_base.find('-');
    if (dash_pos != std::string::npos) {
        vc.current_base = vc.current_base.substr(0, dash_pos);
        vc.is_dev_build = true;
    }

    vc.up_to_date = (vc.current_base == vc.latest && !vc.is_dev_build);
    return true;
}

static int cmd_upgrade(const adiboupk::cli::ParsedArgs& args) {
    (void)args;

    std::cout << "==> Checking for updates..." << std::endl;

    VersionCheck vc;
    if (!check_version(vc)) {
        std::cerr << "Failed to check for updates (no network or curl unavailable)." << std::endl;
        std::cerr << "Manual upgrade: curl -sSL https://raw.githubusercontent.com/NoahPodcast/adiboupk/main/install.sh | bash" << std::endl;
        return 1;
    }

    std::cout << "  Installed: " << vc.current << std::endl;
    std::cout << "  Latest:    " << vc.latest << std::endl;

    if (vc.up_to_date) {
        std::cout << std::endl << "Already up to date." << std::endl;
        return 0;
    }

    if (vc.is_dev_build) {
        std::cout << std::endl << "You are running a dev build (" << vc.current
                  << "), latest release is " << vc.latest << "." << std::endl;
    } else {
        std::cout << std::endl << "A new version is available: "
                  << vc.current_base << " -> " << vc.latest << std::endl;
    }

    // If --force, skip confirmation
    if (!args.force) {
        std::cout << std::endl << "Upgrade to " << vc.latest << "? [y/N] ";
        std::cout.flush();
        std::string answer;
        std::getline(std::cin, answer);
        if (answer.empty() || (answer[0] != 'y' && answer[0] != 'Y')) {
            std::cout << "Upgrade cancelled." << std::endl;
            return 0;
        }
    }

    std::cout << std::endl << "==> Upgrading to " << vc.latest << "..." << std::endl;

    // Find where the current binary is installed
    std::error_code ec;
    auto self_path = fs::read_symlink("/proc/self/exe", ec);
    if (self_path.empty()) {
        std::string which_output;
        adiboupk::platform::run_process("which", {"adiboupk"}, true, &which_output);
        if (!which_output.empty()) {
            while (!which_output.empty() && (which_output.back() == '\n' || which_output.back() == '\r'))
                which_output.pop_back();
            self_path = which_output;
        }
    }

    std::string install_dir;
    if (!self_path.empty()) {
        install_dir = self_path.parent_path().string();
    }
    if (install_dir.empty()) {
        install_dir = "/usr/local/bin";
    }

    // Clone
    std::string tmp_output;
    adiboupk::platform::run_process("mktemp", {"-d"}, true, &tmp_output);
    while (!tmp_output.empty() && (tmp_output.back() == '\n' || tmp_output.back() == '\r'))
        tmp_output.pop_back();
    std::string tmp_dir = tmp_output.empty() ? "/tmp/adiboupk-upgrade" : tmp_output;

    // Clone the tagged release, fall back to main if tag doesn't exist
    std::cout << "  Downloading v" << vc.latest << "..." << std::endl;
    int rc = adiboupk::platform::run_process(
        "git", {"clone", "--depth", "1", "--branch", "v" + vc.latest,
                "https://github.com/NoahPodcast/adiboupk.git", tmp_dir}
    );
    if (rc != 0) {
        adiboupk::platform::run_process("rm", {"-rf", tmp_dir});
        adiboupk::platform::run_process("mktemp", {"-d"}, true, &tmp_output);
        while (!tmp_output.empty() && (tmp_output.back() == '\n' || tmp_output.back() == '\r'))
            tmp_output.pop_back();
        tmp_dir = tmp_output.empty() ? "/tmp/adiboupk-upgrade" : tmp_output;

        std::cout << "  Tag not found, using main branch..." << std::endl;
        rc = adiboupk::platform::run_process(
            "git", {"clone", "--depth", "1",
                    "https://github.com/NoahPodcast/adiboupk.git", tmp_dir}
        );
        if (rc != 0) {
            std::cerr << "Failed to clone repository." << std::endl;
            return 1;
        }
    }

    // Build
    std::cout << "  Building..." << std::endl;
    rc = adiboupk::platform::run_process(
        "cmake", {"-B", tmp_dir + "/build", "-S", tmp_dir,
                  "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_TESTS=OFF"}
    );
    if (rc != 0) {
        std::cerr << "Configure failed." << std::endl;
        adiboupk::platform::run_process("rm", {"-rf", tmp_dir});
        return 1;
    }

    rc = adiboupk::platform::run_process(
        "cmake", {"--build", tmp_dir + "/build", "--config", "Release"}
    );
    if (rc != 0) {
        std::cerr << "Build failed." << std::endl;
        adiboupk::platform::run_process("rm", {"-rf", tmp_dir});
        return 1;
    }

    // Install (copy+rename to avoid "Text file busy")
    std::cout << "  Installing to " << install_dir << "..." << std::endl;
    std::string new_binary = tmp_dir + "/build/adiboupk";
    std::string dest = install_dir + "/adiboupk";
    std::string dest_tmp = dest + ".new";

    auto try_install = [&](bool use_sudo) -> bool {
        std::vector<std::string> cp_args = {new_binary, dest_tmp};
        std::vector<std::string> mv_args = {dest_tmp, dest};
        std::vector<std::string> chmod_args = {"+x", dest};

        if (use_sudo) {
            cp_args.insert(cp_args.begin(), "cp");
            mv_args.insert(mv_args.begin(), "mv");
            chmod_args.insert(chmod_args.begin(), "chmod");
            return adiboupk::platform::run_process("sudo", cp_args) == 0
                && adiboupk::platform::run_process("sudo", mv_args) == 0
                && adiboupk::platform::run_process("sudo", chmod_args) == 0;
        } else {
            return adiboupk::platform::run_process("cp", cp_args) == 0
                && adiboupk::platform::run_process("mv", mv_args) == 0
                && adiboupk::platform::run_process("chmod", chmod_args) == 0;
        }
    };

    if (!try_install(false) && !try_install(true)) {
        std::cerr << "Failed to install new binary to " << dest << std::endl;
        std::cerr << "Try manually: sudo cp " << new_binary << " " << dest << std::endl;
        // Don't clean up so user can copy manually
        return 1;
    }

    // Cleanup
    adiboupk::platform::run_process("rm", {"-rf", tmp_dir});

    // Verify the upgrade worked
    std::string verify_output;
    adiboupk::platform::run_process(dest, {"version"}, true, &verify_output);
    if (verify_output.find(vc.latest) != std::string::npos) {
        std::cout << std::endl << "==> Successfully upgraded: " << vc.current << " -> " << vc.latest << std::endl;
    } else {
        std::cout << std::endl << "==> Binary replaced. Verify with: adiboupk version" << std::endl;
    }
    return 0;
}

static int cmd_update(const adiboupk::cli::ParsedArgs& args) {
    auto root = resolve_root(args);

    // Step 1: Re-scan for new/removed groups
    std::cout << "==> Scanning for group changes..." << std::endl;

    auto discovered = adiboupk::discovery::scan(root);
    auto cfg = adiboupk::config::load(root);
    auto lock = adiboupk::config::load_lock(root);

    // Detect added/removed groups
    std::set<std::string> old_names, new_names;
    for (const auto& g : cfg.groups) old_names.insert(g.name);
    for (const auto& g : discovered) new_names.insert(g.name);

    for (const auto& name : new_names) {
        if (old_names.find(name) == old_names.end()) {
            std::cout << "  [new] " << name << std::endl;
        }
    }
    for (const auto& name : old_names) {
        if (new_names.find(name) == new_names.end()) {
            std::cout << "  [removed] " << name << std::endl;
            auto vdir = cfg.venvs_dir / name;
            if (adiboupk::venv::destroy(vdir)) {
                std::cout << "    Removed orphaned venv" << std::endl;
            }
            lock.entries.erase(name);
        }
    }

    // Update config with discovered groups
    cfg.groups = discovered;
    if (!adiboupk::config::save(cfg)) {
        std::cerr << "Failed to write " << adiboupk::config::CONFIG_FILE << std::endl;
        return 1;
    }

    // Reload to get fresh hashes
    cfg = adiboupk::config::load(root);

    // Step 2: Reinstall changed groups
    std::cout << "==> Checking dependencies..." << std::endl;

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

    std::cout << "Update complete." << std::endl;

    // Step 3: Check if a newer version of adiboupk itself is available
    std::cout << std::endl << "==> Checking for adiboupk updates..." << std::endl;
    VersionCheck vc;
    if (check_version(vc)) {
        std::cout << "  Installed: " << vc.current << std::endl;
        std::cout << "  Latest:    " << vc.latest << std::endl;
        if (!vc.up_to_date) {
            if (vc.is_dev_build && vc.current_base == vc.latest) {
                std::cout << std::endl
                          << "  You are running a dev build. Release " << vc.latest
                          << " is available." << std::endl;
            } else {
                std::cout << std::endl
                          << "  New version available: "
                          << vc.current_base << " -> " << vc.latest << std::endl;
            }
            std::cout << "  Run 'adiboupk upgrade' to install it." << std::endl;
        } else {
            std::cout << "  adiboupk is up to date." << std::endl;
        }
    }

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

    // Cross-group conflicts (requirements.txt vs requirements.txt)
    std::cout << "==> Cross-group conflicts (requirements.txt)..." << std::endl;
    auto conflicts = adiboupk::auditor::audit(cfg.groups);
    std::cout << adiboupk::auditor::format_conflicts(conflicts);

    // Transitive dependency conflicts (pip check inside each venv)
    std::cout << "==> Transitive dependency conflicts (sub-dependencies)..." << std::endl;
    std::cout << adiboupk::auditor::audit_transitive(cfg);

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
    if (cfg.isolate_packages) {
        std::cout << "Mode:    per-package isolation" << std::endl;
    }
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

        if (cfg.isolate_packages) {
            bool iso_ok = adiboupk::isolator::exists(cfg, g);
            std::cout << "    Isolation:    " << (iso_ok ? "OK" : "MISSING") << std::endl;
        } else if (venv_ok) {
            std::string check = adiboupk::installer::pip_check(vdir);
            if (!check.empty() &&
                check.find("No broken requirements found") == std::string::npos) {
                std::cout << "    Deps:         CONFLICTS" << std::endl;
            } else {
                std::cout << "    Deps:         OK" << std::endl;
            }
        }

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
            std::cout << "  Removed venv " << g.name << std::endl;
            removed++;
        }
        if (adiboupk::isolator::clean(cfg, g)) {
            std::cout << "  Removed isolated deps " << g.name << std::endl;
            removed++;
        }
    }

    // Also remove the .venvs directory if empty
    std::error_code ec;
    fs::remove(cfg.venvs_dir, ec);

    // Clear lock file
    adiboupk::LockFile empty_lock;
    adiboupk::config::save_lock(empty_lock, root);

    std::cout << "Cleaned " << removed << " environment(s)." << std::endl;
    return 0;
}

static int cmd_uninstall(const adiboupk::cli::ParsedArgs& args) {
    // Step 1: Clean project files if we're in a project
    auto root = resolve_root(args);
    auto cfg = adiboupk::config::load(root);

    bool has_project = !cfg.groups.empty() || fs::exists(root / adiboupk::config::CONFIG_FILE);

    if (has_project) {
        std::cout << "Project files found in " << root.string() << std::endl;
        std::cout << "  - .venvs/ (managed virtual environments)" << std::endl;
        std::cout << "  - adiboupk.json (config)" << std::endl;
        std::cout << "  - adiboupk.lock (lock file)" << std::endl;
        std::cout << std::endl;

        auto do_remove = [&]() {
            for (const auto& g : cfg.groups) {
                adiboupk::venv::destroy(adiboupk::venv::venv_dir_for(cfg, g));
                adiboupk::isolator::clean(cfg, g);
            }
            std::error_code ec;
            fs::remove(cfg.venvs_dir, ec);
            fs::remove(root / adiboupk::config::CONFIG_FILE, ec);
            fs::remove(root / adiboupk::config::LOCK_FILE, ec);
            std::cout << "Project files removed." << std::endl;
        };

        if (!args.force) {
            std::cout << "Remove project files? [y/N] ";
            std::cout.flush();
            std::string answer;
            std::getline(std::cin, answer);
            if (!answer.empty() && (answer[0] == 'y' || answer[0] == 'Y')) {
                do_remove();
            } else {
                std::cout << "Project files kept." << std::endl;
            }
        } else {
            do_remove();
        }
        std::cout << std::endl;
    }

    // Step 2: Locate the binary
    fs::path self_path;
    std::error_code ec;

#ifdef _WIN32
    // Windows: GetModuleFileNameW
    wchar_t buf[MAX_PATH];
    if (GetModuleFileNameW(nullptr, buf, MAX_PATH) > 0) {
        self_path = buf;
    }
#else
    // Linux: /proc/self/exe
    self_path = fs::read_symlink("/proc/self/exe", ec);
    if (self_path.empty()) {
        // macOS / fallback: which
        std::string which_output;
        adiboupk::platform::run_process("which", {"adiboupk"}, true, &which_output);
        while (!which_output.empty() && (which_output.back() == '\n' || which_output.back() == '\r'))
            which_output.pop_back();
        if (!which_output.empty()) {
            self_path = which_output;
        }
    }
#endif

    if (self_path.empty()) {
        std::cerr << "Could not locate adiboupk binary." << std::endl;
        return 1;
    }

    std::cout << "Remove adiboupk binary (" << self_path.string() << ")?" << std::endl;

    if (!args.force) {
        std::cout << "This cannot be undone. [y/N] ";
        std::cout.flush();
        std::string answer;
        std::getline(std::cin, answer);
        if (answer.empty() || (answer[0] != 'y' && answer[0] != 'Y')) {
            std::cout << "Uninstall cancelled." << std::endl;
            return 0;
        }
    }

#ifdef _WIN32
    // Windows: schedule deletion after process exits
    // Can't delete a running exe, so use a cmd /c ping+del trick
    auto parent_dir = self_path.parent_path();
    std::string del_cmd = "cmd /c \"ping -n 2 127.0.0.1 >nul & del /f /q \""
        + self_path.string() + "\" & rmdir /q \"" + parent_dir.string() + "\"\"";

    // Remove from user PATH
    std::cout << "Removing from PATH..." << std::endl;
    int rc = adiboupk::platform::run_process("powershell", {
        "-Command",
        "$p = [Environment]::GetEnvironmentVariable('Path','User');"
        "$p = ($p -split ';' | Where-Object { $_ -notlike '*adiboupk*' }) -join ';';"
        "[Environment]::SetEnvironmentVariable('Path', $p, 'User')"
    });

    std::cout << "adiboupk will be removed after this process exits." << std::endl;
    std::cout << std::endl;

    // Ask about third-party tools installed by the install script
    std::cout << "The install script may have installed these tools:" << std::endl;
    std::cout << "  - MinGW-w64 (C:\\mingw64)" << std::endl;
    std::cout << "  - 7-Zip" << std::endl;
    std::cout << "  - MSYS2" << std::endl;
    std::cout << "  - CMake" << std::endl;
    std::cout << std::endl;

    if (!args.force) {
        std::cout << "Remove third-party build tools? [y/N] ";
        std::cout.flush();
        std::string answer;
        std::getline(std::cin, answer);
        if (!answer.empty() && (answer[0] == 'y' || answer[0] == 'Y')) {
            // Remove standalone MinGW
            if (fs::exists("C:\\mingw64")) {
                std::cout << "  Removing C:\\mingw64..." << std::endl;
                fs::remove_all("C:\\mingw64", ec);
                // Clean from PATH
                adiboupk::platform::run_process("powershell", {
                    "-Command",
                    "$p = [Environment]::GetEnvironmentVariable('Path','Machine');"
                    "$p = ($p -split ';' | Where-Object { $_ -notlike '*mingw64*' }) -join ';';"
                    "try { [Environment]::SetEnvironmentVariable('Path', $p, 'Machine') } catch {}"
                });
                adiboupk::platform::run_process("powershell", {
                    "-Command",
                    "$p = [Environment]::GetEnvironmentVariable('Path','User');"
                    "$p = ($p -split ';' | Where-Object { $_ -notlike '*mingw64*' }) -join ';';"
                    "[Environment]::SetEnvironmentVariable('Path', $p, 'User')"
                });
            }
            // Uninstall MSYS2 via winget
            adiboupk::platform::run_process("winget", {
                "uninstall", "--id", "MSYS2.MSYS2", "--silent"
            });
            // Uninstall 7-Zip via winget
            adiboupk::platform::run_process("winget", {
                "uninstall", "--id", "7zip.7zip", "--silent"
            });
            // Don't uninstall CMake — user might need it for other projects
            std::cout << "  Third-party tools removed." << std::endl;
            std::cout << "  Note: CMake was kept (may be used by other projects)." << std::endl;
        }
    }

    // Launch delayed deletion
    system(del_cmd.c_str());
#else
    // Unix: try direct remove, fall back to sudo
    int rc = adiboupk::platform::run_process("rm", {self_path.string()});
    if (rc != 0) {
        rc = adiboupk::platform::run_process("sudo", {"rm", self_path.string()});
    }

    if (rc != 0) {
        std::cerr << "Failed to remove binary. Run manually:" << std::endl;
        std::cerr << "  sudo rm " << self_path.string() << std::endl;
        return 1;
    }
#endif

    std::cout << "adiboupk has been uninstalled." << std::endl;
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
        case adiboupk::cli::Command::UPDATE:  return cmd_update(args);
        case adiboupk::cli::Command::UPGRADE: return cmd_upgrade(args);
        case adiboupk::cli::Command::RUN:     return cmd_run(args);
        case adiboupk::cli::Command::AUDIT:   return cmd_audit(args);
        case adiboupk::cli::Command::STATUS:  return cmd_status(args);
        case adiboupk::cli::Command::CLEAN:     return cmd_clean(args);
        case adiboupk::cli::Command::UNINSTALL: return cmd_uninstall(args);
        case adiboupk::cli::Command::WHICH:     return cmd_which(args);
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
