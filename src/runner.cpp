#include "adiboupk/runner.hpp"
#include "adiboupk/discovery.hpp"
#include "adiboupk/isolator.hpp"
#include "adiboupk/platform.hpp"
#include "adiboupk/utils.hpp"
#include "adiboupk/venv.hpp"

#include <cstdlib>
#include <iostream>

namespace adiboupk {
namespace runner {

// Inline Python import hook — written to deps dir at runtime.
// Uses find_spec (modern protocol) and temporarily removes itself from
// sys.meta_path to delegate to standard finders without recursion.
static const char* LOADER_PY = R"PY(
import importlib
import importlib.abc
import importlib.util
import json
import os
import sys


def _load_package_map():
    map_path = os.environ.get("ADIBOUPK_PACKAGE_MAP", "")
    if not map_path or not os.path.exists(map_path):
        return {}
    with open(map_path, "r") as f:
        return json.load(f)


class AdiboupkFinder(importlib.abc.MetaPathFinder):
    """Routes top-level imports to isolated --target directories.

    For each mapped package, prepends its directory to sys.path,
    invalidates finder caches, then temporarily removes this finder
    from sys.meta_path so the standard PathFinder locates the package.
    """

    def __init__(self, package_map):
        self._map = {}
        base_dir = os.path.dirname(os.environ.get("ADIBOUPK_PACKAGE_MAP", ""))
        for pkg, path in package_map.items():
            pkg_lower = pkg.lower().replace("-", "_")
            abs_path = path if os.path.isabs(path) else os.path.join(base_dir, path)
            self._map[pkg_lower] = os.path.normpath(abs_path)

    def find_spec(self, fullname, path, target=None):
        top_level = fullname.split(".")[0].lower()
        if top_level not in self._map:
            return None

        pkg_dir = self._map[top_level]

        # Add the isolated directory to sys.path
        if pkg_dir not in sys.path:
            sys.path.insert(0, pkg_dir)

        # Temporarily remove ourselves so the standard PathFinder
        # handles the actual spec lookup (avoids infinite recursion)
        sys.meta_path.remove(self)
        try:
            importlib.invalidate_caches()
            return importlib.util.find_spec(fullname)
        finally:
            if self not in sys.meta_path:
                sys.meta_path.insert(0, self)


def install():
    package_map = _load_package_map()
    if not package_map:
        return
    finder = AdiboupkFinder(package_map)
    sys.meta_path.insert(0, finder)
)PY";

// Inline runner script — loads hook then executes user script
static const char* RUN_PY = R"PY(
import os
import sys
import runpy

import adiboupk_loader
adiboupk_loader.install()

if len(sys.argv) < 2:
    print("Usage: python adiboupk_run.py <script.py> [args...]", file=sys.stderr)
    sys.exit(1)

script_path = sys.argv[1]
sys.argv = sys.argv[1:]

script_abs = os.path.abspath(script_path)
script_dir = os.path.dirname(script_abs)

if script_dir not in sys.path:
    sys.path.insert(0, script_dir)

runpy.run_path(script_abs, run_name="__main__")
)PY";

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
    // Find the group for this script
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

        // Write runtime Python files into deps dir if not present
        fs::path loader_path = deps_dir / "adiboupk_loader.py";
        fs::path run_path = deps_dir / "adiboupk_run.py";

        if (!fs::exists(loader_path)) {
            utils::write_file(loader_path, LOADER_PY);
        }
        if (!fs::exists(run_path)) {
            utils::write_file(run_path, RUN_PY);
        }

        // Set env var for the import hook
        setenv("ADIBOUPK_PACKAGE_MAP", fs::absolute(map_path).string().c_str(), 1);

        // Resolve python from the base venv
        auto python = resolve_python(script_path, cfg);
        std::string python_cmd;
        if (python.empty()) {
            python_cmd = cfg.python_path.empty()
                ? platform::python_command()
                : cfg.python_path;
        } else {
            python_cmd = python.string();
        }

        // Launch: python adiboupk_run.py <script.py> [args...]
        std::vector<std::string> args;
        args.push_back(fs::absolute(run_path).string());
        args.push_back(fs::absolute(script_path).string());
        args.insert(args.end(), script_args.begin(), script_args.end());

        platform::exec_replace(python_cmd, args);
    }

    // --- Standard mode ---
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
