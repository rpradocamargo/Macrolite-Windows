# Macrolite - environment check. Reports which build tools are installed.
# Usage:  .\check-env.ps1
$ErrorActionPreference = 'Continue'

function Show($name, $found, $detail) {
    if ($found) { Write-Host ("[ OK ]  {0,-22} {1}" -f $name, $detail) -ForegroundColor Green }
    else        { Write-Host ("[MISS]  {0,-22} {1}" -f $name, $detail) -ForegroundColor Yellow }
}

Write-Host ""
Write-Host "Windows: $([System.Environment]::OSVersion.VersionString)"
Write-Host "PowerShell: $($PSVersionTable.PSVersion)"
Write-Host ""

# CMake
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    $vsCmake = Get-ChildItem "$env:ProgramFiles\Microsoft Visual Studio\*\*\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($vsCmake) { $cmake = $vsCmake }
}
if ($cmake) { Show "CMake" $true ($(if ($cmake.Source) { $cmake.Source } else { $cmake.FullName })) }
else        { Show "CMake" $false "not found" }

# Visual Studio / Build Tools with the C++ workload
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = $null
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
}
if ($vsPath) { Show "Visual Studio C++" $true $vsPath }
else         { Show "Visual Studio C++" $false "C++ build tools not found" }

# MinGW
$gpp = Get-Command g++ -ErrorAction SilentlyContinue
if ($gpp) { Show "MinGW g++" $true $gpp.Source } else { Show "MinGW g++" $false "not found (optional)" }

Write-Host ""
if ($cmake -and ($vsPath -or $gpp)) {
    Write-Host "You have what you need. Next:  .\build.ps1" -ForegroundColor Green
} else {
    Write-Host "Something is missing. Install both CMake and the Visual Studio C++ Build Tools with:" -ForegroundColor Yellow
    Write-Host ""
    Write-Host '  winget install --id Kitware.CMake -e'
    Write-Host '  winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"'
    Write-Host ""
    Write-Host "Then CLOSE this PowerShell window, open a new one, and run .\check-env.ps1 again."
}
