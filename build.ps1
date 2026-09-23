<#
.SYNOPSIS
  Build the oklchRamp Maya plug-in (Release) into module\plug-ins\oklchRamp.mll.

.PARAMETER Install
  Also write oklchRamp.mod (with an absolute path to this folder's "module" dir)
  into %USERPROFILE%\Documents\maya\2025\modules so Maya picks it up on startup.

.PARAMETER MayaLocation
  Maya install root. Default: $env:MAYA_LOCATION if set, otherwise
  C:\Program Files\Autodesk\Maya2025

.PARAMETER MtoaLocation
  MtoA (Arnold for Maya) install root. Default: $env:MTOA_LOCATION if set, otherwise
  C:\Program Files\Autodesk\Arnold\Maya2025. The Arnold translator is skipped if not found.
#>
param(
    [switch]$Install,
    [string]$MayaLocation = $(if ($env:MAYA_LOCATION) { $env:MAYA_LOCATION } else { "C:\Program Files\Autodesk\Maya2025" }),
    [string]$MtoaLocation = $(if ($env:MTOA_LOCATION) { $env:MTOA_LOCATION } else { "C:\Program Files\Autodesk\Arnold\Maya2025" }),
    [string]$Config = "Release"
)
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$build = Join-Path $root "build"

if (-not (Test-Path (Join-Path $MayaLocation "include\maya\MPxNode.h"))) {
    throw "Maya headers not found under '$MayaLocation'. Pass -MayaLocation or set MAYA_LOCATION."
}
cmake -S $root -B $build -G "Visual Studio 17 2022" -A x64 `
    -DMAYA_LOCATION="$($MayaLocation -replace '\\','/')" `
    -DMTOA_LOCATION="$($MtoaLocation -replace '\\','/')"
if ($LASTEXITCODE) { throw "cmake configure failed" }
cmake --build $build --config $Config
if ($LASTEXITCODE) { throw "cmake build failed" }

Write-Host "Built: $(Join-Path $root 'module\plug-ins\oklchRamp.mll')"

if ($Install) {
    $modDir = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "maya\2025\modules"
    New-Item -ItemType Directory -Force $modDir | Out-Null
    $modulePath = Join-Path $root "module"
    $modFile = Join-Path $modDir "oklchRamp.mod"
    @"
+ MAYAVERSION:2025 PLATFORM:win64 oklchRamp 1.0.0 $modulePath
plug-ins: plug-ins
scripts: scripts
MTOA_EXTENSIONS_PATH +:= arnold
"@ | Set-Content -Path $modFile -Encoding ascii
    Write-Host "Installed module file: $modFile"
    Write-Host "Restart Maya, then: loadPlugin oklchRamp;"
}
