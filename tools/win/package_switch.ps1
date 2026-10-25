# Lays out the Switch SD card folder in out\nx\sdcard\switch\ds2-nx: the NRO, its settings (app\ds2_switch.toml as
# ds2.toml), the testing notes (README.md), and the alternative configurations. The game files are yours and stay
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
# ds2-nx (2026-10-24): the deko3d NRO only (out\nx_dk: Renut on deko3d, no Vulkan driver; docs/deko3d.md), the one
# the console runs since the twenty-fifth build. The Vulkan NROs (out\nx, nx_pgo, nx_rel) and their shader package
# (ds2_shaders.mesp + .idx, which the deko3d build never reads) are no longer staged; their stale copies are removed.
$nro = Join-Path $root 'out\nx_dk\ds2.nro'
if (-not (Test-Path $nro)) { throw "no NRO at ${nro}: build out\nx_dk first (docs/deko3d.md)" }
Copy-Item -Force $nro (Join-Path $dest 'ds2-dk.nro')
# 2026-10-25: the optimised deko3d build (out/nx_dk_rel: LTO and PGO "use", tools/win/build_switch.ps1 -NoNvk -Lto
# -Pgo use -BuildDir out/nx_dk_rel) is the one shipped as ds2-dk.nro when it exists (+11-12 % where the CPU limits,
# thirtieth build); the separate ds2-dk-opt.nro of that test is removed.
$opt = Join-Path $root 'out/nx_dk_rel/ds2.nro'
if (Test-Path $opt) { Copy-Item -Force $opt (Join-Path $dest 'ds2-dk.nro') }
Remove-Item -Force (Join-Path $dest 'ds2-dk-opt.nro') -ErrorAction SilentlyContinue
foreach ($f in 'ds2.nro', 'ds2-pgo.nro', 'ds2-opt.nro', 'ds2_shaders.mesp', 'ds2_shaders.mesp.idx', 'LEIA-ME.md') {
    Remove-Item -Force (Join-Path $dest $f) -ErrorAction SilentlyContinue
}
Copy-Item -Force (Join-Path $root 'app\ds2_switch.toml') (Join-Path $dest 'ds2.toml')
Copy-Item -Force (Join-Path $root 'docs\switch-testing.md') (Join-Path $dest 'README.md')
# Alternative configurations: copy one over ds2.toml to use it.
$configs = Join-Path $dest 'configs'
New-Item -ItemType Directory -Force $configs | Out-Null
# 2026-10-18: written fresh every time. The Portuguese names of before the twenty-second build stayed in the staged
# folder and on the card, and a session ran with a stale or missing ds2.toml (ZCULL off: 16 % slower, no stacks).
Get-ChildItem -Path $configs -Filter *.toml -File | Remove-Item -Force
$base = Get-Content (Join-Path $root 'app\ds2_switch.toml') -Raw -Encoding UTF8
# Written without a byte order mark (Windows PowerShell's "utf8" adds one, which a TOML reader may not accept).
$noBom = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText((Join-Path $configs 'ds2_default.toml'), $base, $noBom)
# 2026-10-22: the profile config used to append keys the base already has (ds2_gpu_marks_categories,
# switch_profile_report). A TOML file with a key twice is invalid, so the game ignored the whole file and ran on its
# built-in defaults (ZCULL off, no stack sampling): every profiled session from the twenty-third build on was lost.
# Set-Key replaces a key that is there and appends one that is not; the check at the end parses every file.
function Set-Key([string]$text, [string]$key, [string]$value) {
    $pattern = "(?m)^\s*$([regex]::Escape($key))\s*=.*$"
    if ($text -match $pattern) { return [regex]::Replace($text, $pattern, "$key = $value") }
    return $text.TrimEnd() + "`n$key = $value`n"
}
# 2026-10-24: the deko3d renderer's GPU profile (renut_gpu_profile: the GPU's time per kind of work every 10 s;
# banjo_renut_diag_gpu_shaders: the main pass split by pixel shader and blending), and the CPU stack sampling. The
# A/B configurations of the twenty-seventh build and the native renderer's ones are gone.
$profileText = Set-Key (Set-Key (Set-Key (Set-Key $base 'renut_gpu_profile' '10.0') 'banjo_renut_diag_gpu_shaders' 'true') `
    'switch_profile_report' 'true') 'switch_profile_stacks' 'true'
[IO.File]::WriteAllText((Join-Path $configs 'ds2_profile.toml'), ($profileText +
    "`n# Measuring: the GPU's time per pass and per pixel shader every 10 s, and the CPU stacks (rex_profile_stacks.log).`n" +
    "# The timestamps cost a little GPU and render thread time; it is for finding where the time goes.`n"), $noBom)
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
