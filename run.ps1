# Macrolite - launch the built application.
# Usage:  .\run.ps1
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

$exe = Get-ChildItem -Path build -Recurse -Filter 'macrolite.exe' -ErrorAction SilentlyContinue |
       Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $exe) {
    Write-Host "macrolite.exe not found. Build it first:  .\build.ps1" -ForegroundColor Yellow
    exit 1
}
Write-Host "Starting $($exe.FullName)"
Start-Process -FilePath $exe.FullName
