// The addresses both renderers hook (banjo-nx's tools/renut_gpu_import.py generated this list for N&B). sa2-nx
// (2026-10-20): SA2's. sa2-nx's own hook there (src/me, src/cpu) calls renut_native_sub_X when Renut's renderer is
// chosen (sa2_renut_yield.h).
#pragma once

#include <rex/ppc.h>

extern "C" void renut_native_sub_8254D9A8(PPCContext& ctx, uint8_t* base);  // AllocateInlineData
extern "C" void renut_native_sub_8254F400(PPCContext& ctx, uint8_t* base);  // CBlocker::Check
extern "C" void renut_native_sub_82565758(PPCContext& ctx, uint8_t* base);  // D3DDevice_BeginVertices
extern "C" void renut_native_sub_82565C18(PPCContext& ctx, uint8_t* base);  // D3DDevice_DrawVerticesUP
extern "C" void renut_native_sub_82565C60(PPCContext& ctx, uint8_t* base);  // D3DDevice_DrawVertices
extern "C" void renut_native_sub_82566078(PPCContext& ctx, uint8_t* base);  // D3DDevice_DrawIndexedVertices
