# Macrolite - build script.
# Usage:  .\build.ps1            (Release build)
#         .\build.ps1 -Config Debug
#         .\build.ps1 -Clean     (delete the build folder first)
[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')][string]$Config = 'Release',
    [switch]$Clean
)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

function Fail($msg) { Write-Host ""; Write-Host "BUILD FAILED: $msg" -ForegroundColor Red; exit 1 }

# ---- find CMake -------------------------------------------------------------
$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) {
    $cands = @("$env:ProgramFiles\CMake\bin\cmake.exe")
    $cands += (Get-ChildItem "$env:ProgramFiles\Microsoft Visual Studio\*\*\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -ErrorAction SilentlyContinue | ForEach-Object FullName)
    $cmake = $cands | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
}
if (-not $cmake) { Fail "CMake was not found. Run .\check-env.ps1 for install instructions." }

# ---- pick a generator -------------------------------------------------------
$generator = $null
$extraArgs = @()
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsVersion = $null
if (Test-Path $vswhere) {
    $vsVersion = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion 2>$null
}
if ($vsVersion) {
    $major = [int]($vsVersion.Split('.')[0])
    if     ($major -eq 17) { $generator = 'Visual Studio 17 2022' }
    elseif ($major -eq 16) { $generator = 'Visual Studio 16 2019' }
    else                   { $generator = $null }   # let CMake choose the newest it knows
    $extraArgs = @('-A', 'x64')
    $mode = "Visual Studio $vsVersion"
} elseif (Get-Command g++ -ErrorAction SilentlyContinue) {
    if (Get-Command ninja -ErrorAction SilentlyContinue) { $generator = 'Ninja' }
    else { $generator = 'MinGW Makefiles' }
    $mode = "MinGW ($generator)"
} else {
    Fail "No C++ compiler found. Run .\check-env.ps1 for install instructions."
}
Write-Host "CMake:     $cmake"
Write-Host "Compiler:  $mode"
Write-Host "Config:    $Config"

# ---- configure --------------------------------------------------------------
$buildDir = Join-Path $PSScriptRoot 'build'
if ($Clean -and (Test-Path $buildDir)) { Remove-Item $buildDir -Recurse -Force }

# If an old build folder used a different generator, start fresh.
$cache = Join-Path $buildDir 'CMakeCache.txt'
if ((Test-Path $cache) -and $generator) {
    $old = (Select-String -Path $cache -Pattern '^CMAKE_GENERATOR:INTERNAL=(.*)$' | Select-Object -First 1).Matches.Groups[1].Value
    if ($old -and $old -ne $generator) {
        Write-Host "Generator changed ($old -> $generator); recreating build folder."
        Remove-Item $buildDir -Recurse -Force
    }
}

$configureArgs = @('-S', '.', '-B', 'build')
if ($generator) { $configureArgs += @('-G', $generator) }
$configureArgs += $extraArgs
$configureArgs += "-DCMAKE_BUILD_TYPE=$Config"

Write-Host ""
Write-Host "== Configuring ==" -ForegroundColor Cyan
& $cmake @configureArgs
if ($LASTEXITCODE -ne 0) { Fail "CMake configure step failed (see the messages above)." }

# ---- build ------------------------------------------------------------------
Write-Host ""
Write-Host "== Building ==" -ForegroundColor Cyan
& $cmake --build build --config $Config
if ($LASTEXITCODE -ne 0) { Fail "Compilation failed (see the messages above)." }

$exe = Get-ChildItem -Path build -Recurse -Filter 'macrolite.exe' -ErrorAction SilentlyContinue |
       Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $exe) { Fail "Build reported success but macrolite.exe was not found." }

Write-Host ""
Write-Host "BUILD SUCCEEDED: $($exe.FullName)" -ForegroundColor Green
Write-Host "Next:  .\run.ps1"
