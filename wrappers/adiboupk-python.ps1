# Wrapper that replaces "python" calls with adiboupk-routed calls.
# Usage: .\adiboupk-python.ps1 script.py [args...]
#
# Install by creating a function in your PowerShell profile:
#   function python { & "C:\path\to\adiboupk-python.ps1" @args }

param(
    [Parameter(Position=0, Mandatory=$true)]
    [string]$Script,

    [Parameter(Position=1, ValueFromRemainingArguments=$true)]
    [string[]]$Arguments
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Adiboupk = Join-Path $ScriptDir "..\build\Release\adiboupk.exe"

if (-not (Test-Path $Adiboupk)) {
    $Adiboupk = Join-Path $ScriptDir "..\build\adiboupk.exe"
}

if (-not (Test-Path $Adiboupk)) {
    # Try system PATH
    $Adiboupk = Get-Command adiboupk -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
}

if (-not $Adiboupk -or -not (Test-Path $Adiboupk)) {
    Write-Error "adiboupk: binary not found. Build with 'cmake --build build' or install to PATH."
    exit 1
}

$allArgs = @("run", $Script) + $Arguments
& $Adiboupk @allArgs
exit $LASTEXITCODE
