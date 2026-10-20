# Sets up vcpkg in VCPKG_ROOT at the builtin-baseline of vcpkg.json, and bootstraps it. The machine
# keeps the clone between jobs, so it is cloned only once and fetched after that. vcpkg reads each
# port's version with git in an environment of its own, which on Windows holds nothing else, so git
# can't reach the network for a blob a partial clone lacks, and the clone is a full one.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

$baseline = (Get-Content vcpkg.json -Raw | ConvertFrom-Json).'builtin-baseline'
if (-not $baseline) {
    throw 'setup-vcpkg: vcpkg.json names no builtin-baseline'
}
if (Test-Path (Join-Path $env:VCPKG_ROOT '.git')) {
    git -C $env:VCPKG_ROOT fetch --quiet origin
} else {
    git clone --quiet https://github.com/microsoft/vcpkg $env:VCPKG_ROOT
}
git -C $env:VCPKG_ROOT checkout --quiet --force $baseline
# An earlier job's build trees stay on the machine with their logs, and a failed job keeps only its
# own.
$buildtrees = Join-Path $env:VCPKG_ROOT 'buildtrees'
if (Test-Path $buildtrees) {
    Remove-Item -Recurse -Force -Path $buildtrees
}
& (Join-Path $env:VCPKG_ROOT 'bootstrap-vcpkg.bat') -disableMetrics
New-Item -ItemType Directory -Force -Path $env:VCPKG_DEFAULT_BINARY_CACHE | Out-Null
