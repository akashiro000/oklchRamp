<#
.SYNOPSIS
  Run one of the GUI tests (tests/gui/*.py) inside a throwaway Maya GUI instance.
  Viewport 2.0 rendering, scriptJob/idle behaviour and MtoA export only work in a
  real GUI session, not in mayapy.

.PARAMETER Test
  vp2     - renders the VP2 override with ogsRender  -> <out>/vp2_test.png
  watch   - checks oklchRampWatch fires / coalesces  -> <out>/watch_test.log
  arnold  - exports .ass via MtoA, then renders with kick (watermarked) -> <out>/arnold_test.png

.PARAMETER MayaLocation / MtoaLocation
  Defaults: env MAYA_LOCATION / MTOA_LOCATION, else the Program Files paths.

.PARAMETER OutDir
  Where logs / images go. Default: tests/out (gitignored).
#>
param(
    [Parameter(Mandatory)][ValidateSet("vp2", "watch", "arnold")][string]$Test,
    [string]$MayaLocation = $(if ($env:MAYA_LOCATION) { $env:MAYA_LOCATION } else { "C:\Program Files\Autodesk\Maya2025" }),
    [string]$MtoaLocation = $(if ($env:MTOA_LOCATION) { $env:MTOA_LOCATION } else { "C:\Program Files\Autodesk\Arnold\Maya2025" }),
    [string]$OutDir = "",
    [int]$TimeoutSec = 420
)
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
if (-not $OutDir) { $OutDir = Join-Path $PSScriptRoot "out" }
New-Item -ItemType Directory -Force $OutDir | Out-Null

$py  = (Join-Path $PSScriptRoot "gui\$($Test)_test.py") -replace '\\', '/'
$mel = Join-Path $OutDir "$($Test)_test.mel"
@"
global proc oklchGuiTestRun() {
    int `$failed = catch(python("exec(open('$py').read())"));
    if (`$failed) { quit -force; }
}
evalDeferred("oklchGuiTestRun");
"@ | Set-Content -Path $mel -Encoding ascii

$env:OKLCH_TEST_OUT = $OutDir
$env:OKLCH_REPO = $repo
if ($Test -eq "arnold") { $env:MTOA_EXTENSIONS_PATH = Join-Path $repo "module\arnold" }

Remove-Item (Join-Path $OutDir "$($Test)_test.log") -ErrorAction SilentlyContinue
$maya = Join-Path $MayaLocation "bin\maya.exe"
$p = Start-Process -FilePath $maya -ArgumentList @("-hideConsole", "-script", "`"$mel`"") -PassThru
if (-not $p.WaitForExit($TimeoutSec * 1000)) {
    Write-Warning "Maya did not exit within $TimeoutSec s; killing it."
    Stop-Process -Id $p.Id -Force
}
Write-Host "--- $($Test)_test.log"
Get-Content (Join-Path $OutDir "$($Test)_test.log") -ErrorAction SilentlyContinue

if ($Test -eq "arnold") {
    # Render the exported .ass without a license (watermarked). The scene has no
    # lights and references the Maya denoiser, so patch the .ass first.
    $ass = Join-Path $OutDir "arnold_test.ass"
    if (Test-Path $ass) {
        $txt = Get-Content $ass -Raw
        $txt = $txt -replace '(?m)^\s*input "defaultArnoldDenoiser"\s*\r?\n', ''
        $txt = $txt -replace '(?m)^driver_exr$', 'driver_png'
        $txt = $txt -replace '(?m)^ filename ".*\.exr"', ' filename "arnold_test.png"'
        $txt += "`nskydome_light`n{`n name sky`n intensity 1`n}`n"
        $patched = Join-Path $OutDir "arnold_test_png.ass"
        Set-Content -Path $patched -Value $txt -Encoding utf8
        $kick = Join-Path $MtoaLocation "bin\kick.exe"
        Push-Location $OutDir
        & $kick -nokeypress -dw -dp -v 1 -set options.skip_license_check true -set options.abort_on_license_fail false -set options.abort_on_error false -i $patched -r 800 500 -as 3 2>&1 | Select-String -Pattern "ERROR|render done" | Select-Object -First 5
        Pop-Location
        Write-Host "Rendered: $(Join-Path $OutDir 'arnold_test.png')"
    }
}
