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
# ds2-nx (2026-10-23): and the deko3d one (out\nx_dk: Renut on deko3d, no Vulkan driver; docs/deko3d.md).
foreach ($extra in @(@('out\nx_pgo\ds2.nro', 'ds2-pgo.nro'), @('out\nx_rel\ds2.nro', 'ds2-opt.nro'),
                     @('out\nx_dk\ds2.nro', 'ds2-dk.nro'))) {
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
# 2026-10-18: written fresh every time. The Portuguese names of before the twenty-second build stayed in the staged
# folder and on the card, and a session ran with a stale or missing ds2.toml (ZCULL off: 16 % slower, no stacks).
Get-ChildItem -Path $configs -Filter *.toml -File | Remove-Item -Force
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
# 2026-10-22: the profile config used to append keys the base already has (ds2_gpu_marks_categories,
# switch_profile_report). A TOML file with a key twice is invalid, so the game ignored the whole file and ran on its
# built-in defaults (ZCULL off, no stack sampling): every profiled session from the twenty-third build on was lost.
# Set-Key replaces a key that is there and appends one that is not; the check at the end parses every file.
function Set-Key([string]$text, [string]$key, [string]$value) {
    $pattern = "(?m)^\s*$([regex]::Escape($key))\s*=.*$"
    if ($text -match $pattern) { return [regex]::Replace($text, $pattern, "$key = $value") }
    return $text.TrimEnd() + "`n$key = $value`n"
}
$profileText = Set-Key (Set-Key (Set-Key $base 'ds2_gpu_marks_categories' 'true') 'switch_profile_report' 'true') `
    'switch_profile_stacks' 'true'
[IO.File]::WriteAllText((Join-Path $configs 'ds2_profile.toml'), ($profileText +
    "`n# Measuring: GPU time per pass category and sampling of the CPU stacks (rex_profile_stacks.log). It runs`n" +
    "# a little slower; it is for finding where the time goes.`n"), $noBom)
[IO.File]::WriteAllText((Join-Path $configs 'ds2_test_720p.toml'), (
    ($base -replace 'ds2_render_height_handheld = 540', 'ds2_render_height_handheld = 720') +
    "`n# A/B test: handheld mode at the game's own resolution (1280x720), as in the earlier builds.`n"), $noBom)
# 2026-10-24: the effects missing on deko3d (bonfire flames, fog walls, item glows, waterfalls) but drawn on the PC:
# each half of the deko3d renderer's default-on shortcuts turned off, to find the half they come from.
$shaderKeys = 'banjo_deko3d_vs_outputs', 'banjo_deko3d_early_z', 'banjo_deko3d_early_hoist', 'banjo_deko3d_txq_constants',
    'banjo_deko3d_uam_gvn', 'banjo_deko3d_uam_prmt', 'banjo_deko3d_uam_uniform_branches', 'banjo_deko3d_uam_wide_rows',
    'banjo_deko3d_tess_spec', 'banjo_gpu_output_trim', 'banjo_gpu_depth_only', 'banjo_gpu_bool_spec', 'banjo_renut_linked_stages'
$memoryKeys = 'banjo_deko3d_depth24', 'banjo_deko3d_depth_compression', 'banjo_deko3d_depth_handover',
    'banjo_deko3d_draw_resolves', 'banjo_deko3d_push_constants', 'banjo_deko3d_zcull', 'banjo_deko3d_barrier_lean',
    'banjo_deko3d_barrier_merge', 'banjo_deko3d_eager_kicks', 'banjo_deko3d_kick_submit', 'banjo_deko3d_ring_cleaner',
    'banjo_gpu_depth_words_view', 'banjo_renut_depth_words_lazy', 'banjo_renut_resolve_skip', 'banjo_renut_upload_cached',
    'banjo_renut_store_cards', 'banjo_renut_exact_streams', 'banjo_renut_cpu_memos', 'banjo_renut_shared_dedupe',
    'banjo_renut_device_swizzle', 'banjo_renut_push_point_only'
foreach ($ab in @(@('ds2_ab_shaders_off.toml', $shaderKeys, 'the shader specializations'),
                  @('ds2_ab_memory_off.toml', $memoryKeys, 'the depth, resolve, upload and stream cache shortcuts'))) {
    $text = $base
    foreach ($k in $ab[1]) { $text = Set-Key $text $k 'false' }
    [IO.File]::WriteAllText((Join-Path $configs $ab[0]), ($text +
        "`n# A/B test for the missing effects: $($ab[2]) of the deko3d renderer off.`n"), $noBom)
}
# Every configuration must parse (a key twice makes the game ignore the whole file).
& python -c "import sys, tomllib, pathlib; [tomllib.loads(p.read_text('utf-8')) for p in [pathlib.Path(sys.argv[1]), *pathlib.Path(sys.argv[2]).glob('*.toml')]]; print('configs: all parse')" (Join-Path $dest 'ds2.toml') $configs
if ($LASTEXITCODE -ne 0) { throw 'a configuration does not parse as TOML' }
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
