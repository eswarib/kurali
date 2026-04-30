# Build Kurali C++ backend for Windows - run from PowerShell
# Usage: .\build\build-windows.ps1
# Or from build dir: .\build-windows.ps1

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..")).Path
$batPath = Join-Path $scriptDir "build-windows.bat"

Set-Location $repoRoot
& cmd /c "`"$batPath`""
