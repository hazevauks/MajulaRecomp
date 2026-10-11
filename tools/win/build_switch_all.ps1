# Builds the three Switch NROs one after another, for a machine left alone for some hours (each full build of the game
# code takes about 100 minutes on a 2-core PC, the LTO link more):
#   1. out\nx       the everyday build (no LTO, no PGO)                         -> out\nx\ds2.nro
#   2. out\nx_pgo   the PGO-instrumented build (collects counts on the console) -> out\nx_pgo\ds2.nro
#   3. out\nx_rel   the optimised build: LTO, and PGO "use" when pgo\ has a profile -> out\nx_rel\ds2.nro
# A step that fails is logged and the next one still runs. Progress: out\build_switch_all.log.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\win\build_switch_all.ps1 [-Steps 1,2,3] [-Jobs 4] [-LtoJobs 2]
# NVK_SDK (environment) as for tools\win\build_switch.ps1.
param([int[]]$Steps = @(1, 2, 3), [int]$Jobs = 4, [int]$LtoJobs = 2)
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$log = Join-Path $root 'out\build_switch_all.log'
$build = Join-Path $PSScriptRoot 'build_switch.ps1'
function Step([int]$n, [string]$name, [string]$dir, [string[]]$more) {
    if ($Steps -notcontains $n) { return }
    $started = Get-Date
    Add-Content $log "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') step ${n} (${name}): start"
    $stepLog = Join-Path $root "out\build_switch_all_$n.log"
    & powershell -NoProfile -ExecutionPolicy Bypass -File $build -Jobs $Jobs -LtoJobs $LtoJobs -BuildDir $dir @more *> $stepLog
    $code = $LASTEXITCODE
    $minutes = [math]::Round(((Get-Date) - $started).TotalMinutes, 1)
    $nro = Join-Path $root "$dir\ds2.nro"
    $size = if (Test-Path $nro) { (Get-Item $nro).Length } else { 0 }
    Add-Content $log "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') step ${n} (${name}): exit $code after $minutes min, NRO $size bytes"
}
Add-Content $log "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') ---- build_switch_all: steps $($Steps -join ',')"
Step 1 'everyday' 'out\nx' @()
Step 2 'PGO generate' 'out\nx_pgo' @('-Pgo', 'generate')
$rel = @('-Lto')
if (Get-ChildItem (Join-Path $root 'pgo') -Filter *.gcda -File -ErrorAction SilentlyContinue | Select-Object -First 1) {
    $rel += @('-Pgo', 'use')
}
Step 3 'optimised' 'out\nx_rel' $rel
Add-Content $log "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') ---- done"
