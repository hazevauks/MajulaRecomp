# Lays out the Switch SD card folder in out\nx\sdcard\switch\ds2-nx: the NRO, its settings (app\ds2_switch.toml as
# ds2.toml), the testing notes (README.md), the shader package and two fallback configurations. The game files are yours and stay
# out of the repository:
#   -IncludeGame              adds them as the asset cache (assets\game_root.nxpack + .nxidx, built here with
#                             tools\build_asset_pack.py when missing or older than the folder);
#   -IncludeGame -LooseGame   copies the game_root folder instead: the console builds the pack on its first boot.
# By hand:
#   sdmc:/switch/ds2-nx/game_root.nxpack + game_root.nxidx   <- python tools\build_asset_pack.py assets\game_root
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\win\package_switch.ps1 [-IncludeGame [-LooseGame]]
param([switch]$IncludeGame, [switch]$LooseGame)
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$dest = Join-Path $root 'out\nx\sdcard\switch\ds2-nx'
New-Item -ItemType Directory -Force $dest | Out-Null
$nro = Join-Path $root 'out\nx\ds2.nro'
if (-not (Test-Path $nro)) { throw "no NRO at ${nro}: run tools\win\build_switch.ps1 first" }
Copy-Item -Force $nro (Join-Path $dest 'ds2.nro')
# The other builds, when they exist (tools\win\build_switch_all.ps1): the PGO-instrumented NRO, which collects the
# profile on the console, and the optimised one (LTO, and PGO when pgo\ has a profile). They read the same ds2.toml,
# game files and saves as ds2.nro.
foreach ($extra in @(@('out\nx_pgo\ds2.nro', 'ds2-pgo.nro'), @('out\nx_rel\ds2.nro', 'ds2-opt.nro'))) {
    $from = Join-Path $root $extra[0]
    if (Test-Path $from) { Copy-Item -Force $from (Join-Path $dest $extra[1]) }
}
Copy-Item -Force (Join-Path $root 'app\ds2_switch.toml') (Join-Path $dest 'ds2.toml')
Copy-Item -Force (Join-Path $root 'docs\switch-testing.md') (Join-Path $dest 'README.md')
# The shader package the PC build was verified with (out\pc), not a stale build output.
foreach ($f in 'ds2_shaders.mesp', 'ds2_shaders.mesp.idx') {
    Copy-Item -Force (Join-Path $root "out\pc\$f") (Join-Path $dest $f)
}
# Fallback configurations for the first hardware tests: copy one over ds2.toml to use it.
$configs = Join-Path $dest 'configs'
New-Item -ItemType Directory -Force $configs | Out-Null
$base = Get-Content (Join-Path $root 'app\ds2_switch.toml') -Raw -Encoding UTF8
# Written without a byte order mark (Windows PowerShell's "utf8" adds one, which a TOML reader may not accept).
$noBom = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText((Join-Path $configs 'ds2_default.toml'), $base, $noBom)
[IO.File]::WriteAllText((Join-Path $configs 'ds2_enhancements_off.toml'), ($base +
    "`n# Every CPU and GPU shortcut inherited from sa2-nx turned off: the original paths everywhere.`n" +
    "ds2_cpu_enhancements = false`nds2_noncpu_enhancements = false`n"), $noBom)
[IO.File]::WriteAllText((Join-Path $configs 'ds2_emulated_gpu.toml'), (
    ($base -replace 'ds2_renderer_native = true', 'ds2_renderer_native = false') +
    "`n# The SDK's emulated GPU instead of the native renderer (very slow on the console; for comparing the picture).`n" +
    "ds2_gpu_backend = `"vulkan`"`n"), $noBom)
[IO.File]::WriteAllText((Join-Path $configs 'ds2_profile.toml'), ($base +
    "`n# Measuring: GPU time per pass category and sampling of the CPU stacks (rex_profile.log). It runs a little`n" +
    "# slower; it is for finding where the time goes.`n" +
    "ds2_gpu_marks_categories = true`nswitch_profile_stacks = true`nswitch_profile_report = true`n"), $noBom)
[IO.File]::WriteAllText((Join-Path $configs 'ds2_test_zcull.toml'), ($base +
    "`n# A/B test: ZCULL on the single-sample depth targets too (the game's main one). The stencil imports then`n" +
    "# use masked passes instead of the copy engine.`n" +
    "ds2_native_zcull_1x_depth = true`n"), $noBom)
[IO.File]::WriteAllText((Join-Path $configs 'ds2_test_no_clear_shortcut.toml'), ($base +
    "`n# A/B test: without the shortcut for the clears through 4x surfaces (round 2): as in the build before it.`n" +
    "ds2_native_edram4_clear_color0_alias = false`n"), $noBom)
[IO.File]::WriteAllText((Join-Path $configs 'ds2_test_compute_conversion.toml'), (
    ($base -replace 'ds2_native_conversion_frag = true', 'ds2_native_conversion_frag = false') +
    "`n# A/B test: the HDR target's format conversions by compute shader, as up to the fifteenth build.`n"), $noBom)
[IO.File]::WriteAllText((Join-Path $configs 'ds2_test_720p.toml'), (
    ($base -replace 'ds2_render_height_handheld = 540', 'ds2_render_height_handheld = 720') +
    "`n# A/B test: handheld mode at the game's own resolution (1280x720), as in the earlier builds.`n"), $noBom)
if ($IncludeGame) {
    if ($LooseGame) {
        robocopy (Join-Path $root 'assets\game_root') (Join-Path $dest 'game_root') /E /NFL /NDL /NJH /NJS | Out-Null
        foreach ($f in 'game_root.nxpack', 'game_root.nxidx') {
            Remove-Item -Force (Join-Path $dest $f) -ErrorAction SilentlyContinue
        }
    } else {
        & python (Join-Path $root 'tools\build_asset_pack.py') (Join-Path $root 'assets\game_root') --if-stale
        if ($LASTEXITCODE) { throw 'asset pack build failed' }
        foreach ($f in 'game_root.nxpack', 'game_root.nxidx') {
            Copy-Item -Force (Join-Path $root "assets\$f") (Join-Path $dest $f)
        }
        # A loose folder from an earlier package is no longer read (the pack wins): drop it from the staging copy.
        if (Test-Path (Join-Path $dest 'game_root')) { Remove-Item -Recurse -Force (Join-Path $dest 'game_root') }
    }
}
Get-ChildItem $dest | Select-Object Name, Length
