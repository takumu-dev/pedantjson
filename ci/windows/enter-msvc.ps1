# Sets up the process it runs in as a developer command prompt for MSVC would be: runs vcvarsall.bat
# for x64, at the toolset named or else at the newest the machine carries, and sets the variables it
# sets or changes, all but VCPKG_ROOT, for the rest of the process.
param(
    # The MSVC toolset to set up, such as 14.44. Without it, the newest installed.
    [ValidatePattern('^14\.\d{2}$')]
    [string] $Version
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) {
    throw 'enter-msvc: no Visual Studio with the C++ tools is installed'
}
$vcvarsall = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$arguments = if ($Version) { "x64 -vcvars_ver=$Version" } else { 'x64' }

$before = @{}
Get-ChildItem env: | ForEach-Object { $before[$_.Name] = $_.Value }

$lines = cmd /c "`"$vcvarsall`" $arguments && set"
if ($LASTEXITCODE -ne 0) {
    $lines | Write-Output
    throw "enter-msvc: vcvarsall.bat couldn't set up MSVC $Version"
}

$toolset = $null
foreach ($line in $lines) {
    if ($line -notmatch '^([^=]+)=(.*)$') {
        continue
    }
    $name, $value = $Matches[1], $Matches[2]
    if ($name -eq 'VCToolsVersion') {
        $toolset = $value
    }
    # vcvarsall.bat points VCPKG_ROOT at the vcpkg Visual Studio bundles, over the job's own.
    if ($name -eq 'VCPKG_ROOT') {
        continue
    }
    # The names of environment variables don't depend on case on Windows, and neither does
    # this table's lookup.
    if (-not $before.ContainsKey($name) -or $before[$name] -cne $value) {
        Set-Item -Path "env:$name" -Value $value
    }
}
if (-not $toolset) {
    throw "enter-msvc: vcvarsall.bat set up no MSVC toolset"
}
if ($Version -and $toolset -notlike "$Version.*") {
    throw "enter-msvc: vcvarsall.bat set up MSVC $toolset, not $Version"
}
Write-Output "enter-msvc: MSVC $toolset, from $vs"
