# CLAUDE.md — adiboupk project context

## What is adiboupk

adiboupk is a CLI tool that isolates Python dependencies for multi-module projects. When a project has multiple directories each with their own `requirements.txt`, adiboupk creates a separate venv per group and transparently routes `adiboupk run script.py` to the correct one.

It also supports **per-package isolation** (`isolate_packages: true`), where each package within a single `requirements.txt` gets its own `--target` directory, with a Python import hook routing imports at runtime. This solves transitive dependency conflicts (e.g., `requests==2.28.0` needing `urllib3<1.27` while `urllib3==2.2.1` is also required).

Written in C++17 for ~1ms startup overhead. Cross-platform: Linux, macOS, Windows.

## Repository layout

```
adiboupk/
├── CMakeLists.txt              # Build system (CMake 3.16+, C++17)
├── include/adiboupk/           # Public headers (one per module)
│   ├── cli.hpp                 # Command parsing, help text
│   ├── config.hpp              # Config structs (Group, Config, LockFile)
│   ├── discovery.hpp           # Scan project for groups
│   ├── installer.hpp           # pip install into venvs
│   ├── isolator.hpp            # Per-package isolation (--target installs, import hook)
│   ├── runner.hpp              # Execute scripts in correct venv
│   ├── auditor.hpp             # Cross-group conflict detection
│   ├── venv.hpp                # Venv create/destroy/path helpers
│   ├── platform.hpp            # OS abstraction (process exec, paths)
│   └── utils.hpp               # SHA-256, file I/O, string helpers
├── src/
│   ├── main.cpp                # Entry point, all cmd_* functions (~890 lines)
│   ├── platform_linux.cpp      # Linux/macOS: fork/execvp, setenv
│   ├── platform_windows.cpp    # Windows: CreateProcessW, _putenv_s
│   ├── platform_common.cpp     # Shared platform code
│   ├── isolator.cpp            # Inline Python hook strings + install logic
│   └── *.cpp                   # One per header
├── tests/                      # doctest unit tests (27 tests, 91 assertions)
├── third_party/                # Vendored headers (nlohmann/json, picosha2, doctest)
├── demo/conflict_test/         # E2E test: requests==2.28.0 + urllib3==2.2.1
├── docs/                       # MkDocs Material documentation (branch: docs)
├── wrappers/                   # Shell/PS1 wrappers to replace `python` transparently
├── install.sh                  # Linux/macOS installer
├── install.ps1                 # Windows installer (MinGW standalone)
└── .github/workflows/build.yml # CI: build + test on Ubuntu and Windows
```

## Architecture

### Module responsibilities

| Module | Purpose |
|---|---|
| `cli` | Parse argv into `Command` enum + `ParsedArgs` struct |
| `config` | Load/save `adiboupk.json` and `adiboupk.lock` (nlohmann/json) |
| `discovery` | Scan directories for `requirements.txt` / `requirements-*.txt`, build `Group` list |
| `installer` | Create venvs, run `pip install -r`, `pip check` for transitive conflicts |
| `isolator` | Per-package isolation: `pip install --target`, `package_map.json`, inline Python import hook |
| `runner` | Resolve script → group, set env vars, `exec_replace()` into correct python |
| `auditor` | Compare requirements across groups, detect version conflicts |
| `venv` | Venv path helpers, create/destroy, locate python binary |
| `platform` | OS abstraction: `run_process()`, `exec_replace()`, `venv_python()`, `get_env()` |
| `utils` | `sha256_file()`, `read_file()`, `write_file()`, `parse_requirements()`, `split()` |
| `main` | All `cmd_*` functions (setup, install, run, audit, status, clean, upgrade, uninstall, etc.) |

### Data flow

```
adiboupk.json (config)
    ↓
discovery::scan() → vector<Group>
    ↓
installer::install_all() → creates .venvs/<group>/ + pip install
    or
isolator::install_isolated() → creates .venvs/<group>_isolated/<pkg>/ + package_map.json
    ↓
runner::run() → resolves group → exec_replace(python, script)
    (isolated mode: sets ADIBOUPK_PACKAGE_MAP, launches adiboupk_run.py wrapper)
```

### Per-package isolation internals

The Python import hook is embedded as C++ raw string literals in `isolator.cpp` (not external files). At runtime:

1. `runner.cpp` calls `isolator::ensure_runtime_files()` to write `adiboupk_loader.py` + `adiboupk_run.py` into the deps dir
2. Sets `ADIBOUPK_PACKAGE_MAP` env var pointing to `package_map.json`
3. Launches `python adiboupk_run.py <script.py>`
4. The Python hook (`AdiboupkFinder`) uses `find_spec()` — temporarily removes itself from `sys.meta_path`, adds the package's isolated dir to `sys.path`, calls `importlib.invalidate_caches()`, then delegates to the standard `PathFinder`

Key design decision: the finder must remove itself from `sys.meta_path` during delegation to avoid infinite recursion, and `invalidate_caches()` is required because `PathFinder` caches negative lookups for paths not yet in `sys.path`.

## Build and test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Tests use doctest (header-only, vendored). All tests are in `tests/test_*.cpp`.

On macOS: don't use `-static-libgcc -static-libstdc++` (Apple clang doesn't support it). The `CMakeLists.txt` handles this with `if(NOT APPLE)`.

On Windows: use MinGW (`-G "MinGW Makefiles"`) or MSVC (`-G "Visual Studio 17 2022"`). The `setenv()` call is replaced by `_putenv_s()` via `#ifdef _WIN32`.

## Platform-specific gotchas

- **Linux**: `fork/execvp` for process execution, `/proc/self/exe` for binary path
- **macOS**: No `/proc/self/exe` — uses `which adiboupk` fallback. No static linking. Uses `sysctl -n hw.ncpu` instead of `nproc`
- **Windows**: `CreateProcessW` for process execution, `GetModuleFileNameW` for binary path. Can't delete/overwrite a running `.exe` — uses rename trick (`adiboupk.exe` → `adiboupk.exe.old`, copy new, delete old). `setenv()` → `_putenv_s()`. Install dir is `%LOCALAPPDATA%\adiboupk\` not `/usr/local/bin`

## Versioning

Version is injected at build time via `git describe --tags` → `ADIBOUPK_VERSION` preprocessor define. If no tag is on the current commit, the hash is used (e.g., `28c0962`). Always tag releases.

Current version: **v1.6.0**

## Key files for common tasks

| Task | Files to modify |
|---|---|
| Add a new CLI command | `cli.hpp` (enum + parse), `main.cpp` (cmd_* function + switch case) |
| Change config format | `config.hpp` (struct), `config.cpp` (load/save JSON) |
| Change group discovery | `discovery.cpp` |
| Change install behavior | `installer.cpp` (standard), `isolator.cpp` (per-package) |
| Change script execution | `runner.cpp` |
| Change Python import hook | `isolator.cpp` (LOADER_PY raw string) |
| Add platform support | `platform.hpp` (declare), `platform_<os>.cpp` (implement) |
| Fix install script | `install.sh` (Linux/macOS), `install.ps1` (Windows) |
| Update docs | `docs/*.md` + `mkdocs.yml` (branch: `docs`) |

## Conventions

- **Namespaces**: `adiboupk::<module>::` (e.g., `adiboupk::installer::install_all()`)
- **Headers**: one per module in `include/adiboupk/`, `#pragma once`
- **Error handling**: return `bool` or `int` (exit code), print to `std::cerr`
- **No exceptions**: the codebase doesn't use C++ exceptions
- **Vendored deps**: nlohmann/json, picosha2, doctest — all header-only in `third_party/`
- **Commit messages**: imperative mood, first line is summary, `Co-authored-by:` for AI contributions
- **Tags**: `vMAJOR.MINOR.PATCH` — always tag before telling users to install/upgrade

## Common pitfalls

1. **Forgetting to tag**: `adiboupk version` shows a commit hash instead of a version number if the commit isn't tagged
2. **Python hook recursion**: if modifying the import hook in `isolator.cpp`, the finder MUST remove itself from `sys.meta_path` before calling `find_spec`, and call `invalidate_caches()` after modifying `sys.path`
3. **Windows exe replacement**: can't delete or overwrite a running `.exe` — must rename first
4. **macOS static linking**: Apple clang doesn't support `-static-libgcc`/`-static-libstdc++`
5. **`main.cpp` is large** (~890 lines): all `cmd_*` functions live here. Consider extracting if it grows further
6. **Inline Python in C++**: the import hook Python code lives as raw string literals in `isolator.cpp`. If you edit it, there's no syntax checking at compile time — test with the demo
7. **GitHub raw content cache**: `raw.githubusercontent.com` has CDN caching. After pushing install script changes, users may get stale versions for a few minutes
