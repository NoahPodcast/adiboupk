# adiboupk

Python dependency isolation for multi-module projects. Written in C++ for ~1ms startup overhead.

## Problem

When a project has multiple subdirectories each with their own `requirements.txt`, a global `pip install` causes version conflicts — the last install wins, silently breaking other modules.

```
project/
├── Enrichments/
│   ├── script1.py
│   └── requirements.txt    ← requests==2.28.0
├── Responses/
│   ├── script2.py
│   └── requirements.txt    ← requests==2.32.5
```

`adiboupk` creates one venv per group and transparently routes each script to the correct one.

## Quick Start

```bash
# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# One command to scan, create venvs, install deps, and audit
./build/adiboupk setup

# Run a script — adiboupk picks the right venv automatically
./build/adiboupk run ./Enrichments/cortex_lookup.py hostname123
```

## Commands

| Command | Description |
|---|---|
| `setup` | All-in-one: scan, create venvs, install deps, audit |
| `init` | Scan project root and generate `adiboupk.json` |
| `install` | Create venvs and install dependencies for each group |
| `run <script> [args]` | Execute a script using its group's venv |
| `audit` | Report cross-group dependency version conflicts |
| `status` | Show groups, venv state, and dependency hashes |
| `clean` | Remove all managed venvs |
| `which <script>` | Show which python binary would be used |

## Options

| Flag | Description |
|---|---|
| `--root <path>` | Project root directory (default: cwd) |
| `--force` | Reinstall even if dependencies are up to date |
| `--verbose` | Detailed output |

## How It Works

1. **`init`** scans immediate subdirectories for `requirements.txt` files and writes `adiboupk.json`
2. **`install`** creates a venv per group under `.venvs/` and runs `pip install -r` into each
3. **`run`** resolves the script's path to a group, finds the group's venv python, and `exec`s it
4. A SHA-256 hash of each `requirements.txt` is stored in `adiboupk.lock` — reinstall only happens when the file changes

## Integration with Node.js / Orchestrators

Replace direct `python` calls with `adiboupk run`:

```javascript
// Before — global python, version conflicts
var cmd = 'python ./Enrichments/cortex_lookup.py ' + hostname;

// After — isolated venv per group
var cmd = 'adiboupk run ./Enrichments/cortex_lookup.py ' + hostname;
```

Wrapper scripts are provided in `wrappers/` for bash and PowerShell.

## Cross-Platform

Builds and runs on Linux and Windows from a single codebase. CMake handles platform detection.

- Linux: `fork()/execvp()` for process execution, `bin/python` venv layout
- Windows: `CreateProcessW()` for process execution, `Scripts/python.exe` venv layout

## Building

Requires CMake 3.16+ and a C++17 compiler.

```bash
# Linux
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure

# Windows (Visual Studio)
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Dependencies

All header-only, vendored in `third_party/`:

- [nlohmann/json](https://github.com/nlohmann/json) — JSON parsing
- [PicoSHA2](https://github.com/okdshin/PicoSHA2) — SHA-256 hashing
- [doctest](https://github.com/doctest/doctest) — Unit testing

## License

MIT
