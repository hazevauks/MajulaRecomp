# Starts a detached, guided PC session on the software Vulkan driver (lavapipe in _tools\mesa) that walks from the
# title screen into the save's map ("Continue"), with the live tools on: tools\live.py pad / snap / trace.
# The 3D runs at about half a frame a second on a PC without a GPU, so only one frame in 31 is sent to the GPU
# (ds2_diag_draw_every); the map takes about 130 s to load and the first 3D frames a few minutes more.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\win\guided_session.ps1 [-Height 540] [-Seconds 900]
#       [-Validation] [-Extra "REX_DS2_SOME_CVAR=value;DS2_SOME_SWITCH=1"]
#
# Stop it before building (the executable stays locked): taskkill /IM ds2.exe /F
param([int]$Height = 720, [int]$Seconds = 900, [string]$Extra = '', [switch]$Validation)
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Set-Location $root
$icd = Join-Path $root '_tools\mesa\x64\lvp_icd.x86_64.json'
$env:VK_DRIVER_FILES = $icd
$env:VK_ICD_FILENAMES = $icd
$env:REX_DS2_RENDERER_NATIVE = 'true'
$env:REX_DS2_NATIVE_TEXTURES_BINDING_THREAD = 'false'
$env:REX_DS2_NATIVE_ZCULL_1X_DEPTH = 'true'
$env:REX_DS2_RENDER_HEIGHT_HANDHELD = "$Height"
$env:DS2_CHECK_SLOTS = '1'
$env:REX_DS2_INPUT_LIVE = (Join-Path $root 'out\pc\input_live.txt')
$env:REX_DS2_SNAP_LIVE = 'true'
$env:REX_DS2_DIAG_DRAW_EVERY = '31'
$env:DS2_TRACE_LIVE = '1'
$env:DS2_TRACE_DUMP = '1'
if ($Validation) {
    $env:VK_ADD_LAYER_PATH = (Join-Path $root '_tools\vulkansdk\Bin')
    $env:REX_VULKAN_VALIDATION_ENABLED = 'true'
}
foreach ($pair in ($Extra -split ';')) {
    if ($pair -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
}
# A at the warning, START through the title screens, then A every 6 s: "Continue" and whatever asks for confirmation.
$pad = '16-16.6:a,30-30.6:start,42-42.6:start,54-54.6:start'
for ($t = 62; $t -le 160; $t += 6) { $pad += ",$t-$($t + 0.6):a" }
$p = Start-Process powershell -WindowStyle Hidden -PassThru `
    -ArgumentList "-NoProfile -File tools\win\boot_test.ps1 -Seconds $Seconds -Pad $pad"
"started $($p.Id) at $(Get-Date -Format HH:mm:ss)"
