#!/usr/bin/env bash
# The game shaders for the Switch build's deko3d back end (docs/deko3d.md, step 3), sa2-nx 1.0.3's chain
# (its docs/renut-renderer.md, "Shaders for the Switch") with this port's paths:
#
#   containers (tools/ds2_dump_shaders.py: out/renut_shader_dump)
#     -> Renut's XenosRecomp (the PC build's, out/pc) -> SPIR-V          out/deko3d_shaders/spirv
#     -> SPIRV-Cross -> GLSL with fixed bindings (tools/deko3d_shaders.py) out/deko3d_shaders/glsl
#     -> banjo-nx's passes, each output compiled by the PC uam (--check)  out/deko3d_shaders/glsl_opt
#     -> the package the Switch build links                              app/out/renut_switch
#     -> Renut's shader cache without the DXIL arrays (the PC build's)    app/out/renut_switch
#
# Game data: everything it writes is made from the game's shaders and stays out of git (out/, app/out/).
# Needs the PC build (out/pc, with DS2_RENUT_RENDERER) and the PC tools (tools/deko3d_build_tools.sh).
#   bash tools/ds2_deko3d_shaders.sh
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
PY="${PYTHON:-/c/Users/Usuario/AppData/Local/Programs/Python/Python312/python.exe}"
OUT=out/deko3d_shaders
XENOS=out/pc/xenos_recomp/build/XenosRecomp/XenosRecomp.exe
[ -x "$XENOS" ] || { echo "no $XENOS: build out/pc with DS2_RENUT_RENDERER first"; exit 1; }
[ -f out/renut_shader_dump/manifest.csv ] || "$PY" tools/ds2_dump_shaders.py
mkdir -p "$OUT/spirv"
BANJO_DEKO3D_SPIRV_DIR="$OUT/spirv" BANJO_HLSL_OVERRIDE=out/pc/renut_gpu/early_alpha_hlsl \
    "$XENOS" out/renut_shader_dump "$OUT/spirv_pass.cpp" app/thirdparty/reblue-XenosRecomp/XenosRecomp/shader_common.h
"$PY" tools/deko3d_shaders.py --spirv "$OUT/spirv" --out "$OUT/glsl" --no-uam
"$PY" tools/deko3d_glsl_opt.py --in "$OUT/glsl/glsl" --out "$OUT/glsl_opt" --check
"$PY" tools/deko3d_shader_package.py --glsl "$OUT/glsl_opt" --out app/out/renut_switch
"$PY" tools/renut_switch_shader_cache.py --pc out/pc
echo "deko3d shaders: $(ls "$OUT/glsl_opt" | wc -l) files -> app/out/renut_switch"
