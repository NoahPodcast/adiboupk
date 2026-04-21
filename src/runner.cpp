#include "adiboupk/runner.hpp"
#include "adiboupk/discovery.hpp"
#include "adiboupk/platform.hpp"
#include "adiboupk/venv.hpp"

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
    auto python = resolve_python(script_path, cfg);

    if (python.empty()) {
        // No matching group — fall back to system python
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

    // Build args: python <script> <script_args...>
    std::vector<std::string> args;
    args.push_back(script_path.string());
    args.insert(args.end(), script_args.begin(), script_args.end());

    platform::exec_replace(python.string(), args);
}

} // namespace runner
} // namespace adiboupk
