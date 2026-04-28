---
hide:
  - navigation
---

# Tutorial

This guide walks you through using adiboupk step by step, from the simplest project to the most advanced use case.

---

## Scenario 1 — Multi-module Project

### Project Structure

You have a SOAR project with multiple modules, each with its own dependencies:

```
soar-project/
├── Analytics/
│   ├── data_fetch.py
│   ├── vt_scan.py
│   └── requirements.txt      ← requests==2.28.0, pandas==2.1.0
├── Notifications/
│   ├── send_alert.py
│   ├── block_ip.py
│   └── requirements.txt      ← requests==2.32.5, smtplib2==1.0
└── Utilities/
    ├── cleanup.py
    └── requirements.txt       ← boto3==1.34.0
```

### Step 1 — Initialize

```bash
cd soar-project/
adiboupk setup
```

```
==> Scanning /home/user/soar-project for module groups...
Found 3 group(s):
  - Analytics (./Analytics/requirements.txt)
  - Notifications (./Notifications/requirements.txt)
  - Utilities (./Utilities/requirements.txt)
Created adiboupk.json

==> Installing dependencies...
  [install] Analytics
    Creating venv...
    Installing dependencies...
    Done.
  [install] Notifications
    Creating venv...
    Installing dependencies...
    Done.
  [install] Utilities
    Creating venv...
    Installing dependencies...
    Done.

==> Auditing for cross-group conflicts...
  requests:
    Analytics: ==2.28.0
    Notifications:   ==2.32.5

Setup complete. Use 'adiboupk run <script.py>' to execute scripts.
```

!!! note "Conflict detected"
    The audit reports that `requests` has different versions between Analytics and Notifications.
    This is expected — it's exactly what adiboupk solves by isolating each group.

### Step 2 — Run scripts

```bash
# Uses the Analytics venv (requests==2.28.0)
adiboupk run ./Analytics/data_fetch.py hostname123

# Uses the Notifications venv (requests==2.32.5)
adiboupk run ./Notifications/send_alert.py user@example.com

# Uses the Utilities venv (boto3)
adiboupk run ./Utilities/cleanup.py --older-than 30d
```

### Step 3 — Check status

```bash
adiboupk status
```

```
Project: /home/user/soar-project
Venvs:   /home/user/soar-project/.venvs

  Analytics
    Directory:    ./Analytics
    Requirements: ./Analytics/requirements.txt
    Hash:         a1b2c3d4e5f6...
    Venv:         OK
    Status:       UP TO DATE
    Deps:         OK

  Notifications
    ...
```

### Step 4 — Update after a change

You modify `Analytics/requirements.txt` to add a package:

```bash
echo "shodan==1.31.0" >> Analytics/requirements.txt
adiboupk install
```

```
Installing dependencies for 3 group(s)...
  [skip] Notifications (up to date)
  [skip] Utilities (up to date)
  [install] Analytics
    Installing dependencies...
    Done.
All groups installed successfully.
```

Only the modified group is reinstalled.

---

## Scenario 2 — Transitive Dependency Conflicts

### The Problem

Your `requirements.txt` contains packages with incompatible transitive dependencies:

```
# requirements.txt
requests==2.28.0      # depends on urllib3<1.27
urllib3==2.2.1         # explicit version, incompatible with requests
```

A standard `pip install` fails:

```
ERROR: Cannot install requests==2.28.0 and urllib3==2.2.1
because these package versions have conflicting dependencies.
```

### The Solution — per-package isolation

```bash
mkdir conflict-demo && cd conflict-demo

# Create requirements.txt
cat > requirements.txt << 'EOF'
requests==2.28.0
urllib3==2.2.1
EOF

# Create a test script
cat > test.py << 'EOF'
import requests
import urllib3
print(f"requests: {requests.__version__}")
print(f"urllib3: {urllib3.__version__}")
s = requests.Session()
print("Everything works!")
EOF

# Initialize
adiboupk init
```

Edit `adiboupk.json` to enable isolation:

```json
{
  "isolate_packages": true,
  "venvs_dir": ".venvs",
  "groups": [
    {
      "name": "conflict-demo",
      "directory": "."
    }
  ]
}
```

```bash
adiboupk install
```

```
Installing dependencies for 1 group(s)...
  [install] conflict-demo
    Creating venv...
    Installing packages in isolated mode...
      requests==2.28.0 -> requests/
      urllib3==2.2.1 -> urllib3/
    Done.
All groups installed successfully.
```

```bash
adiboupk run test.py
```

```
requests: 2.28.0
urllib3: 2.2.1
Everything works!
```

Both packages coexist without conflict.

---

## Scenario 3 — Subgroups

### Structure

A single directory contains scripts with different dependencies:

```
Analytics/
├── requirements-vt.txt        ← vt-py==0.18.0
├── requirements-data.txt    ← pandas==2.1.0
├── script_vt.py
├── script_vt_bulk.py
└── data_fetch.py
```

### Initialization

```bash
adiboupk init
```

adiboupk automatically detects subgroups and maps scripts by naming convention:

```
Found 2 group(s):
  - Analytics/data (./Analytics/requirements-data.txt)
  - Analytics/vt (./Analytics/requirements-vt.txt)
```

Scripts containing `vt` in their name (`script_vt.py`, `script_vt_bulk.py`) are mapped to the `Analytics/vt` subgroup. Those containing `data` (`data_fetch.py`) are mapped to `Analytics/data`.

```bash
# Uses the Analytics/vt venv
adiboupk run ./Analytics/script_vt.py hash123

# Uses the Analytics/data venv
adiboupk run ./Analytics/data_fetch.py ip 1.2.3.4
```

---

## Scenario 4 — Orchestrator Integration

### Node.js / XSOAR

```javascript
const { execSync } = require('child_process');

// Before — global python
// const result = execSync('python ./Analytics/data_fetch.py ' + hostname);

// After — isolated per group
const result = execSync('adiboupk run ./Analytics/data_fetch.py ' + hostname);
```

### Bash / cron

```bash
#!/bin/bash
# Before
# python3 ./Analytics/data_fetch.py "$1"

# After
adiboupk run ./Analytics/data_fetch.py "$1"
```

### Wrapper Scripts

Wrappers are provided in `wrappers/` to transparently replace `python`:

=== "Bash"

    ```bash
    # wrappers/adiboupk-python.sh
    # Replaces 'python' with 'adiboupk run' if an adiboupk.json exists
    #!/bin/bash
    if [ -f "adiboupk.json" ] && command -v adiboupk &>/dev/null; then
        adiboupk run "$@"
    else
        python3 "$@"
    fi
    ```

=== "PowerShell"

    ```powershell
    # wrappers/adiboupk-python.ps1
    if ((Test-Path "adiboupk.json") -and (Get-Command adiboupk -ErrorAction SilentlyContinue)) {
        adiboupk run @args
    } else {
        python @args
    }
    ```

---

## Daily Workflow

```mermaid
graph TD
    A[New project] -->|"adiboupk setup"| B[Project configured]
    B -->|"adiboupk run script.py"| C[Isolated execution]

    B -->|"Edit requirements.txt"| D[Dependencies changed]
    D -->|"adiboupk install"| B

    B -->|"Add a module"| E[New directory]
    E -->|"adiboupk update"| B

    B -->|"New version available"| F[adiboupk outdated]
    F -->|"adiboupk upgrade"| B

    B -->|"Project done"| G[Cleanup]
    G -->|"adiboupk clean"| H[Venvs removed]
    G -->|"adiboupk uninstall"| I[Everything removed]
```

---

## Command Summary by Stage

| Stage | Command | Frequency |
|---|---|---|
| First setup | `adiboupk setup` | Once |
| Run a script | `adiboupk run script.py` | Daily |
| After editing requirements | `adiboupk install` | As needed |
| After adding/removing modules | `adiboupk update` | As needed |
| Check status | `adiboupk status` | As needed |
| Detect conflicts | `adiboupk audit` | As needed |
| Update adiboupk | `adiboupk upgrade` | As needed |
| Clean venvs | `adiboupk clean` | As needed |
