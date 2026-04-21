"""
adiboupk import hook — routes each top-level package to its own isolated directory.

Loaded automatically by adiboupk before executing user scripts.
Reads the package map from ADIBOUPK_PACKAGE_MAP env var (JSON file path).

Package map format (.deps/package_map.json):
{
  "requests": ".deps/requests",
  "urllib3": ".deps/urllib3",
  ...
}

Each directory contains the package and its own sub-dependencies,
installed via `pip install --target=<dir>`.
"""
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
    """Install the adiboupk import hook. Called once at startup."""
    package_map = _load_package_map()
    if not package_map:
        return
    finder = AdiboupkFinder(package_map)
    sys.meta_path.insert(0, finder)
