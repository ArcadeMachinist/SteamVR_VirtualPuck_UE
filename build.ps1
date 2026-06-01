# Build ArcadeCabVR SteamVR driver
# Usage: .\build.ps1 [Release|Debug]

param([string]$Config = "Release")

$cmakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmakeCmd) {
    Write-Error "cmake not found. Install via: winget install Kitware.CMake"
    exit 1
}
$cmakePath = $cmakeCmd.Source

$root = $PSScriptRoot
$buildDir = "$root\build"

Write-Host "Configuring ($Config)..."
& $cmakePath -S $root -B $buildDir -G "Visual Studio 18 2026" -A x64
if ($LASTEXITCODE -ne 0) { Write-Error "CMake configure failed"; exit 1 }

Write-Host "Building..."
& $cmakePath --build $buildDir --config $Config
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

Write-Host "Done. Driver staged at: $root\driver_stage\arcadecabvr"
