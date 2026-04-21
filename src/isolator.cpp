#include "adiboupk/isolator.hpp"
#include "adiboupk/platform.hpp"
#include "adiboupk/utils.hpp"
#include "nlohmann/json.hpp"

#include <iostream>

using json = nlohmann::json;

namespace adiboupk {
namespace isolator {

// ---------------------------------------------------------------------------
// Inline Python sources — embedded here so no external files are needed.
// ---------------------------------------------------------------------------

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

        if pkg_dir not in sys.path:
            sys.path.insert(0, pkg_dir)

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

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool install_isolated(const fs::path& deps_dir,
                      const fs::path& requirements_path,
                      const std::string& python_cmd) {
    std::string content = utils::read_file(requirements_path);
    if (content.empty()) {
        std::cerr << "adiboupk: empty or missing " << requirements_path.string() << std::endl;
        return false;
    }

    auto reqs = utils::parse_requirements(content);
    if (reqs.empty()) {
        std::cerr << "adiboupk: no packages found in " << requirements_path.string() << std::endl;
        return false;
    }

    std::error_code ec;
    fs::create_directories(deps_dir, ec);

    json package_map;
    bool all_ok = true;

    for (const auto& [pkg_name, req_line] : reqs) {
        fs::path pkg_dir = deps_dir / pkg_name;

        if (fs::exists(pkg_dir)) {
            fs::remove_all(pkg_dir, ec);
        }
        fs::create_directories(pkg_dir, ec);

        std::cout << "      " << req_line << " -> " << pkg_name << "/" << std::endl;

        int rc = platform::run_process(
            python_cmd,
            {"-m", "pip", "install", "--target", pkg_dir.string(),
             req_line, "--quiet", "--no-warn-conflicts"},
            false, nullptr
        );

        if (rc != 0) {
            std::cerr << "      Failed to install " << req_line << std::endl;
            all_ok = false;
            continue;
        }

        package_map[pkg_name] = pkg_dir.string();
    }

    fs::path map_path = deps_dir / "package_map.json";
    if (!utils::write_file(map_path, package_map.dump(2) + "\n")) {
        std::cerr << "adiboupk: failed to write " << map_path.string() << std::endl;
        return false;
    }

    return all_ok;
}

void ensure_runtime_files(const fs::path& deps_dir) {
    fs::path loader = deps_dir / "adiboupk_loader.py";
    fs::path runner = deps_dir / "adiboupk_run.py";

    if (!fs::exists(loader)) {
        utils::write_file(loader, LOADER_PY);
    }
    if (!fs::exists(runner)) {
        utils::write_file(runner, RUN_PY);
    }
}

fs::path deps_dir_for(const Config& cfg, const Group& group) {
    std::string safe_name = group.name;
    for (auto& c : safe_name) {
        if (c == '/') c = '_';
    }
    return cfg.venvs_dir / (safe_name + "_isolated");
}

fs::path package_map_for(const Config& cfg, const Group& group) {
    return deps_dir_for(cfg, group) / "package_map.json";
}

bool exists(const Config& cfg, const Group& group) {
    return fs::exists(package_map_for(cfg, group));
}

bool clean(const Config& cfg, const Group& group) {
    auto dir = deps_dir_for(cfg, group);
    if (!fs::exists(dir)) return false;
    std::error_code ec;
    fs::remove_all(dir, ec);
    return !ec;
}

} // namespace isolator
} // namespace adiboupk
