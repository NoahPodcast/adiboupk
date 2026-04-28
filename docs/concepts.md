---
hide:
  - navigation
---

# Concepts

This page explains the core concepts behind adiboupk.

---

## Overall Architecture

```mermaid
graph TB
    subgraph "Your project"
        A[adiboupk.json] --> B[adiboupk]
        C[requirements.txt] --> B
        D[.py scripts] --> B
    end

    subgraph "Managed by adiboupk"
        B --> E[".venvs/"]
        B --> F["adiboupk.lock"]
        E --> G["GroupA/"]
        E --> H["GroupB/"]
    end

    G --> I["python + deps A"]
    H --> J["python + deps B"]
```

---

## Groups

A **group** is a set of Python scripts that share the same dependencies. Each group has:

- A **name** (e.g. `Analytics`)
- A **directory** containing the scripts
- A **requirements.txt** listing the dependencies
- A **dedicated venv** in `.venvs/`

```mermaid
graph LR
    subgraph "Group Analytics"
        A[requirements.txt<br/>requests==2.28.0] --> V1[".venvs/Analytics/"]
        S1[script1.py] -.-> V1
        S2[script2.py] -.-> V1
    end

    subgraph "Group Notifications"
        B[requirements.txt<br/>requests==2.32.5] --> V2[".venvs/Notifications/"]
        S3[script3.py] -.-> V2
    end
```

### Auto-discovery

`adiboupk init` scans the project to find groups:

1. Each **subdirectory** containing a `requirements.txt` becomes a group
2. A `requirements.txt` **at the project root** also creates a group
3. Ignored directories: `.git`, `node_modules`, `build`, `__pycache__`, etc.

### Subgroups

A directory can contain multiple `requirements-*.txt` files to create subgroups:

```
Analytics/
├── requirements.txt          → group "Analytics" (fallback)
├── requirements-vt.txt       → subgroup "Analytics/vt"
├── requirements-data.txt   → subgroup "Analytics/data"
├── script_vt.py              → mapped to "Analytics/vt"
└── data_fetch.py          → mapped to "Analytics/data"
```

Scripts are automatically associated with the subgroup whose suffix appears in their filename.

---

## Virtual Environments (venvs)

Each group gets its own Python venv in the `.venvs/` directory:

```
.venvs/
├── Analytics/
│   ├── bin/python
│   ├── lib/python3.x/site-packages/
│   │   ├── requests/
│   │   └── ...
│   └── pyvenv.cfg
└── Notifications/
    ├── bin/python
    └── lib/python3.x/site-packages/
        ├── requests/
        └── ...
```

### Lifecycle

```mermaid
stateDiagram-v2
    [*] --> Absent: Project initialized
    Absent --> Created: adiboupk install
    Created --> UpToDate: pip install -r OK
    UpToDate --> Stale: requirements.txt modified
    Stale --> UpToDate: adiboupk install
    UpToDate --> Removed: adiboupk clean
    Removed --> [*]
```

---

## Lock File

The `adiboupk.lock` file stores the **SHA-256 hash** of each `requirements.txt` at install time. This allows:

- **Skipping** groups whose dependencies haven't changed
- **Detecting** when a reinstall is needed

```json
{
  "groups": {
    "Analytics": {
      "requirements_hash": "a1b2c3d4e5f6...",
      "installed": true
    },
    "Notifications": {
      "requirements_hash": "f6e5d4c3b2a1...",
      "installed": true
    }
  }
}
```

### Install Flow

```mermaid
flowchart TD
    A[adiboupk install] --> B{For each group}
    B --> C{requirements.txt hash<br/>== hash in lock?}
    C -->|Yes| D["[skip] up to date"]
    C -->|No| E[Create venv if missing]
    E --> F[pip install -r requirements.txt]
    F --> G[Update lock file]
    G --> B
```

---

## Script Resolution

When you run `adiboupk run script.py`, adiboupk determines which venv to use:

```mermaid
flowchart TD
    A["adiboupk run script.py"] --> B{Script explicitly<br/>mapped?}
    B -->|Yes| C[Use the subgroup]
    B -->|No| D{Script in a<br/>group directory?}
    D -->|Yes| E[Use the directory's group]
    D -->|No| F[Fallback: system python]
    C --> G["exec python script.py"]
    E --> G
    F --> G
```

Resolution priority:

1. **Explicit mapping** — the script is listed in a group's `scripts` field
2. **Subgroup** — the script's directory matches a subgroup
3. **Parent group** — the script's directory is under a group
4. **Fallback** — system python if no group matches

---

## Two Isolation Modes

adiboupk offers two levels of isolation:

### Standard mode (default)

Each **group** gets its own venv. Conflicts **between groups** are resolved.

```mermaid
graph TB
    subgraph "Group A"
        VA[venv A] --> PA["requests==2.28.0<br/>urllib3==1.26.x"]
    end
    subgraph "Group B"
        VB[venv B] --> PB["requests==2.32.5<br/>urllib3==2.2.1"]
    end
```

### Per-package isolation mode

Each **package** gets its own directory. Conflicts **within a single group** are resolved.

```mermaid
graph TB
    subgraph "Group A — isolate_packages: true"
        direction TB
        VA[venv A<br/>Python interpreter]
        subgraph "Isolated directories"
            P1[".deps/requests/<br/>requests + urllib3 1.26.x"]
            P2[".deps/urllib3/<br/>urllib3 2.2.1"]
        end
    end
```

→ See [Per-Package Isolation](isolation.md) for details.
