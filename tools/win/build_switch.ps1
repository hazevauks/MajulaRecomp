# Builds the Nintendo Switch NRO: out\nx\ds2.nro (devkitA64 GCC + libnx, static NVK from the Mesa Horizon SDK).
# The generated code must already exist (tools\codegen.ps1 with the PC SDK): a cross build cannot run rexglue.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\win\build_switch.ps1 [-Jobs 8] [-LtoJobs 8] [-Reconfigure]
#       [-Pgo none|generate|use]
# NVK_SDK (environment) overrides the Mesa SDK folder (it must contain lib\libvulkan.a).
# -Pgo (profile-guided optimisation, 2026-10-08): generate = the instrumented NRO that collects counts on the console
# (app/src/ds2_pgo.cpp); use = the optimised NRO from the counts tools\win\pgo_merge.ps1 merged; none = neither.
# Without -Pgo the build keeps the setting it had.
param(
    [int]$Jobs = 8,
    [int]$LtoJobs = 8,
    [switch]$Reconfigure,
    [ValidateSet('', 'none', 'generate', 'use')][string]$Pgo = ''
)
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $PSScriptRoot 'env.ps1')  # VS CMake and Ninja on PATH (the compilers come from the toolchain file)
$nvk = if ($env:NVK_SDK) { $env:NVK_SDK } else {
    'C:/devkitPro/mesa-switch-main-nfsmw-source/mesa-switch-main/mesa-unified-install/opt/devkitpro/portlibs/switch' }
# devkitPro's installer exports DEVKITPRO as an MSYS path (/opt/devkitpro) that Windows CMake cannot open.
# DS2_DEVKITPRO (environment) names an install that is not at C:\devkitPro.
$devkitpro = @($env:DS2_DEVKITPRO, $env:DEVKITPRO, 'C:/devkitPro', (Join-Path $HOME 'Downloads\devkitPro')) |
    Where-Object { $_ -and (Test-Path (Join-Path $_ 'devkitA64')) } | Select-Object -First 1
if (-not $devkitpro) { throw 'devkitPro not found: set DS2_DEVKITPRO to its folder (the one holding devkitA64)' }
$devkitpro = $devkitpro -replace '\\', '/'
# The toolchain file is read again inside CMake's compiler checks (try_compile), where -DDEVKITPRO is not passed on and
# it falls back to the environment: the MSYS path there made the configure fail ("libnx linker specs not found:
# /opt/devkitpro/libnx/switch.specs"). The whole build gets the Windows path.
$env:DEVKITPRO = $devkitpro
$build = Join-Path $root 'out\nx'
$glslang = Join-Path $root 'out\glslang\StandAlone\glslangValidator.exe'  # host tool: SPIR-V is portable
if ($Reconfigure -or -not (Test-Path (Join-Path $build 'CMakeCache.txt'))) {
    & cmake -S (Join-Path $root 'app') -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release `
        "-DCMAKE_TOOLCHAIN_FILE=$root\tools\switch\cmake\switch-devkitA64.cmake" `
        "-DREXGLUE_SWITCH_NVK_SDK=$nvk" "-DDS2_GLSLANG_VALIDATOR=$glslang" "-DDEVKITPRO=$devkitpro" `
        "-DCMAKE_MAKE_PROGRAM=$((Get-Command ninja).Source)"
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
}
# GCC's lto-wrapper compiles the LTRANS partitions in parallel only through GNU make: without a make on PATH it runs
# all 512 of them one after another (about 17 minutes). env.ps1 keeps MSYS2's usr\bin off PATH (its own cmd, link
# and find), so make.exe and the three DLLs it needs get a folder of their own.
$make = Join-Path $root 'out\tools\make'
if (-not (Test-Path (Join-Path $make 'make.exe'))) {
    New-Item -ItemType Directory -Force $make | Out-Null
    foreach ($f in 'make.exe', 'msys-2.0.dll', 'msys-iconv-2.dll', 'msys-intl-8.dll') {
        Copy-Item -Force (Join-Path $devkitpro "msys2\usr\bin\$f") $make
    }
}
$env:PATH = "$make;$env:PATH"
$pgoArgs = @()
if ($Pgo) { $pgoArgs = @("-DDS2_PGO=$(if ($Pgo -eq 'none') { '' } else { $Pgo })") }
& cmake $build "-DDS2_SWITCH_LTO_JOBS=$LtoJobs" @pgoArgs | Out-Null
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& cmake --build $build -j $Jobs
exit $LASTEXITCODE
