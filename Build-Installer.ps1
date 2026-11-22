# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only

param(
    [string]$BuildDirectory = 'build',
    [string]$Iscc,
    [string]$CMake = 'cmake'
)

$ErrorActionPreference = 'Stop'
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$content = Get-Content -LiteralPath "$PSScriptRoot/CMakeLists.txt" -Raw
if ($content -notmatch 'project\(CyMCPHub VERSION (\d+\.\d+\.\d+) ') { throw 'Could not read the application version.' }
$version = $Matches[1]

if (!$Iscc) {
    $Iscc = (Get-Command ISCC.exe -ErrorAction SilentlyContinue).Source
    if (!$Iscc) {
        $Iscc = @("${env:ProgramFiles(x86)}/Inno Setup 6/ISCC.exe", "$env:LOCALAPPDATA/Programs/Inno Setup 6/ISCC.exe") |
            Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
    }
}
if (!$Iscc -or !(Test-Path -LiteralPath $Iscc -PathType Leaf)) { throw 'Provide the Inno Setup 6 compiler path with -Iscc.' }

& $CMake --build $build --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw 'Application compilation failed.' }
$stage = Join-Path $build ('installer-stage-' + [guid]::NewGuid())
$output = Join-Path $PSScriptRoot out
& $CMake --install $build --config Release --component Hub --prefix $stage
if ($LASTEXITCODE -ne 0) { throw 'Installer staging failed.' }
& $Iscc "/DAppVersion=$version" "/DStagePath=$stage" "/DOutputPath=$output" "$PSScriptRoot/packaging/CyMCPHub.iss"
if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed.' }
