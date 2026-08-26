[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [switch]$BinaryOnly
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDirectory = Join-Path $ProjectRoot "build\windows"

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "cmake is required but was not found in PATH."
}

& cmake -S $ProjectRoot -B $BuildDirectory -A x64
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed."
}

& cmake --build $BuildDirectory --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) {
    throw "CMake build failed."
}

if (-not $BinaryOnly) {
    & (Join-Path $ProjectRoot "scripts\package_windows.ps1") `
        -Configuration $Configuration `
        -BuildDirectory $BuildDirectory `
        -SkipBuild
    if ($LASTEXITCODE -ne 0) {
        throw "Windows packaging failed."
    }
}
