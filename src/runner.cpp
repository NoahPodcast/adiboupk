#include "adiboupk/runner.hpp"
#include "adiboupk/discovery.hpp"
#include "adiboupk/isolator.hpp"
#include "adiboupk/platform.hpp"
#include "adiboupk/venv.hpp"

#include <cstdlib>
#include <iostream>

namespace adiboupk {
namespace runner {

fs::path resolve_python(const fs::path& script_path, const Config& cfg) {
    const Group* group = discovery::find_group_for_script(cfg.groups, script_path);
    if (!group) return {};

    auto vdir = venv::venv_dir_for(cfg, *group);
    auto python = platform::venv_python(vdir);

    if (!fs::exists(python)) {
        std::cerr << "adiboupk: venv for group '" << group->name
                  << "' not found. Run 'adiboupk install' first." << std::endl;
        return {};
    }

    return python;
}

[[noreturn]] void run(const fs::path& script_path,
                      const std::vector<std::string>& script_args,
                      const Config& cfg) {
    const Group* group = discovery::find_group_for_script(cfg.groups, script_path);

    if (cfg.isolate_packages && group) {
        // --- Isolated mode ---
        auto map_path = isolator::package_map_for(cfg, *group);
        if (!fs::exists(map_path)) {
            std::cerr << "adiboupk: package_map.json not found for group '"
                      << group->name << "'. Run 'adiboupk install' first." << std::endl;
            std::exit(1);
        }

        auto deps_dir = isolator::deps_dir_for(cfg, *group);
        isolator::ensure_runtime_files(deps_dir);

        setenv("ADIBOUPK_PACKAGE_MAP", fs::absolute(map_path).string().c_str(), 1);

        auto python = resolve_python(script_path, cfg);
        std::string python_cmd;
        if (python.empty()) {
            python_cmd = cfg.python_path.empty()
                ? platform::python_command()
                : cfg.python_path;
        } else {
            python_cmd = python.string();
        }

        std::vector<std::string> args;
        args.push_back(fs::absolute(deps_dir / "adiboupk_run.py").string());
        args.push_back(fs::absolute(script_path).string());
        args.insert(args.end(), script_args.begin(), script_args.end());

        platform::exec_replace(python_cmd, args);
    }

    // --- Standard mode ---
    auto python = resolve_python(script_path, cfg);

    if (python.empty()) {
        std::string sys_python = cfg.python_path.empty()
            ? platform::python_command()
            : cfg.python_path;

        std::cerr << "adiboupk: no group found for " << script_path.string()
                  << ", falling back to system python" << std::endl;

        std::vector<std::string> args;
        args.push_back(script_path.string());
        args.insert(args.end(), script_args.begin(), script_args.end());
        platform::exec_replace(sys_python, args);
    }

    std::vector<std::string> args;
    args.push_back(script_path.string());
    args.insert(args.end(), script_args.begin(), script_args.end());

    platform::exec_replace(python.string(), args);
}

} // namespace runner
} // namespace adiboupk
