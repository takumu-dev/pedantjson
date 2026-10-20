# Configures, builds and tests each workflow preset named, in turn, in a developer command prompt
# for MSVC: at the toolset named, or else at the newest the machine carries.
#
#   ci/windows/build.ps1 -MsvcVersion 14.44 -Presets 'msvc-release msvc-noexcept'
param(
    # The MSVC toolset, such as 14.44, or nothing for the newest installed.
    [ValidatePattern('^(14\.\d{2})?$')]
    [string] $MsvcVersion = '',

    # The workflow presets, separated by spaces.
    [Parameter(Mandatory)]
    [string] $Presets
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

if ($MsvcVersion) {
    & (Join-Path $PSScriptRoot 'enter-msvc.ps1') -Version $MsvcVersion
} else {
    & (Join-Path $PSScriptRoot 'enter-msvc.ps1')
}
foreach ($preset in $Presets -split ' ') {
    cmake --workflow --preset $preset
}
