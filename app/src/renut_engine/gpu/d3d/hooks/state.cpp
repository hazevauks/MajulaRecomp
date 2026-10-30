/**
 * @file    gpu/d3d/hooks/state.cpp
 * @brief   N&B's D3D state calls, hooked with the native ports (d3d_native.h).
 *
 * The render and sampler state setters are the ones N&B's state tables name
 * (D3D::InitializeApiState puts them into every device's dispatch table,
 * which the game's inline SetRenderState / SetSamplerState call through).
 * The rest are D3DDevice_Set* and the CDevice internals that write the ring.
 */
#include <cstring>

#include <rex/hook.h>

#include "renut_engine/gpu/banjo_renut_select.h"
#include <rex/types.h>

#include "renut_engine/gpu/d3d/d3d_native.h"
#include "renut_engine/gpu/guest/guest_memory.h"

using namespace renut::gpu;

namespace {

template <u32 State>
void SetRenderStateHook(u32 device, u32 value) {
  native::SetRenderState(native::DeviceAt(device), State, value);
}

template <u32 State>
void SetSamplerStateHook(u32 device, u32 sampler, u32 value) {
  native::SetSamplerState(native::DeviceAt(device), sampler, State, value);
}

template <u32 State>
u32 GetRenderStateHook(u32 device) {
  return native::GetRenderState(native::DeviceAt(device), State);
}

template <u32 State>
u32 GetSamplerStateHook(u32 device, u32 sampler) {
  return native::GetSamplerState(native::DeviceAt(device), sampler, State);
}

// The device's AddRef of a resource it hands out (D3DResource_AddRef,
// resource.cpp).
u32 AddRefResource(u32 resource) {
  return native::AddRefResource(resource);
}

// D3DSurface *GetRenderTarget(D3DDevice*, DWORD Index) [0x8267CDA0]
u32 D3DDevice_GetRenderTarget_hook(u32 device, u32 index) {
  const native::Device d = native::DeviceAt(device);
  const u32 surface = d && index < 4 ? d.W(native::off::RenderTargets + index * 4) : 0;
  if (surface)
    AddRefResource(surface);
  return surface;
}

// D3DSurface *GetDepthStencilSurface(D3DDevice*) [0x8267CDE0]
u32 D3DDevice_GetDepthStencilSurface_hook(u32 device) {
  const native::Device d = native::DeviceAt(device);
  const u32 surface = d ? d.W(native::off::DepthStencil) : 0;
  if (surface)
    AddRefResource(surface);
  return surface;
}

// D3DVertexBuffer *GetStreamSource(D3DDevice*, UINT StreamNumber,
//     UINT *pOffsetInBytes, UINT *pStride) [0x8267CD10]: the offset is the
// stream's fetch constant address less the buffer's.
u32 D3DDevice_GetStreamSource_hook(u32 device, u32 stream, mapped_u32 offset, mapped_u32 stride) {
  const native::Device d = native::DeviceAt(device);
  if (!d || stream >= 16)
    return 0;
  const u32 buffer = d.W(native::off::StreamSources + stream * 4);
  u32 bytes = 0;
  if (buffer) {
    const u32 fetch = d.W(native::off::TextureFetch + (95 - stream) * 8);
    bytes = fetch - native::GpuAddress(GuestPtr<const be_u32>(buffer)[6]);
    AddRefResource(buffer);
  }
  if (offset)
    *offset = bytes;
  if (stride)
    *stride = u32(d.B(native::off::StreamStrides + stream)) << 2;
  return buffer;
}

// void D3DDevice_GetViewport(D3DDevice*, D3DVIEWPORT9*) [0x82279A40]
void D3DDevice_GetViewport_hook(u32 device, mapped_u32 viewport) {
  const native::Device d = native::DeviceAt(device);
  if (!d || !viewport)
    return;
  for (u32 i = 0; i < 4; ++i)  // fctidz, the low word
    viewport[i] = u32(native::TruncateToI64(d.F(0x3168 + i * 4)));
  viewport[4] = d.W(0x3178);
  viewport[5] = d.W(0x317C);
}

// void D3DDevice_SetClipPlane(D3DDevice*, DWORD Index, const float *pPlane) [0x8267CFB8]
void D3DDevice_SetClipPlane_hook(u32 device, u32 index, mapped_u32 plane) {
  native::Device d = native::DeviceAt(device);
  if (!d || !plane || index >= 6)
    return;
  for (u32 i = 0; i < 4; ++i)
    d.SetW(0x2820 + index * 16 + i * 4, plane[i]);
  d.Dirty(4, u64(1) << (54 - index));
}

// void D3DDevice_SetVertexFetchConstant(D3DDevice*, UINT VertexFetchRegister,
//     D3DVertexBuffer*, UINT Offset) [0x8267D008]: a vertex fetch slot of the
// 3 in a fetch constant, made a vertex fetch constant first.
void D3DDevice_SetVertexFetchConstant_hook(u32 device, u32 reg, u32 vertex_buffer, u32 offset) {
  native::Device d = native::DeviceAt(device);
  const auto *vb = vertex_buffer ? GuestPtr<const be_u32>(vertex_buffer) : nullptr;
  if (!d || !vb || reg > 0x5F)
    return;
  const u32 slot = (0x5F - reg) / 3;
  const u32 part = (0x5F - reg) - slot * 3;
  const u32 constant = native::off::TextureFetch + slot * 24;
  const u32 type = d.W(constant) & 3;
  if (type == 0 || type == 2)
    std::memset(d.bytes + constant, 0, 24);
  d.SetW(constant, native::Insert(d.W(constant), 1, 0, 2));
  d.Dirty(3, u64(1) << (31 - slot));
  d.SetW(constant + part * 8, native::GpuAddress(u32(vb[6]) + offset));
  d.SetW(constant + part * 8 + 4, native::Insert(u32(vb[7]) - offset, 1, 28, 2));
}

// The boolean and integer shader constants (D3DDevice_Set{Vertex,Pixel}
// ShaderConstant{B,I}): bits from 0x2780 (vertex) / 0x2790 (pixel), a word
// of x, y, z bytes each from 0x27A0 / 0x27E0.
void SetShaderConstantsB(u32 device, u32 base, u32 start, mapped_u32 data, u32 count) {
  native::Device d = native::DeviceAt(device);
  if (!d || !data)
    return;
  for (u32 i = 0; i < count; ++i, ++start) {
    const u32 word = base + (start >> 5) * 4;
    const u32 bit = 1u << (start & 31);
    d.SetW(word, (d.W(word) & ~bit) | ((u32(data[i]) & 1) << (start & 31)));
  }
  d.Dirty(4, u64(1) << 56);
}

void SetShaderConstantsI(u32 device, u32 base, u32 start, mapped_u32 data, u32 count) {
  native::Device d = native::DeviceAt(device);
  if (!d || !data)
    return;
  for (u32 i = 0; i < count; ++i) {
    const u32 x = u32(data[i * 4]) & 0xFF, y = u32(data[i * 4 + 1]) & 0xFF;
    const u32 z = u32(data[i * 4 + 2]) & 0xFF;
    d.SetW(base + (start + i) * 4, (z << 16) | (y << 8) | x);
  }
  d.Dirty(4, u64(1) << 56);
}

// void D3DDevice_SetVertexShaderConstantB(D3DDevice*, UINT StartRegister,
//     const BOOL*, UINT BoolCount) [0x82221930], and the others alike.
void D3DDevice_SetVertexShaderConstantB_hook(u32 device, u32 start, mapped_u32 data, u32 count) {
  SetShaderConstantsB(device, 0x2780, start, data, count);
}
void D3DDevice_SetPixelShaderConstantB_hook(u32 device, u32 start, mapped_u32 data, u32 count) {
  SetShaderConstantsB(device, 0x2790, start, data, count);
}
void D3DDevice_SetVertexShaderConstantI_hook(u32 device, u32 start, mapped_u32 data, u32 count) {
  SetShaderConstantsI(device, 0x27A0, start, data, count);
}
void D3DDevice_SetPixelShaderConstantI_hook(u32 device, u32 start, mapped_u32 data, u32 count) {
  SetShaderConstantsI(device, 0x27E0, start, data, count);
}

// void D3DDevice_EndIndexedVertices(D3DDevice*) [0x826857A0]: the inline end
// of BeginIndexedVertices, back to the ring pointer it saved.
void D3DDevice_EndIndexedVertices_hook(u32 device) {
  native::Device d = native::DeviceAt(device);
  if (d)
    d.SetW(native::off::Ring, d.W(0x3484));
}

// void D3DDevice_SetTexture(D3DDevice*, DWORD Sampler, D3DBaseTexture*,
//     UINT64 PendingMask3) [0x8222F410]; the mask is the sampler's fetch
// constant bit, which the port derives from Sampler.
void D3DDevice_SetTexture_hook(u32 device, u32 sampler, u32 texture) {
  native::SetTexture(native::DeviceAt(device), sampler, texture);
}

// void D3DDevice_SetStreamSource(D3DDevice*, UINT StreamNumber, D3DVertexBuffer*,
//     UINT OffsetInBytes, UINT Stride, UINT64 PendingMask3) [0x8222A408]
void D3DDevice_SetStreamSource_hook(u32 device, u32 stream, u32 vertex_buffer, u32 offset,
                                    u32 stride) {
  native::SetStreamSource(native::DeviceAt(device), stream, vertex_buffer, offset, stride);
}

// void D3DDevice_SetIndices(D3DDevice*, D3DIndexBuffer*) [0x8220EE38]
void D3DDevice_SetIndices_hook(u32 device, u32 index_buffer) {
  native::SetIndices(native::DeviceAt(device), index_buffer);
}

// void D3DDevice_SetVertexDeclaration(D3DDevice*, D3DVertexDeclaration*) [0x82205700]
void D3DDevice_SetVertexDeclaration_hook(u32 device, u32 declaration) {
  native::SetVertexDeclaration(native::DeviceAt(device), declaration);
}

// void D3DDevice_SetViewport(D3DDevice*, const D3DVIEWPORT9*) [0x82222EA0]
void D3DDevice_SetViewport_hook(u32 device, u32 viewport) {
  native::SetViewport(native::DeviceAt(device), viewport);
}

// void D3D::SetViewport(D3DDevice*, float X, float Y, float Width, float Height,
//     float MinZ, float MaxZ) [0x82222F20]
void D3D_SetViewport_hook(u32 device, float x, float y, float width, float height, float min_z,
                          float max_z) {
  native::SetViewport(native::DeviceAt(device), x, y, width, height, min_z, max_z);
}

// void D3DDevice_SetScissorRect(D3DDevice*, const RECT*) [0x82223178]
void D3DDevice_SetScissorRect_hook(u32 device, mapped_u32 rect) {
  if (!rect)
    return;
  native::SetScissorRect(native::DeviceAt(device), i32(u32(rect[0])), i32(u32(rect[1])),
                         i32(u32(rect[2])), i32(u32(rect[3])));
}

// void D3D::SetSurfaceClip(CDevice*, int, int, int, int) [0x82223270]: the
// window scissor and per-tile clip packets, nothing else.
void D3D_SetSurfaceClip_hook(u32 /*device*/, u32, u32, u32, u32) {}

// void D3DDevice_SetRenderTarget(D3DDevice*, DWORD Index, D3DSurface*) [0x8227D9D0]
void D3DDevice_SetRenderTarget_hook(u32 device, u32 index, u32 surface) {
  native::SetRenderTarget(native::DeviceAt(device), index, surface);
}

// void D3DDevice_SetDepthStencilSurface(D3DDevice*, D3DSurface*) [0x8227DD38]
void D3DDevice_SetDepthStencilSurface_hook(u32 device, u32 surface) {
  native::SetDepthStencilSurface(native::DeviceAt(device), surface);
}

// void D3D::SetSurfaceInfo(CDevice*, D3DSurface*) [0x8227DD10]
void D3D_SetSurfaceInfo_hook(u32 device, u32 surface) {
  native::SetSurfaceInfo(native::DeviceAt(device), surface);
}

// void D3DDevice_SetPredication(D3DDevice*, DWORD PredicationMask) [0x8222AA18]
void D3DDevice_SetPredication_hook(u32 device, u32 mask) {
  native::SetPredication(native::DeviceAt(device), mask);
}

// void D3DDevice_SetCommandBufferPredication(D3DDevice*, DWORD TilePredication,
//     DWORD RunPredication) [0x82687C78]
void D3DDevice_SetCommandBufferPredication_hook(u32 device, u32 tile_predication,
                                                u32 run_predication) {
  native::SetCommandBufferPredication(native::DeviceAt(device), tile_predication,
                                      run_predication);
}

// void D3DDevice_SetShaderGPRAllocation(D3DDevice*, DWORD Flags,
//     DWORD VertexShaderCount, DWORD PixelShaderCount) [0x82220B20]
void D3DDevice_SetShaderGPRAllocation_hook(u32 device, u32 flags, u32 vertex_count,
                                           u32 pixel_count) {
  native::SetShaderGPRAllocation(native::DeviceAt(device), flags, vertex_count, pixel_count);
}

// void D3D::CleanupGprAllocationSetup(CDevice*) [0x8267D790]
void D3D_CleanupGprAllocationSetup_hook(u32 device) {
  native::CleanupGprAllocationSetup(native::DeviceAt(device));
}

// void D3DDevice_SetScreenExtentQueryMode(D3DDevice*, D3DSCREENEXTENTQUERYMODE)
// [0x82688000]
void D3DDevice_SetScreenExtentQueryMode_hook(u32 device, u32 mode) {
  native::SetScreenExtentQueryMode(native::DeviceAt(device), mode);
}

// void D3DDevice_UnsetAll(D3DDevice*) [0x8267D678]
void D3DDevice_UnsetAll_hook(u32 device) {
  native::UnsetAll(native::DeviceAt(device));
}

// void D3D::ResetAllState(CDevice*, BOOL) [0x8228AFE8]
void D3D_ResetAllState_hook(u32 device, u32 /*skip_packets*/) {
  native::ResetAllState(native::DeviceAt(device));
}

// void D3D::InvalidateConstants(D3DDevice*) [0x8267D5F8]
void D3D_InvalidateConstants_hook(u32 device) {
  native::InvalidateConstants(native::DeviceAt(device));
}

// void D3DDevice_BlockUntilIdle(D3DDevice*) [0x82686A38] (IDA: the second
// D3D::InvalidateConstants): waits for the device's current fence (N&B calls
// it before releasing a device).
void D3DDevice_BlockUntilIdle_hook(u32 device) {
  native::InvalidateConstantsAndWait(native::DeviceAt(device));
}

} // namespace

// ds2-nx (2026-10-29): every hook below is at an address of N&B's census, not of Dark Souls II's Direct3D (which is at
// 0x829B..0x829F, device.cpp and draw.cpp). Most name no function of the game's code and never ran; the few that
// do (8 in the 2014 executable, 9 once moved to Scholar of the First Sin) land on ordinary game functions, with
// prologues and calls (one stores a vtable: a constructor), which they replaced with a state read or write. Out of
// the build: the game's state goes through its inline setters into the device, which Renut reads at the draw
// (d3d_native_state.cpp). DS2_NB_STATE_HOOKS=1 brings them back.
#ifndef DS2_NB_STATE_HOOKS
#define DS2_NB_STATE_HOOKS 0
#endif
#if DS2_NB_STATE_HOOKS

// Render states, by their index in N&B's table (off_82E506F0).
BANJO_RENUT_HOOK(sub_8267B418, SetRenderStateHook<10>);  // D3DDevice_SetRenderState_ZEnable
BANJO_RENUT_HOOK(sub_821FB7B0, SetRenderStateHook<11>);  // D3DDevice_SetRenderState_ZFunc
BANJO_RENUT_HOOK(sub_8267B458, SetRenderStateHook<12>);  // D3DDevice_SetRenderState_ZWriteEnable
BANJO_RENUT_HOOK(sub_82292E28, SetRenderStateHook<13>);  // D3DDevice_SetRenderState_FillMode
BANJO_RENUT_HOOK(sub_8228F350, SetRenderStateHook<14>);  // D3DDevice_SetRenderState_CullMode
BANJO_RENUT_HOOK(sub_8222A718, SetRenderStateHook<15>);  // D3DDevice_SetRenderState_AlphaBlendEnable
BANJO_RENUT_HOOK(sub_8222A690, SetRenderStateHook<16>);  // D3DDevice_SetRenderState_SeparateAlphaBlendEnable
BANJO_RENUT_HOOK(sub_822717F0, SetRenderStateHook<17>);  // D3DDevice_SetRenderState_BlendFactor
BANJO_RENUT_HOOK(sub_82230090, SetRenderStateHook<18>);  // D3DDevice_SetRenderState_SrcBlend
BANJO_RENUT_HOOK(sub_822307B8, SetRenderStateHook<19>);  // D3DDevice_SetRenderState_DestBlend
BANJO_RENUT_HOOK(sub_82226380, SetRenderStateHook<20>);  // D3DDevice_SetRenderState_BlendOp
BANJO_RENUT_HOOK(sub_8226A170, SetRenderStateHook<21>);  // D3DDevice_SetRenderState_SrcBlendAlpha
BANJO_RENUT_HOOK(sub_822208E8, SetRenderStateHook<22>);  // D3DDevice_SetRenderState_DestBlendAlpha
BANJO_RENUT_HOOK(sub_8221F5F8, SetRenderStateHook<23>);  // D3DDevice_SetRenderState_BlendOpAlpha
BANJO_RENUT_HOOK(sub_8264BE98, SetRenderStateHook<24>);  // D3DDevice_SetRenderState_AlphaTestEnable
BANJO_RENUT_HOOK(sub_8267B228, SetRenderStateHook<25>);  // D3DDevice_SetRenderState_AlphaRef
BANJO_RENUT_HOOK(sub_8267B290, SetRenderStateHook<26>);  // D3DDevice_SetRenderState_AlphaFunc
BANJO_RENUT_HOOK(sub_8267B498, SetRenderStateHook<27>);  // D3DDevice_SetRenderState_StencilEnable
BANJO_RENUT_HOOK(sub_8267B4D8, SetRenderStateHook<28>);  // D3DDevice_SetRenderState_TwoSidedStencilMode
BANJO_RENUT_HOOK(sub_8267B540, SetRenderStateHook<29>);  // D3DDevice_SetRenderState_StencilFail
BANJO_RENUT_HOOK(sub_8267B578, SetRenderStateHook<30>);  // D3DDevice_SetRenderState_StencilZFail
BANJO_RENUT_HOOK(sub_8267B5B0, SetRenderStateHook<31>);  // D3DDevice_SetRenderState_StencilPass
BANJO_RENUT_HOOK(sub_8267B510, SetRenderStateHook<32>);  // D3DDevice_SetRenderState_StencilFunc
BANJO_RENUT_HOOK(sub_8264C3D8, SetRenderStateHook<33>);  // D3DDevice_SetRenderState_StencilRef
BANJO_RENUT_HOOK(sub_8264C3F8, SetRenderStateHook<34>);  // D3DDevice_SetRenderState_StencilMask
BANJO_RENUT_HOOK(sub_8264C418, SetRenderStateHook<35>);  // D3DDevice_SetRenderState_StencilWriteMask
BANJO_RENUT_HOOK(sub_8267B610, SetRenderStateHook<36>);  // D3DDevice_SetRenderState_CCWStencilFail
BANJO_RENUT_HOOK(sub_8264C370, SetRenderStateHook<37>);  // D3DDevice_SetRenderState_CCWStencilZFail
BANJO_RENUT_HOOK(sub_8264C3A8, SetRenderStateHook<38>);  // D3DDevice_SetRenderState_CCWStencilPass
BANJO_RENUT_HOOK(sub_8267B5E0, SetRenderStateHook<39>);  // D3DDevice_SetRenderState_CCWStencilFunc
BANJO_RENUT_HOOK(sub_8264C438, SetRenderStateHook<40>);  // D3DDevice_SetRenderState_CCWStencilRef
BANJO_RENUT_HOOK(sub_8264C458, SetRenderStateHook<41>);  // D3DDevice_SetRenderState_CCWStencilMask
BANJO_RENUT_HOOK(sub_8264C478, SetRenderStateHook<42>);  // D3DDevice_SetRenderState_CCWStencilWriteMask
BANJO_RENUT_HOOK(sub_8228F370, SetRenderStateHook<43>);  // D3DDevice_SetRenderState_ClipPlaneEnable
BANJO_RENUT_HOOK(sub_8267BA08, SetRenderStateHook<44>);  // D3DDevice_SetRenderState_PointSize
BANJO_RENUT_HOOK(sub_8267BA68, SetRenderStateHook<45>);  // D3DDevice_SetRenderState_PointSizeMin
BANJO_RENUT_HOOK(sub_8267B9F8, SetRenderStateHook<46>);  // D3DDevice_SetRenderState_PointSpriteEnable
BANJO_RENUT_HOOK(sub_8226B6E8, SetRenderStateHook<47>);  // D3DDevice_SetRenderState_PointSizeMax
BANJO_RENUT_HOOK(sub_8267B8E0, SetRenderStateHook<48>);  // D3DDevice_SetRenderState_MultiSampleAntiAlias
BANJO_RENUT_HOOK(sub_8267B910, SetRenderStateHook<49>);  // D3DDevice_SetRenderState_MultiSampleMask
BANJO_RENUT_HOOK(sub_8264C4A8, SetRenderStateHook<50>);  // D3DDevice_SetRenderState_ScissorTestEnable
BANJO_RENUT_HOOK(sub_8267B808, SetRenderStateHook<51>);  // D3DDevice_SetRenderState_SlopeScaleDepthBias
BANJO_RENUT_HOOK(sub_821FB718, SetRenderStateHook<52>);  // D3DDevice_SetRenderState_DepthBias
BANJO_RENUT_HOOK(sub_821FB6E0, SetRenderStateHook<53>);  // D3DDevice_SetRenderState_ColorWriteEnable
BANJO_RENUT_HOOK(sub_8267B938, SetRenderStateHook<54>);  // D3DDevice_SetRenderState_ColorWriteEnable1
BANJO_RENUT_HOOK(sub_8267B978, SetRenderStateHook<55>);  // D3DDevice_SetRenderState_ColorWriteEnable2
BANJO_RENUT_HOOK(sub_8267B9B8, SetRenderStateHook<56>);  // D3DDevice_SetRenderState_ColorWriteEnable3
BANJO_RENUT_HOOK(sub_8267C108, SetRenderStateHook<57>);  // D3DDevice_SetRenderState_TessellationMode
BANJO_RENUT_HOOK(sub_8267C098, SetRenderStateHook<58>);  // D3DDevice_SetRenderState_MinTessellationLevel
BANJO_RENUT_HOOK(sub_8267C0D0, SetRenderStateHook<59>);  // D3DDevice_SetRenderState_MaxTessellationLevel
BANJO_RENUT_HOOK(sub_8267BAC8, SetRenderStateHook<60>);  // D3DDevice_SetRenderState_Wrap0
BANJO_RENUT_HOOK(sub_8267BAE8, SetRenderStateHook<61>);  // D3DDevice_SetRenderState_Wrap1
BANJO_RENUT_HOOK(sub_8267BB10, SetRenderStateHook<62>);  // D3DDevice_SetRenderState_Wrap2
BANJO_RENUT_HOOK(sub_8267BB38, SetRenderStateHook<63>);  // D3DDevice_SetRenderState_Wrap3
BANJO_RENUT_HOOK(sub_8267BB60, SetRenderStateHook<64>);  // D3DDevice_SetRenderState_Wrap4
BANJO_RENUT_HOOK(sub_8267BB88, SetRenderStateHook<65>);  // D3DDevice_SetRenderState_Wrap5
BANJO_RENUT_HOOK(sub_8267BBB0, SetRenderStateHook<66>);  // D3DDevice_SetRenderState_Wrap6
BANJO_RENUT_HOOK(sub_8267BBD8, SetRenderStateHook<67>);  // D3DDevice_SetRenderState_Wrap7
BANJO_RENUT_HOOK(sub_8267BBF8, SetRenderStateHook<68>);  // D3DDevice_SetRenderState_Wrap8
BANJO_RENUT_HOOK(sub_8267BC18, SetRenderStateHook<69>);  // D3DDevice_SetRenderState_Wrap9
BANJO_RENUT_HOOK(sub_8267BC40, SetRenderStateHook<70>);  // D3DDevice_SetRenderState_Wrap10
BANJO_RENUT_HOOK(sub_8267BC68, SetRenderStateHook<71>);  // D3DDevice_SetRenderState_Wrap11
BANJO_RENUT_HOOK(sub_8267BC90, SetRenderStateHook<72>);  // D3DDevice_SetRenderState_Wrap12
BANJO_RENUT_HOOK(sub_8267BCB8, SetRenderStateHook<73>);  // D3DDevice_SetRenderState_Wrap13
BANJO_RENUT_HOOK(sub_8267BCE0, SetRenderStateHook<74>);  // D3DDevice_SetRenderState_Wrap14
BANJO_RENUT_HOOK(sub_8267BD08, SetRenderStateHook<75>);  // D3DDevice_SetRenderState_Wrap15
BANJO_RENUT_HOOK(sub_82205A80, SetRenderStateHook<76>);  // D3DDevice_SetRenderState_ViewportEnable
BANJO_RENUT_HOOK(sub_8267BE38, SetRenderStateHook<77>);  // D3DDevice_SetRenderState_HighPrecisionBlendEnable
BANJO_RENUT_HOOK(sub_8267BED0, SetRenderStateHook<78>);  // D3DDevice_SetRenderState_HighPrecisionBlendEnable1
BANJO_RENUT_HOOK(sub_8267BF68, SetRenderStateHook<79>);  // D3DDevice_SetRenderState_HighPrecisionBlendEnable2
BANJO_RENUT_HOOK(sub_8267C000, SetRenderStateHook<80>);  // D3DDevice_SetRenderState_HighPrecisionBlendEnable3
BANJO_RENUT_HOOK(sub_8267C140, SetRenderStateHook<81>);  // D3DDevice_SetRenderState_HalfPixelOffset
BANJO_RENUT_HOOK(sub_8267C178, SetRenderStateHook<82>);  // D3DDevice_SetRenderState_PrimitiveResetEnable
BANJO_RENUT_HOOK(sub_8267C1A8, SetRenderStateHook<83>);  // D3DDevice_SetRenderState_PrimitiveResetIndex
BANJO_RENUT_HOOK(sub_8267C1D0, SetRenderStateHook<84>);  // D3DDevice_SetRenderState_AlphaToMaskEnable
BANJO_RENUT_HOOK(sub_8267C200, SetRenderStateHook<85>);  // D3DDevice_SetRenderState_AlphaToMaskOffsets
BANJO_RENUT_HOOK(sub_8267C238, SetRenderStateHook<86>);  // D3DDevice_SetRenderState_GuardBand_X
BANJO_RENUT_HOOK(sub_8267C268, SetRenderStateHook<87>);  // D3DDevice_SetRenderState_GuardBand_Y
BANJO_RENUT_HOOK(sub_8267C298, SetRenderStateHook<88>);  // D3DDevice_SetRenderState_DiscardBand_X
BANJO_RENUT_HOOK(sub_8267C2C0, SetRenderStateHook<89>);  // D3DDevice_SetRenderState_DiscardBand_Y
BANJO_RENUT_HOOK(sub_8267C330, SetRenderStateHook<90>);  // D3DDevice_SetRenderState_HiStencilEnable
BANJO_RENUT_HOOK(sub_8267C360, SetRenderStateHook<91>);  // D3DDevice_SetRenderState_HiStencilWriteEnable
BANJO_RENUT_HOOK(sub_8267C390, SetRenderStateHook<92>);  // D3DDevice_SetRenderState_HiStencilFunc
BANJO_RENUT_HOOK(sub_8267C3C0, SetRenderStateHook<93>);  // D3DDevice_SetRenderState_HiStencilRef
BANJO_RENUT_HOOK(sub_8267C3E0, SetRenderStateHook<94>);  // D3DDevice_SetRenderState_PresentInterval
BANJO_RENUT_HOOK(sub_8267C3F0, SetRenderStateHook<95>);  // D3DDevice_SetRenderState_PresentImmediateThreshold
BANJO_RENUT_HOOK(sub_8267C2F0, SetRenderStateHook<96>);  // D3DDevice_SetRenderState_HiZEnable
BANJO_RENUT_HOOK(sub_8228CCB8, SetRenderStateHook<97>);  // D3DDevice_SetRenderState_HiZWriteEnable
BANJO_RENUT_HOOK(sub_8267B338, SetRenderStateHook<98>);  // D3DDevice_SetRenderState_LastPixel
BANJO_RENUT_HOOK(sub_8267B370, SetRenderStateHook<99>);  // D3DDevice_SetRenderState_LineWidth
BANJO_RENUT_HOOK(sub_8267B3E8, SetRenderStateHook<100>);  // D3DDevice_SetRenderState_Buffer2Frames

// Sampler states, by their index in N&B's table (off_82E50BD0).
BANJO_RENUT_HOOK(sub_822800D0, SetSamplerStateHook<0>);  // D3DDevice_SetSamplerState_AddressU
BANJO_RENUT_HOOK(sub_8220C4E0, SetSamplerStateHook<1>);  // D3DDevice_SetSamplerState_AddressV
BANJO_RENUT_HOOK(sub_8267CA88, SetSamplerStateHook<2>);  // D3DDevice_SetSamplerState_AddressW
BANJO_RENUT_HOOK(sub_8267C9E8, SetSamplerStateHook<3>);  // D3DDevice_SetSamplerState_BorderColor
BANJO_RENUT_HOOK(sub_8222F1B8, SetSamplerStateHook<4>);  // D3DDevice_SetSamplerState_MagFilter
BANJO_RENUT_HOOK(sub_8222F280, SetSamplerStateHook<5>);  // D3DDevice_SetSamplerState_MinFilter
BANJO_RENUT_HOOK(sub_82292DE8, SetSamplerStateHook<6>);  // D3DDevice_SetSamplerState_MipFilter
BANJO_RENUT_HOOK(sub_8267C7A8, SetSamplerStateHook<7>);  // D3DDevice_SetSamplerState_MipMapLodBias
BANJO_RENUT_HOOK(sub_8222F590, SetSamplerStateHook<8>);  // D3DDevice_SetSamplerState_MaxMipLevel
BANJO_RENUT_HOOK(sub_8267C688, SetSamplerStateHook<9>);  // D3DDevice_SetSamplerState_MaxAnisotropy
BANJO_RENUT_HOOK(sub_8267C528, SetSamplerStateHook<10>);  // D3DDevice_SetSamplerState_MagFilterZ
BANJO_RENUT_HOOK(sub_8267C448, SetSamplerStateHook<11>);  // D3DDevice_SetSamplerState_MinFilterZ
BANJO_RENUT_HOOK(sub_8267C5E0, SetSamplerStateHook<12>);  // D3DDevice_SetSamplerState_SeparateZFilterEnable
BANJO_RENUT_HOOK(sub_8267C858, SetSamplerStateHook<13>);  // D3DDevice_SetSamplerState_MinMipLevel
BANJO_RENUT_HOOK(sub_8267CAD8, SetSamplerStateHook<14>);  // D3DDevice_SetSamplerState_TrilinearThreshold
BANJO_RENUT_HOOK(sub_8267C700, SetSamplerStateHook<15>);  // D3DDevice_SetSamplerState_AnisotropyBias
BANJO_RENUT_HOOK(sub_8267CB30, SetSamplerStateHook<16>);  // D3DDevice_SetSamplerState_HGradientExpBias
BANJO_RENUT_HOOK(sub_8267CB88, SetSamplerStateHook<17>);  // D3DDevice_SetSamplerState_VGradientExpBias
BANJO_RENUT_HOOK(sub_8267CBE0, SetSamplerStateHook<18>);  // D3DDevice_SetSamplerState_WhiteBorderColorW
BANJO_RENUT_HOOK(sub_8267CCB0, SetSamplerStateHook<19>);  // D3DDevice_SetSamplerState_PointBorderEnable

BANJO_RENUT_HOOK(sub_8222F410, D3DDevice_SetTexture_hook);  // D3DDevice_SetTexture
BANJO_RENUT_HOOK(sub_82BC63C8, D3DDevice_SetTexture_hook);  // D3DDevice_SetTexture_Inline
BANJO_RENUT_HOOK(sub_8222A408, D3DDevice_SetStreamSource_hook);  // D3DDevice_SetStreamSource
BANJO_RENUT_HOOK(sub_8220EE38, D3DDevice_SetIndices_hook);  // D3DDevice_SetIndices
BANJO_RENUT_HOOK(sub_82205700, D3DDevice_SetVertexDeclaration_hook);  // D3DDevice_SetVertexDeclaration
BANJO_RENUT_HOOK(sub_82222EA0, D3DDevice_SetViewport_hook);  // D3DDevice_SetViewport
BANJO_RENUT_HOOK(sub_82222F20, D3D_SetViewport_hook);  // D3D_SetViewport
BANJO_RENUT_HOOK(sub_82223178, D3DDevice_SetScissorRect_hook);  // D3DDevice_SetScissorRect
BANJO_RENUT_HOOK_SHARED(sub_82223270, D3D_SetSurfaceClip_hook);  // D3D_SetSurfaceClip
BANJO_RENUT_HOOK(sub_8227D9D0, D3DDevice_SetRenderTarget_hook);  // D3DDevice_SetRenderTarget
BANJO_RENUT_HOOK(sub_8227DD38, D3DDevice_SetDepthStencilSurface_hook);  // D3DDevice_SetDepthStencilSurface
BANJO_RENUT_HOOK(sub_8227DD10, D3D_SetSurfaceInfo_hook);  // D3D_SetSurfaceInfo
BANJO_RENUT_HOOK_SHARED(sub_8222AA18, D3DDevice_SetPredication_hook);  // D3DDevice_SetPredication
BANJO_RENUT_HOOK_SHARED(sub_82687C78, D3DDevice_SetCommandBufferPredication_hook);  // D3DDevice_SetCommandBufferPredication
BANJO_RENUT_HOOK_SHARED(sub_82220B20, D3DDevice_SetShaderGPRAllocation_hook);  // D3DDevice_SetShaderGPRAllocation
BANJO_RENUT_HOOK(sub_8267D790, D3D_CleanupGprAllocationSetup_hook);  // D3D_CleanupGprAllocationSetup
BANJO_RENUT_HOOK_SHARED(sub_82688000, D3DDevice_SetScreenExtentQueryMode_hook);  // D3DDevice_SetScreenExtentQueryMode
BANJO_RENUT_HOOK(sub_8267D678, D3DDevice_UnsetAll_hook);  // D3DDevice_UnsetAll
BANJO_RENUT_HOOK_SHARED(sub_8228AFE8, D3D_ResetAllState_hook);  // D3D_ResetAllState
BANJO_RENUT_HOOK_SHARED(sub_8267D5F8, D3D_InvalidateConstants_hook);  // D3D_InvalidateConstants
BANJO_RENUT_HOOK_SHARED(sub_82686A38, D3DDevice_BlockUntilIdle_hook);  // D3DDevice_BlockUntilIdle
// Through a thunk (j_D3DDevice_SetRenderTarget 0x8267CD98).
BANJO_RENUT_HOOK(sub_8267CD98, D3DDevice_SetRenderTarget_hook);  // D3DDevice_SetRenderTarget_Thunk
BANJO_RENUT_HOOK(sub_8267CDA0, D3DDevice_GetRenderTarget_hook);  // D3DDevice_GetRenderTarget
BANJO_RENUT_HOOK(sub_8267CDE0, D3DDevice_GetDepthStencilSurface_hook);  // D3DDevice_GetDepthStencilSurface
BANJO_RENUT_HOOK(sub_8267CD10, D3DDevice_GetStreamSource_hook);  // D3DDevice_GetStreamSource
BANJO_RENUT_HOOK(sub_82279A40, D3DDevice_GetViewport_hook);  // D3DDevice_GetViewport
BANJO_RENUT_HOOK(sub_8267CFB8, D3DDevice_SetClipPlane_hook);  // D3DDevice_SetClipPlane
BANJO_RENUT_HOOK(sub_8267D008, D3DDevice_SetVertexFetchConstant_hook);  // D3DDevice_SetVertexFetchConstant
BANJO_RENUT_HOOK(sub_82221930, D3DDevice_SetVertexShaderConstantB_hook);  // D3DDevice_SetVertexShaderConstantB
BANJO_RENUT_HOOK(sub_8226B728, D3DDevice_SetPixelShaderConstantB_hook);  // D3DDevice_SetPixelShaderConstantB
BANJO_RENUT_HOOK(sub_8267DA40, D3DDevice_SetVertexShaderConstantI_hook);  // D3DDevice_SetVertexShaderConstantI
BANJO_RENUT_HOOK(sub_8267DA98, D3DDevice_SetPixelShaderConstantI_hook);  // D3DDevice_SetPixelShaderConstantI
BANJO_RENUT_HOOK(sub_826857A0, D3DDevice_EndIndexedVertices_hook);  // D3DDevice_EndIndexedVertices

// Render state getters, by their index in N&B's table (off_82E506F0, field 0).
BANJO_RENUT_HOOK(sub_8267B450, GetRenderStateHook<10>);  // D3DDevice_GetRenderState_ZEnable
BANJO_RENUT_HOOK(sub_8267B488, GetRenderStateHook<11>);  // D3DDevice_GetRenderState_ZFunc
BANJO_RENUT_HOOK(sub_8267B478, GetRenderStateHook<12>);  // D3DDevice_GetRenderState_ZWriteEnable
BANJO_RENUT_HOOK(sub_8264BE88, GetRenderStateHook<13>);  // D3DDevice_GetRenderState_FillMode
BANJO_RENUT_HOOK(sub_8264BE78, GetRenderStateHook<14>);  // D3DDevice_GetRenderState_CullMode
BANJO_RENUT_HOOK(sub_8264BED0, GetRenderStateHook<15>);  // D3DDevice_GetRenderState_AlphaBlendEnable
BANJO_RENUT_HOOK(sub_8267B218, GetRenderStateHook<16>);  // D3DDevice_GetRenderState_SeparateAlphaBlendEnable
BANJO_RENUT_HOOK(sub_8267B2C0, GetRenderStateHook<17>);  // D3DDevice_GetRenderState_BlendFactor
BANJO_RENUT_HOOK(sub_8267B1C8, GetRenderStateHook<18>);  // D3DDevice_GetRenderState_SrcBlend
BANJO_RENUT_HOOK(sub_8267B1D8, GetRenderStateHook<19>);  // D3DDevice_GetRenderState_DestBlend
BANJO_RENUT_HOOK(sub_8264BEE0, GetRenderStateHook<20>);  // D3DDevice_GetRenderState_BlendOp
BANJO_RENUT_HOOK(sub_8267B1F8, GetRenderStateHook<21>);  // D3DDevice_GetRenderState_SrcBlendAlpha
BANJO_RENUT_HOOK(sub_8267B208, GetRenderStateHook<22>);  // D3DDevice_GetRenderState_DestBlendAlpha
BANJO_RENUT_HOOK(sub_8267B1E8, GetRenderStateHook<23>);  // D3DDevice_GetRenderState_BlendOpAlpha
BANJO_RENUT_HOOK(sub_8264BEC0, GetRenderStateHook<24>);  // D3DDevice_GetRenderState_AlphaTestEnable
BANJO_RENUT_HOOK(sub_8267B260, GetRenderStateHook<25>);  // D3DDevice_GetRenderState_AlphaRef
BANJO_RENUT_HOOK(sub_8267B2B0, GetRenderStateHook<26>);  // D3DDevice_GetRenderState_AlphaFunc
BANJO_RENUT_HOOK(sub_8267B4D0, GetRenderStateHook<27>);  // D3DDevice_GetRenderState_StencilEnable
BANJO_RENUT_HOOK(sub_8267B500, GetRenderStateHook<28>);  // D3DDevice_GetRenderState_TwoSidedStencilMode
BANJO_RENUT_HOOK(sub_8267B568, GetRenderStateHook<29>);  // D3DDevice_GetRenderState_StencilFail
BANJO_RENUT_HOOK(sub_8267B5A0, GetRenderStateHook<30>);  // D3DDevice_GetRenderState_StencilZFail
BANJO_RENUT_HOOK(sub_8267B5D0, GetRenderStateHook<31>);  // D3DDevice_GetRenderState_StencilPass
BANJO_RENUT_HOOK(sub_8267B530, GetRenderStateHook<32>);  // D3DDevice_GetRenderState_StencilFunc
BANJO_RENUT_HOOK(sub_8264C3F0, GetRenderStateHook<33>);  // D3DDevice_GetRenderState_StencilRef
BANJO_RENUT_HOOK(sub_8264C410, GetRenderStateHook<34>);  // D3DDevice_GetRenderState_StencilMask
BANJO_RENUT_HOOK(sub_8264C430, GetRenderStateHook<35>);  // D3DDevice_GetRenderState_StencilWriteMask
BANJO_RENUT_HOOK(sub_8264C360, GetRenderStateHook<36>);  // D3DDevice_GetRenderState_CCWStencilFail
BANJO_RENUT_HOOK(sub_8264C398, GetRenderStateHook<37>);  // D3DDevice_GetRenderState_CCWStencilZFail
BANJO_RENUT_HOOK(sub_8264C3C8, GetRenderStateHook<38>);  // D3DDevice_GetRenderState_CCWStencilPass
BANJO_RENUT_HOOK(sub_8267B600, GetRenderStateHook<39>);  // D3DDevice_GetRenderState_CCWStencilFunc
BANJO_RENUT_HOOK(sub_8264C450, GetRenderStateHook<40>);  // D3DDevice_GetRenderState_CCWStencilRef
BANJO_RENUT_HOOK(sub_8264C470, GetRenderStateHook<41>);  // D3DDevice_GetRenderState_CCWStencilMask
BANJO_RENUT_HOOK(sub_8267B788, GetRenderStateHook<42>);  // D3DDevice_GetRenderState_CCWStencilWriteMask
BANJO_RENUT_HOOK(sub_8267B790, GetRenderStateHook<43>);  // D3DDevice_GetRenderState_ClipPlaneEnable
BANJO_RENUT_HOOK(sub_8267BA58, GetRenderStateHook<44>);  // D3DDevice_GetRenderState_PointSize
BANJO_RENUT_HOOK(sub_8267BAA8, GetRenderStateHook<45>);  // D3DDevice_GetRenderState_PointSizeMin
BANJO_RENUT_HOOK(sub_8267BA00, GetRenderStateHook<46>);  // D3DDevice_GetRenderState_PointSpriteEnable
BANJO_RENUT_HOOK(sub_8267BAB8, GetRenderStateHook<47>);  // D3DDevice_GetRenderState_PointSizeMax
BANJO_RENUT_HOOK(sub_8267B900, GetRenderStateHook<48>);  // D3DDevice_GetRenderState_MultiSampleAntiAlias
BANJO_RENUT_HOOK(sub_8267B928, GetRenderStateHook<49>);  // D3DDevice_GetRenderState_MultiSampleMask
BANJO_RENUT_HOOK(sub_8267B800, GetRenderStateHook<50>);  // D3DDevice_GetRenderState_ScissorTestEnable
BANJO_RENUT_HOOK(sub_8267B8B0, GetRenderStateHook<51>);  // D3DDevice_GetRenderState_SlopeScaleDepthBias
BANJO_RENUT_HOOK(sub_8267B8D0, GetRenderStateHook<52>);  // D3DDevice_GetRenderState_DepthBias
BANJO_RENUT_HOOK(sub_8267B930, GetRenderStateHook<53>);  // D3DDevice_GetRenderState_ColorWriteEnable
BANJO_RENUT_HOOK(sub_8267B970, GetRenderStateHook<54>);  // D3DDevice_GetRenderState_ColorWriteEnable1
BANJO_RENUT_HOOK(sub_8267B9B0, GetRenderStateHook<55>);  // D3DDevice_GetRenderState_ColorWriteEnable2
BANJO_RENUT_HOOK(sub_8267B9F0, GetRenderStateHook<56>);  // D3DDevice_GetRenderState_ColorWriteEnable3
BANJO_RENUT_HOOK(sub_8267C130, GetRenderStateHook<57>);  // D3DDevice_GetRenderState_TessellationMode
BANJO_RENUT_HOOK(sub_8267C0C0, GetRenderStateHook<58>);  // D3DDevice_GetRenderState_MinTessellationLevel
BANJO_RENUT_HOOK(sub_8267C0F8, GetRenderStateHook<59>);  // D3DDevice_GetRenderState_MaxTessellationLevel
BANJO_RENUT_HOOK(sub_8267BD28, GetRenderStateHook<60>);  // D3DDevice_GetRenderState_Wrap0
BANJO_RENUT_HOOK(sub_8267BD38, GetRenderStateHook<61>);  // D3DDevice_GetRenderState_Wrap1
BANJO_RENUT_HOOK(sub_8267BD48, GetRenderStateHook<62>);  // D3DDevice_GetRenderState_Wrap2
BANJO_RENUT_HOOK(sub_8267BD58, GetRenderStateHook<63>);  // D3DDevice_GetRenderState_Wrap3
BANJO_RENUT_HOOK(sub_8267BD68, GetRenderStateHook<64>);  // D3DDevice_GetRenderState_Wrap4
BANJO_RENUT_HOOK(sub_8267BD78, GetRenderStateHook<65>);  // D3DDevice_GetRenderState_Wrap5
BANJO_RENUT_HOOK(sub_8267BD88, GetRenderStateHook<66>);  // D3DDevice_GetRenderState_Wrap6
BANJO_RENUT_HOOK(sub_8267BD98, GetRenderStateHook<67>);  // D3DDevice_GetRenderState_Wrap7
BANJO_RENUT_HOOK(sub_8267BDA8, GetRenderStateHook<68>);  // D3DDevice_GetRenderState_Wrap8
BANJO_RENUT_HOOK(sub_8267BDB8, GetRenderStateHook<69>);  // D3DDevice_GetRenderState_Wrap9
BANJO_RENUT_HOOK(sub_8267BDC8, GetRenderStateHook<70>);  // D3DDevice_GetRenderState_Wrap10
BANJO_RENUT_HOOK(sub_8267BDD8, GetRenderStateHook<71>);  // D3DDevice_GetRenderState_Wrap11
BANJO_RENUT_HOOK(sub_8267BDE8, GetRenderStateHook<72>);  // D3DDevice_GetRenderState_Wrap12
BANJO_RENUT_HOOK(sub_8267BDF8, GetRenderStateHook<73>);  // D3DDevice_GetRenderState_Wrap13
BANJO_RENUT_HOOK(sub_8267BE08, GetRenderStateHook<74>);  // D3DDevice_GetRenderState_Wrap14
BANJO_RENUT_HOOK(sub_8267BE18, GetRenderStateHook<75>);  // D3DDevice_GetRenderState_Wrap15
BANJO_RENUT_HOOK(sub_8267BE28, GetRenderStateHook<76>);  // D3DDevice_GetRenderState_ViewportEnable
BANJO_RENUT_HOOK(sub_8267BEC8, GetRenderStateHook<77>);  // D3DDevice_GetRenderState_HighPrecisionBlendEnable
BANJO_RENUT_HOOK(sub_8267BF60, GetRenderStateHook<78>);  // D3DDevice_GetRenderState_HighPrecisionBlendEnable1
BANJO_RENUT_HOOK(sub_8267BFF8, GetRenderStateHook<79>);  // D3DDevice_GetRenderState_HighPrecisionBlendEnable2
BANJO_RENUT_HOOK(sub_8267C090, GetRenderStateHook<80>);  // D3DDevice_GetRenderState_HighPrecisionBlendEnable3
BANJO_RENUT_HOOK(sub_8267C168, GetRenderStateHook<81>);  // D3DDevice_GetRenderState_HalfPixelOffset
BANJO_RENUT_HOOK(sub_8267C198, GetRenderStateHook<82>);  // D3DDevice_GetRenderState_PrimitiveResetEnable
BANJO_RENUT_HOOK(sub_8267C1C8, GetRenderStateHook<83>);  // D3DDevice_GetRenderState_PrimitiveResetIndex
BANJO_RENUT_HOOK(sub_8267C1F0, GetRenderStateHook<84>);  // D3DDevice_GetRenderState_AlphaToMaskEnable
BANJO_RENUT_HOOK(sub_8267C220, GetRenderStateHook<85>);  // D3DDevice_GetRenderState_AlphaToMaskOffsets
BANJO_RENUT_HOOK(sub_8267C260, GetRenderStateHook<86>);  // D3DDevice_GetRenderState_GuardBand_X
BANJO_RENUT_HOOK(sub_8267C290, GetRenderStateHook<87>);  // D3DDevice_GetRenderState_GuardBand_Y
BANJO_RENUT_HOOK(sub_8267C2B8, GetRenderStateHook<88>);  // D3DDevice_GetRenderState_DiscardBand_X
BANJO_RENUT_HOOK(sub_8267C2E8, GetRenderStateHook<89>);  // D3DDevice_GetRenderState_DiscardBand_Y
BANJO_RENUT_HOOK(sub_8267C350, GetRenderStateHook<90>);  // D3DDevice_GetRenderState_HiStencilEnable
BANJO_RENUT_HOOK(sub_8267C380, GetRenderStateHook<91>);  // D3DDevice_GetRenderState_HiStencilWriteEnable
BANJO_RENUT_HOOK(sub_8267C3B0, GetRenderStateHook<92>);  // D3DDevice_GetRenderState_HiStencilFunc
BANJO_RENUT_HOOK(sub_8267C3D8, GetRenderStateHook<93>);  // D3DDevice_GetRenderState_HiStencilRef
BANJO_RENUT_HOOK(sub_8267C3E8, GetRenderStateHook<94>);  // D3DDevice_GetRenderState_PresentInterval
BANJO_RENUT_HOOK(sub_8267C400, GetRenderStateHook<95>);  // D3DDevice_GetRenderState_PresentImmediateThreshold
BANJO_RENUT_HOOK(sub_8267C310, GetRenderStateHook<96>);  // D3DDevice_GetRenderState_HiZEnable
BANJO_RENUT_HOOK(sub_8267C320, GetRenderStateHook<97>);  // D3DDevice_GetRenderState_HiZWriteEnable
BANJO_RENUT_HOOK(sub_8267B360, GetRenderStateHook<98>);  // D3DDevice_GetRenderState_LastPixel
BANJO_RENUT_HOOK(sub_8267B3B0, GetRenderStateHook<99>);  // D3DDevice_GetRenderState_LineWidth
BANJO_RENUT_HOOK(sub_8267B408, GetRenderStateHook<100>);  // D3DDevice_GetRenderState_Buffer2Frames

// Sampler state getters (off_82E81550, field 0).
BANJO_RENUT_HOOK(sub_8267CA58, GetSamplerStateHook<0>);  // D3DDevice_GetSamplerState_AddressU
BANJO_RENUT_HOOK(sub_8267CA70, GetSamplerStateHook<1>);  // D3DDevice_GetSamplerState_AddressV
BANJO_RENUT_HOOK(sub_8267CAC0, GetSamplerStateHook<2>);  // D3DDevice_GetSamplerState_AddressW
BANJO_RENUT_HOOK(sub_8267CA38, GetSamplerStateHook<3>);  // D3DDevice_GetSamplerState_BorderColor
BANJO_RENUT_HOOK(sub_8267C4F0, GetSamplerStateHook<4>);  // D3DDevice_GetSamplerState_MagFilter
BANJO_RENUT_HOOK(sub_8267C410, GetSamplerStateHook<5>);  // D3DDevice_GetSamplerState_MinFilter
BANJO_RENUT_HOOK(sub_8267C5C8, GetSamplerStateHook<6>);  // D3DDevice_GetSamplerState_MipFilter
BANJO_RENUT_HOOK(sub_8267C808, GetSamplerStateHook<7>);  // D3DDevice_GetSamplerState_MipMapLodBias
BANJO_RENUT_HOOK(sub_8267C848, GetSamplerStateHook<8>);  // D3DDevice_GetSamplerState_MaxMipLevel
BANJO_RENUT_HOOK(sub_8267C6F0, GetSamplerStateHook<9>);  // D3DDevice_GetSamplerState_MaxAnisotropy
BANJO_RENUT_HOOK(sub_8267C5B8, GetSamplerStateHook<10>);  // D3DDevice_GetSamplerState_MagFilterZ
BANJO_RENUT_HOOK(sub_8267C4E0, GetSamplerStateHook<11>);  // D3DDevice_GetSamplerState_MinFilterZ
BANJO_RENUT_HOOK(sub_8267C678, GetSamplerStateHook<12>);  // D3DDevice_GetSamplerState_SeparateZFilterEnable
BANJO_RENUT_HOOK(sub_8267C9D8, GetSamplerStateHook<13>);  // D3DDevice_GetSamplerState_MinMipLevel
BANJO_RENUT_HOOK(sub_8267CB18, GetSamplerStateHook<14>);  // D3DDevice_GetSamplerState_TrilinearThreshold
BANJO_RENUT_HOOK(sub_8267C760, GetSamplerStateHook<15>);  // D3DDevice_GetSamplerState_AnisotropyBias
BANJO_RENUT_HOOK(sub_8267CB70, GetSamplerStateHook<16>);  // D3DDevice_GetSamplerState_HGradientExpBias
BANJO_RENUT_HOOK(sub_8267CBC8, GetSamplerStateHook<17>);  // D3DDevice_GetSamplerState_VGradientExpBias
BANJO_RENUT_HOOK(sub_8267CC98, GetSamplerStateHook<18>);  // D3DDevice_GetSamplerState_WhiteBorderColorW
BANJO_RENUT_HOOK(sub_8267CCF8, GetSamplerStateHook<19>);  // D3DDevice_GetSamplerState_PointBorderEnable
#endif  // DS2_NB_STATE_HOOKS
