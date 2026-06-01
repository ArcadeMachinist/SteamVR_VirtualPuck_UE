# Install ArcadeCabVR driver to SteamVR
# IMPORTANT: Close SteamVR before running this script.

$stagePath   = "$PSScriptRoot\driver_stage\arcadecabvr"
$driversPath = "C:\Program Files (x86)\Steam\steamapps\common\SteamVR\drivers"
$destPath    = "$driversPath\arcadecabvr"

if (-not (Test-Path $stagePath)) {
    Write-Error "Staged driver not found at: $stagePath. Run .\build.ps1 first."
    exit 1
}

if (-not (Test-Path $driversPath)) {
    Write-Error "SteamVR drivers directory not found: $driversPath. Is SteamVR installed?"
    exit 1
}

$vrserver = Get-Process vrserver -ErrorAction SilentlyContinue
if ($vrserver) {
    Write-Error "SteamVR (vrserver.exe) is running. Close SteamVR completely before installing."
    exit 1
}

# Remove old vrplay driver if still present
$oldPath = "$driversPath\vrplay"
if (Test-Path $oldPath) {
    Write-Host "Removing old vrplay driver..."
    Remove-Item -Recurse -Force $oldPath
}

$destConfig  = "$destPath\config.json"
$srcConfig   = "$stagePath\config.json"
$savedConfig = $null

if (Test-Path $destConfig) {
    $savedConfig = Get-Content $destConfig -Raw
}

if (Test-Path $destPath) {
    Write-Host "Removing previous arcadecabvr install..."
    Remove-Item -Recurse -Force $destPath
}

robocopy $stagePath $destPath /E /NFL /NDL /NJH /NJS /XF config.json | Out-Null

if ($null -ne $savedConfig) {
    Set-Content -Path $destConfig -Value $savedConfig -NoNewline
    Write-Host "NOTE: config.json already existed - not overwriting. Edit it manually if needed:"
    Write-Host "  $destConfig"
} elseif (Test-Path $srcConfig) {
    Copy-Item $srcConfig $destConfig
    Write-Host "config.json installed."
}

if (-not (Test-Path "$destPath\bin\win64\driver_arcadecabvr.dll")) {
    Write-Error "Install failed. DLL not found at destination."
    exit 1
}

Write-Host "Installed to: $destPath"
Write-Host "Start SteamVR. The ArcadeCabVR tracker will appear in the device list."
