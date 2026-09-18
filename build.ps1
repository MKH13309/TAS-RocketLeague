param(
    [switch]$Deploy
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

if (-not (Test-Path $vswhere)) {
    throw "Visual Studio Build Tools were not found"
}

$buildTools = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $buildTools) {
    throw "The MSVC C++ workload is not installed"
}

$devShell = Join-Path $buildTools "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Import-Module $devShell
Enter-VsDevShell -VsInstallPath $buildTools -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64"
Set-Location $root

cmake --preset ninja-release
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed with exit code $LASTEXITCODE"
}

cmake --build --preset release
if ($LASTEXITCODE -ne 0) {
    throw "Ninja build failed with exit code $LASTEXITCODE"
}

if ($Deploy) {
    $pluginDir = Join-Path $env:APPDATA "bakkesmod\bakkesmod\plugins"
    if (-not (Test-Path $pluginDir)) {
        throw "BakkesMod plugins folder was not found: $pluginDir"
    }
    Copy-Item (Join-Path $root "dist\TAS.dll") $pluginDir -Force
    Write-Host "Deployed TAS.dll to $pluginDir"
}
