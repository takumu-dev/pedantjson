# Sets up vcpkg in VCPKG_ROOT at the builtin-baseline of vcpkg.json, and bootstraps it. vcpkg reads
# each port's version with git in an environment of its own, which on Windows holds nothing else, so
# git can't reach the network for a blob a partial clone lacks, and the clone is a full one.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

$baseline = (Get-Content vcpkg.json -Raw | ConvertFrom-Json).'builtin-baseline'
if (-not $baseline) {
    throw 'setup-vcpkg: vcpkg.json names no builtin-baseline'
}
git clone --quiet https://github.com/microsoft/vcpkg $env:VCPKG_ROOT
git -C $env:VCPKG_ROOT checkout --quiet $baseline
& (Join-Path $env:VCPKG_ROOT 'bootstrap-vcpkg.bat') -disableMetrics
New-Item -ItemType Directory -Force -Path $env:VCPKG_DEFAULT_BINARY_CACHE | Out-Null
