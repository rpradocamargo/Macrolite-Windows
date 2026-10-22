# Macrolite - build and run the automated tests.
# Usage:  .\test.ps1
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

& "$PSScriptRoot\build.ps1"
if ($LASTEXITCODE -ne 0) { exit 1 }

$exe = Get-ChildItem -Path build -Recurse -Filter 'macrolite_tests.exe' -ErrorAction SilentlyContinue |
       Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $exe) { Write-Host "macrolite_tests.exe not found." -ForegroundColor Red; exit 1 }

Write-Host ""
Write-Host "== Running tests ==" -ForegroundColor Cyan
& $exe.FullName
if ($LASTEXITCODE -ne 0) { Write-Host "TESTS FAILED" -ForegroundColor Red; exit 1 }
Write-Host "ALL TESTS PASSED" -ForegroundColor Green
