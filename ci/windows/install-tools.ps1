# Installs CMake and Ninja, at the versions named, from pip into a virtual environment at CI_TOOLS.
#
#   ci/windows/install-tools.ps1 -CMake 4.4.4 -Ninja 1.13.2
param(
    [Parameter(Mandatory)]
    [string] $CMake,

    [Parameter(Mandatory)]
    [string] $Ninja
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

python -m venv $env:CI_TOOLS
& (Join-Path $env:CI_TOOLS 'Scripts\python.exe') -m pip install --disable-pip-version-check "cmake==$CMake" "ninja==$Ninja"
