# Install adiboupk from source on Windows.
# Usage (PowerShell):
#   irm https://raw.githubusercontent.com/NoahPodcast/adiboupk/main/install.ps1 | iex
# Or:
#   git clone ... ; cd adiboupk ; .\install.ps1

$ErrorActionPreference = "Stop"
$InstallDir = if ($env:INSTALL_DIR) { $env:INSTALL_DIR } else { "$env:LOCALAPPDATA\adiboupk" }

function Info($msg) { Write-Host "==> $msg" -ForegroundColor Cyan }
function Warn($msg) { Write-Host "==> $msg" -ForegroundColor Yellow }
function Error($msg) { Write-Host "==> $msg" -ForegroundColor Red }

function Refresh-Path {
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
}

# --- Detect if we're inside the repo or need to clone ---
$Cloned = $false
if ((Test-Path "CMakeLists.txt") -and (Select-String -Path "CMakeLists.txt" -Pattern "project\(adiboupk" -Quiet)) {
    $RepoDir = (Get-Location).Path
} else {
    $RepoDir = Join-Path $env:TEMP "adiboupk-install-$(Get-Random)"
    $Cloned = $true
    Info "Cloning adiboupk..."
    git clone --depth 1 https://github.com/NoahPodcast/adiboupk.git $RepoDir
    if ($LASTEXITCODE -ne 0) { Error "git clone failed"; exit 1 }
}

# --- Detect available C++ compiler ---
# Priority: g++ (MinGW) > cl (MSVC)
$Compiler = $null
$Generator = $null

if (Get-Command g++ -ErrorAction SilentlyContinue) {
    $Compiler = "mingw"
    $Generator = "MinGW Makefiles"
} elseif (Get-Command cl -ErrorAction SilentlyContinue) {
    $Compiler = "msvc"
}

# --- Install build dependencies ---
$NeedCmake = -not (Get-Command cmake -ErrorAction SilentlyContinue)

if ($NeedCmake -or -not $Compiler) {
    $HasWinget = [bool](Get-Command winget -ErrorAction SilentlyContinue)
    $HasChoco = [bool](Get-Command choco -ErrorAction SilentlyContinue)

    if (-not $HasWinget -and -not $HasChoco) {
        Error "No package manager found (winget or chocolatey)."
        Error "Install manually:"
        Error "  CMake:  https://cmake.org/download/"
        Error "  MinGW:  https://www.mingw-w64.org/ or 'winget install mingw'"
        exit 1
    }

    if ($NeedCmake) {
        Info "Installing CMake..."
        if ($HasWinget) {
            winget install --id Kitware.CMake --accept-package-agreements --accept-source-agreements --silent
        } else {
            choco install cmake --installargs 'ADD_CMAKE_TO_PATH=System' -y | Out-Null
        }
        Refresh-Path
    }

    if (-not $Compiler) {
        Info "Installing MinGW (g++)..."

        # Check if MSYS2 is already installed (e.g. from a previous attempt)
        $msys2Bash = "C:\msys64\usr\bin\bash.exe"
        $mingwBin = "C:\msys64\mingw64\bin"

        if (-not (Test-Path $msys2Bash)) {
            if ($HasWinget) {
                winget install --id MSYS2.MSYS2 --accept-package-agreements --accept-source-agreements --silent
            } elseif ($HasChoco) {
                choco install msys2 -y | Out-Null
            }
            Refresh-Path
        }

        if (Test-Path $msys2Bash) {
            Info "Installing gcc toolchain via MSYS2 pacman..."
            # MSYS2 needs initialization on first run — run bash -lc to trigger it
            # Then install the mingw64 gcc toolchain and make
            & $msys2Bash -lc "pacman --noconfirm -Syuu" 2>&1 | Out-Null
            & $msys2Bash -lc "pacman --noconfirm -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-make make" 2>&1 | Out-Null

            if (Test-Path $mingwBin) {
                $env:Path = "$mingwBin;C:\msys64\usr\bin;$env:Path"
            }
        } elseif ($HasChoco) {
            # Fallback: standalone MinGW via chocolatey
            Info "Falling back to chocolatey MinGW..."
            choco install mingw -y | Out-Null
            Refresh-Path
        }

        Refresh-Path

        # Re-check
        if (Get-Command g++ -ErrorAction SilentlyContinue) {
            $Compiler = "mingw"
            $Generator = "MinGW Makefiles"
        } elseif (Get-Command cl -ErrorAction SilentlyContinue) {
            $Compiler = "msvc"
        } else {
            Error "Could not find a C++ compiler after installation."
            Error ""
            Error "MSYS2 was installed but g++ is not in PATH."
            Error "Open 'MSYS2 MinGW x64' from the Start menu and run:"
            Error "  pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake make"
            Error ""
            Error "Then re-run this installer."
            exit 1
        }
    }
}

# --- Check Python ---
if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    Info "Python not found. Installing..."
    if (Get-Command winget -ErrorAction SilentlyContinue) {
        winget install --id Python.Python.3.12 --accept-package-agreements --accept-source-agreements --silent
    } elseif (Get-Command choco -ErrorAction SilentlyContinue) {
        choco install python -y | Out-Null
    } else {
        Error "Python not found. Install from https://www.python.org/downloads/"
        exit 1
    }
    Refresh-Path
}

# --- Build ---
Info "Building adiboupk with $Compiler..."
Push-Location $RepoDir

$BuildLog = Join-Path $env:TEMP "adiboupk-build-$(Get-Random).log"

# MinGW: ensure mingw32-make is available as 'make' for CMake
if ($Compiler -eq "mingw") {
    $mingw32make = Get-Command mingw32-make -ErrorAction SilentlyContinue
    $hasMake = Get-Command make -ErrorAction SilentlyContinue
    if ($mingw32make -and -not $hasMake) {
        $makeDir = Split-Path $mingw32make.Source
        $makeCopy = Join-Path $makeDir "make.exe"
        if (-not (Test-Path $makeCopy)) {
            Copy-Item $mingw32make.Source $makeCopy -ErrorAction SilentlyContinue
        }
    }
}

try {
    $cmakeArgs = @("-B", "build", "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_TESTS=OFF")
    if ($Generator) {
        $cmakeArgs += @("-G", $Generator)
    }

    $output = & cmake @cmakeArgs 2>&1
    $output | Out-File $BuildLog
    if ($LASTEXITCODE -ne 0) {
        Error "CMake configure failed:"
        Get-Content $BuildLog | Select-Object -Last 20
        exit 1
    }

    $output = & cmake --build build --config Release 2>&1
    $output | Out-File $BuildLog -Append
    if ($LASTEXITCODE -ne 0) {
        Error "Build failed:"
        Get-Content $BuildLog | Select-Object -Last 20
        exit 1
    }
} finally {
    Remove-Item $BuildLog -ErrorAction SilentlyContinue
}

# --- Find the binary ---
$BinaryPath = $null
foreach ($candidate in @("build\adiboupk.exe", "build\Release\adiboupk.exe")) {
    if (Test-Path $candidate) {
        $BinaryPath = $candidate
        break
    }
}

if (-not $BinaryPath) {
    Error "Build succeeded but binary not found."
    Error "Contents of build/:"
    Get-ChildItem build -Recurse -Filter "*.exe" | ForEach-Object { Error "  $($_.FullName)" }
    exit 1
}

# --- Install ---
Info "Installing to $InstallDir..."
New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
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
    Remove-Item -Recurse -Force $RepoDir -ErrorAction SilentlyContinue
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
