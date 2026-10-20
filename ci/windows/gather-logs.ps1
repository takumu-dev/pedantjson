# Copies the logs a failed job leaves into a directory, for CircleCI to keep: each build's test logs
# and vcpkg install log, under their paths in the checkout, and vcpkg's build logs, under vcpkg/.
param(
    [Parameter(Mandatory)]
    [string] $Destination
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

foreach ($pattern in 'build/*/Testing/Temporary', 'build/*/vcpkg-manifest-install.log') {
    foreach ($item in @(Get-Item -Path $pattern -ErrorAction SilentlyContinue)) {
        $relative = [IO.Path]::GetRelativePath($PWD.Path, $item.FullName)
        $target = Join-Path $Destination (Split-Path -Parent $relative)
        New-Item -ItemType Directory -Force -Path $target | Out-Null
        Copy-Item -Recurse -Force -Path $item.FullName -Destination $target
    }
}
$buildLogs = Join-Path $env:VCPKG_ROOT 'buildtrees/*/*.log'
foreach ($log in @(Get-Item -Path $buildLogs -ErrorAction SilentlyContinue)) {
    $target = Join-Path $Destination "vcpkg/$($log.Directory.Name)"
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    Copy-Item -Force -Path $log.FullName -Destination $target
}
