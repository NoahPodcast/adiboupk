# Install adiboupk from source on Windows.
# Usage (PowerShell as Admin):
#   irm https://raw.githubusercontent.com/NoahPodcast/adiboupk/main/install.ps1 | iex
# Or:
#   git clone ... ; cd adiboupk ; .\install.ps1

$ErrorActionPreference = "Stop"
$InstallDir = if ($env:INSTALL_DIR) { $env:INSTALL_DIR } else { "$env:LOCALAPPDATA\adiboupk" }

function Info($msg) { Write-Host "==> $msg" -ForegroundColor Cyan }
function Error($msg) { Write-Host "==> $msg" -ForegroundColor Red }

# --- Detect if we're inside the repo or need to clone ---
$Cloned = $false
if ((Test-Path "CMakeLists.txt") -and (Select-String -Path "CMakeLists.txt" -Pattern "project\(adiboupk" -Quiet)) {
    $RepoDir = Get-Location
} else {
    $RepoDir = Join-Path $env:TEMP "adiboupk-install-$(Get-Random)"
    $Cloned = $true
    Info "Cloning adiboupk..."
    git clone --depth 1 https://github.com/NoahPodcast/adiboupk.git $RepoDir
    if ($LASTEXITCODE -ne 0) { Error "git clone failed"; exit 1 }
}

# --- Install build dependencies ---
$NeedCmake = -not (Get-Command cmake -ErrorAction SilentlyContinue)
$NeedVS = -not (Get-Command cl -ErrorAction SilentlyContinue)

if ($NeedCmake -or $NeedVS) {
    if (Get-Command winget -ErrorAction SilentlyContinue) {
        if ($NeedCmake) {
            Info "Installing CMake (winget)..."
            winget install --id Kitware.CMake --accept-package-agreements --accept-source-agreements --silent
        }
        if ($NeedVS) {
            Info "Installing Visual Studio Build Tools (winget)..."
            winget install --id Microsoft.VisualStudio.2022.BuildTools --accept-package-agreements --accept-source-agreements --silent `
                --override "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --quiet --wait"
        }
    } elseif (Get-Command choco -ErrorAction SilentlyContinue) {
        if ($NeedCmake) {
            Info "Installing CMake (choco)..."
            choco install cmake --installargs 'ADD_CMAKE_TO_PATH=System' -y | Out-Null
        }
        if ($NeedVS) {
            Info "Installing Visual Studio Build Tools (choco)..."
            choco install visualstudio2022buildtools --package-parameters "--add Microsoft.VisualStudio.Workload.VCTools" -y | Out-Null
        }
    } else {
        Error "cmake or Visual Studio Build Tools not found."
        Error "Install them manually or install winget/chocolatey first."
        Error "  CMake:    https://cmake.org/download/"
        Error "  VS Build: https://visualstudio.microsoft.com/visual-cpp-build-tools/"
        exit 1
    }
    # Refresh PATH
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
}

# --- Check Python ---
if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    Info "Python not found. Installing via winget..."
    if (Get-Command winget -ErrorAction SilentlyContinue) {
        winget install --id Python.Python.3.12 --accept-package-agreements --accept-source-agreements --silent
    } elseif (Get-Command choco -ErrorAction SilentlyContinue) {
        choco install python -y | Out-Null
    } else {
        Error "Python not found. Install from https://www.python.org/downloads/"
        exit 1
    }
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
}

# --- Build ---
Info "Building adiboupk..."
Push-Location $RepoDir
try {
    cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF 2>&1 | Out-Null
    cmake --build build --config Release 2>&1 | Out-Null
} catch {
    Error "Build failed: $_"
    exit 1
}

# --- Install ---
Info "Installing to $InstallDir..."
New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null

$BinaryPath = "build\Release\adiboupk.exe"
if (-not (Test-Path $BinaryPath)) {
    $BinaryPath = "build\adiboupk.exe"
}
if (-not (Test-Path $BinaryPath)) {
    Error "Build succeeded but binary not found"
    exit 1
}

Copy-Item $BinaryPath "$InstallDir\adiboupk.exe" -Force
Pop-Location

# --- Add to PATH ---
$UserPath = [System.Environment]::GetEnvironmentVariable("Path", "User")
if ($UserPath -notlike "*$InstallDir*") {
    Info "Adding $InstallDir to user PATH..."
    [System.Environment]::SetEnvironmentVariable("Path", "$UserPath;$InstallDir", "User")
    $env:Path += ";$InstallDir"
}

# --- Cleanup ---
if ($Cloned) {
    Remove-Item -Recurse -Force $RepoDir
}

Info "Done! adiboupk installed to $InstallDir\adiboupk.exe"
Write-Host ""
Write-Host "  Usage:"
Write-Host "    cd your-project\"
Write-Host "    adiboupk setup"
Write-Host "    adiboupk run .\ModuleA\script.py"
Write-Host ""

# Verify
& "$InstallDir\adiboupk.exe" version
