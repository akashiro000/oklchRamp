<#
.SYNOPSIS
  Run the mayapy (no GUI) regression tests. Each script prints "ALL OK" on success.
#>
param(
    [string]$MayaLocation = $(if ($env:MAYA_LOCATION) { $env:MAYA_LOCATION } else { "C:\Program Files\Autodesk\Maya2025" })
)
$ErrorActionPreference = "Stop"
$mayapy = Join-Path $MayaLocation "bin\mayapy.exe"
$failed = 0
foreach ($t in Get-ChildItem (Join-Path $PSScriptRoot "headless") -Filter "test_*.py") {
    Write-Host "=== $($t.Name)"
    $out = & $mayapy $t.FullName 2>&1
    $out | Select-String -Pattern "ALL OK|Error|error|Traceback" | ForEach-Object { Write-Host $_ }
    if (-not ($out | Select-String -Quiet -Pattern "ALL OK")) { $failed++ }
}
if ($failed) { throw "$failed headless test(s) failed" }
Write-Host "All headless tests passed."
