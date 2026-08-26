[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$BuildDirectory,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$ProjectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $ProjectRoot "build\windows"
}

if (-not $SkipBuild) {
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
}

$ExecutableCandidates = @(
    (Join-Path $BuildDirectory "$Configuration\ToolSizeWatcher.exe"),
    (Join-Path $BuildDirectory "ToolSizeWatcher.exe")
)
$ExecutablePath = $ExecutableCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
    Select-Object -First 1
if (-not $ExecutablePath) {
    throw "Built executable not found in $BuildDirectory."
}

$DistDirectory = Join-Path $ProjectRoot "dist"
$PackageDirectory = Join-Path $DistDirectory "ToolSizeWatcher-windows-x64"
$ArchivePath = Join-Path $DistDirectory "ToolSizeWatcher-windows-x64.zip"

New-Item -ItemType Directory -Force -Path $DistDirectory | Out-Null
if (Test-Path -LiteralPath $PackageDirectory) {
    Remove-Item -LiteralPath $PackageDirectory -Recurse -Force
}
if (Test-Path -LiteralPath $ArchivePath) {
    Remove-Item -LiteralPath $ArchivePath -Force
}

New-Item -ItemType Directory -Path $PackageDirectory | Out-Null
Copy-Item -LiteralPath $ExecutablePath -Destination $PackageDirectory
Copy-Item -LiteralPath (Join-Path $ProjectRoot "README.md") -Destination $PackageDirectory
Copy-Item -LiteralPath (Join-Path $ProjectRoot "LICENSE") -Destination $PackageDirectory
Compress-Archive -Path $PackageDirectory -DestinationPath $ArchivePath -CompressionLevel Optimal

Write-Output "Packaged: $PackageDirectory"
Write-Output "Archive:  $ArchivePath"
