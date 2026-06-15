<#
.SYNOPSIS
    Checks for the OpenCV dependency required to build this project.

.DESCRIPTION
    OpenCV is large and license-restricted to redistribute, so it is not
    vendored in the repository. This script only verifies whether a usable
    OpenCV build is reachable and prints guidance otherwise.
#>
[CmdletBinding()]
param(
    [string]$OpenCVDir = $env:OpenCV_DIR
)

if ($OpenCVDir -and (Test-Path (Join-Path $OpenCVDir 'OpenCVConfig.cmake'))) {
    Write-Host "[ok] OpenCV found at $OpenCVDir"
} else {
    Write-Host "[!!] OpenCV not located." -ForegroundColor Yellow
    Write-Host "     Install OpenCV (https://opencv.org/releases/) and configure with:"
    Write-Host "       cmake -S . -B build -DOpenCV_DIR=<path-to>/opencv/build"
}

Write-Host ""
Write-Host "Then build with:"
Write-Host "  cmake --build build --config Release"
