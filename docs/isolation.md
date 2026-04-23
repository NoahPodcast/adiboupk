---
hide:
  - navigation
---

# Per-Package Isolation

Per-package isolation mode resolves dependency conflicts **within a single group** — when two packages in the same `requirements.txt` have incompatible transitive dependencies.

---

## The Problem

Consider this `requirements.txt`:

```
requests==2.28.0
urllib3==2.2.1
```

In standard mode, `pip install` fails:

```
ERROR: Cannot install requests==2.28.0 and urllib3==2.2.1
because these package versions have conflicting dependencies.
```

Why? `requests==2.28.0` depends on `urllib3<1.27`, but we also explicitly need `urllib3==2.2.1`.

```mermaid
graph TD
    R[requirements.txt] --> A["requests==2.28.0"]
    R --> B["urllib3==2.2.1"]
    A -->|"depends on"| C["urllib3 < 1.27 ❌"]
    B -->|"requires"| D["urllib3 == 2.2.1 ❌"]
    C -.-|"CONFLICT"| D

    style C fill:#ff6b6b,color:#fff
    style D fill:#ff6b6b,color:#fff
```

---

## The Solution

With `isolate_packages: true`, each package is installed into its own directory via `pip install --target`. Each package bundles its own transitive dependencies, with no interference.

```mermaid
graph TB
    subgraph "Standard mode ❌"
        direction TB
        V1["single venv"]
        V1 --> X1["requests==2.28.0"]
        V1 --> X2["urllib3==??? 💥"]
    end

    subgraph "Isolation mode ✅"
        direction TB
        V2["venv (interpreter)"]
        subgraph ".venvs/group_isolated/"
            D1["requests/<br/>requests + urllib3==1.26.x"]
            D2["urllib3/<br/>urllib3==2.2.1"]
        end
    end
```

---

## Enabling It

Add `isolate_packages: true` to `adiboupk.json`:

```json
{
  "isolate_packages": true,
  "venvs_dir": ".venvs",
  "groups": [
    {
      "name": "my_group",
      "directory": ".",
      "requirements": "requirements.txt"
    }
  ]
}
```

Then:

```bash
adiboupk install
adiboupk run my_script.py
```

---

## How It Works

### Install Phase

```mermaid
sequenceDiagram
    participant U as User
    participant A as adiboupk
    participant P as pip

    U->>A: adiboupk install
    A->>A: Read requirements.txt
    A->>A: Parse packages

    loop For each package
        A->>P: pip install --target .deps/pkg/ package
        P-->>A: OK
        A->>A: Record in package_map.json
    end

    A-->>U: Installation complete
```

For each package in `requirements.txt`:

1. Creates a directory `.venvs/<group>_isolated/<package>/`
2. Runs `pip install --target=<dir> <package>`
3. Each package gets its own transitive dependencies in its directory

Result:

```
.venvs/my_group_isolated/
├── package_map.json          ← package → directory mapping
├── requests/
│   ├── requests/             ← the requests package
│   ├── urllib3/              ← urllib3 1.26.x (requests' dependency)
│   ├── certifi/
│   └── charset_normalizer/
└── urllib3/
    └── urllib3/              ← urllib3 2.2.1 (explicitly installed)
```

### `package_map.json`

Automatically generated file mapping each package to its directory:

```json
{
  "requests": "/absolute/path/.venvs/my_group_isolated/requests",
  "urllib3": "/absolute/path/.venvs/my_group_isolated/urllib3"
}
```

### Runtime Phase

```mermaid
sequenceDiagram
    participant U as User
    participant A as adiboupk (C++)
    participant R as adiboupk_run.py
    participant H as AdiboupkFinder
    participant S as User script

    U->>A: adiboupk run script.py
    A->>A: Detect group
    A->>A: Write adiboupk_loader.py + adiboupk_run.py
    A->>A: setenv ADIBOUPK_PACKAGE_MAP
    A->>R: exec python adiboupk_run.py script.py

    R->>H: install() → sys.meta_path.insert(finder)
    R->>S: runpy.run_path(script.py)

    S->>H: import requests
    H->>H: find_spec("requests")
    H->>H: sys.path.insert(0, ".deps/requests/")
    H-->>S: spec found → module loaded

    S->>H: import urllib3
    H->>H: find_spec("urllib3")
    H->>H: sys.path.insert(0, ".deps/urllib3/")
    H-->>S: spec found → module loaded
```

### The Import Hook (`AdiboupkFinder`)

The core of the system is a Python `MetaPathFinder` injected into `sys.meta_path`:

```python
class AdiboupkFinder(importlib.abc.MetaPathFinder):
    def find_spec(self, fullname, path, target=None):
        # 1. Extract the top-level package (e.g. "requests.sessions" → "requests")
        top_level = fullname.split(".")[0].lower()

        # 2. Check if we have a mapping for this package
        if top_level not in self._map:
            return None

        # 3. Add the isolated directory to sys.path
        sys.path.insert(0, self._map[top_level])

        # 4. Temporarily remove ourselves from sys.meta_path
        sys.meta_path.remove(self)
        try:
            # 5. Let the standard PathFinder find the module
            importlib.invalidate_caches()
            return importlib.util.find_spec(fullname)
        finally:
            # 6. Re-insert ourselves
            sys.meta_path.insert(0, self)
```

Key points:

- The finder **temporarily removes itself** from `sys.meta_path` to avoid infinite recursion
- `importlib.invalidate_caches()` is required because `sys.path` was modified
- Each package's directory stays in `sys.path` so its internal sub-imports work

---

## Limitations

!!! warning "Compatibility warnings"
    Some packages check their dependency versions at import time.
    For example, `requests` emits a `RequestsDependencyWarning` if it detects
    a different `urllib3` version than expected. This is cosmetic —
    functionality is not affected.

!!! info "Disk space"
    In isolation mode, each package bundles its own transitive dependencies.
    If two packages depend on `certifi`, it will be installed twice.
    This is the trade-off for guaranteeing no conflicts.

---

## When to Use It

| Situation | Recommended mode |
|---|---|
| Conflicts **between groups** (different directories) | Standard (default) |
| Conflicts **within a group** (same requirements.txt) | `isolate_packages: true` |
| No conflicts | Standard (default) |
| Limited disk space | Standard (default) |
