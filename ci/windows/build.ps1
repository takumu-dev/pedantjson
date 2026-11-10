# Configures, builds and tests each workflow preset named, in turn, in a developer command prompt
# for MSVC: at the toolset named, or else at the newest the machine carries. Each configures afresh,
# because one job can build a preset's tree with two toolsets in turn.
#
#   ci/windows/build.ps1 -MsvcVersion 14.44 -Presets 'msvc-release msvc-noexcept'
param(
    # The MSVC toolset, such as 14.44, or `latest` for the newest installed.
    [ValidatePattern('^(14\.\d{2}|latest)$')]
    [string] $MsvcVersion = 'latest',

    # The workflow presets, separated by spaces.
    [Parameter(Mandatory)]
    [string] $Presets
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

if ($MsvcVersion -eq 'latest') {
    & (Join-Path $PSScriptRoot 'enter-msvc.ps1')
} else {
    & (Join-Path $PSScriptRoot 'enter-msvc.ps1') -Version $MsvcVersion
}
# Ahead of the CMake and Ninja that Visual Studio's own tools put on the PATH.
$env:PATH = "$(Join-Path $env:CI_TOOLS 'Scripts');$env:PATH"
foreach ($preset in $Presets -split ' ') {
    cmake --workflow --preset $preset --fresh
}
