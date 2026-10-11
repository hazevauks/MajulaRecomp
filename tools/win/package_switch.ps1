# Lays out the Switch SD card folder in out\nx\sdcard\switch\ds2-nx: the NRO, its settings (app\ds2_switch.toml as
# ds2.toml), the testing notes, the shader package and two fallback configurations. The game files are yours and stay
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
Copy-Item -Force (Join-Path $root 'docs\switch-testing.md') (Join-Path $dest 'LEIA-ME.md')
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
[IO.File]::WriteAllText((Join-Path $configs 'ds2_padrao.toml'), $base, $noBom)
[IO.File]::WriteAllText((Join-Path $configs 'ds2_melhorias_desligadas.toml'), ($base +
    "`n# Todos os atalhos de CPU e GPU herdados do sa2-nx desligados: os caminhos originais em tudo.`n" +
    "ds2_cpu_enhancements = false`nds2_noncpu_enhancements = false`n"), $noBom)
[IO.File]::WriteAllText((Join-Path $configs 'ds2_gpu_emulada.toml'), (
    ($base -replace 'ds2_renderer_native = true', 'ds2_renderer_native = false') +
    "`n# A GPU emulada do SDK no lugar do renderizador nativo (muito lenta no console; serve para comparar a imagem).`n" +
    "ds2_gpu_backend = `"vulkan`"`n"), $noBom)
[IO.File]::WriteAllText((Join-Path $configs 'ds2_perfil.toml'), ($base +
    "`n# Medicao: tempo de GPU por categoria de passe e amostragem das pilhas de CPU (rex_profile.log). Roda um pouco`n" +
    "# mais devagar; serve para saber onde o tempo vai.`n" +
    "ds2_gpu_marks_categories = true`nswitch_profile_stacks = true`nswitch_profile_report = true`n"), $noBom)
[IO.File]::WriteAllText((Join-Path $configs 'ds2_teste_zcull.toml'), ($base +
    "`n# Teste A/B: ZCULL tambem nos alvos de profundidade de 1 amostra (o principal do jogo). As importacoes de`n" +
    "# stencil passam a usar passes mascarados em vez do motor de copia.`n" +
    "ds2_native_zcull_1x_depth = true`n"), $noBom)
[IO.File]::WriteAllText((Join-Path $configs 'ds2_teste_sem_atalho_clear.toml'), ($base +
    "`n# Teste A/B: sem o atalho novo das limpezas por superficie 4x (rodada 2): como no build anterior.`n" +
    "ds2_native_edram4_clear_color0_alias = false`n"), $noBom)
[IO.File]::WriteAllText((Join-Path $configs 'ds2_teste_conversao_compute.toml'), (
    ($base -replace 'ds2_native_conversion_frag = true', 'ds2_native_conversion_frag = false') +
    "`n# Teste A/B: conversoes de formato do alvo HDR por compute shader, como ate o decimo quinto build.`n"), $noBom)
[IO.File]::WriteAllText((Join-Path $configs 'ds2_teste_720p.toml'), (
    ($base -replace 'ds2_render_height_handheld = 540', 'ds2_render_height_handheld = 720') +
    "`n# Teste A/B: modo portatil na resolucao do proprio jogo (1280x720), como nos builds anteriores.`n"), $noBom)
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
