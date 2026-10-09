// Dark Souls II hooks for the native renderer (src/me, from masseffect-nx through banjo-nx and sa2-nx), with this
// game's addresses (docs/ds2-notes.md, "Direct3D do jogo": found by the PM4 headers each function builds and confirmed
// at run time with ds2_d3d_trace).
//
// The renderer reads the game's PM4 ring by itself; these hooks only feed it what the ring cannot say:
//
//   - Shader objects. NoteShaderObject reads the container copy inside a D3D shader object and its microcode pointer.
//     The draw-time shader load is sub_829BF328 (r3 = device; SA2's sub_82556A60, the same code): it reads the bound
//     VS from device+13072 and the PS from device+13068 (the XDK 2.0.21256 layout, the same as sa2-nx's), so the
//     objects are noted there, before the original patches the VS for the vertex declaration.
//   - Draw records, a hint for pairing (NoteDrawCall: primitive type, then r5..r7 as D3D received them):
//       D3DDevice_DrawVertices        sub_829C2E28 (dev, type, start, count)
//       D3DDevice_DrawIndexedVertices sub_829C3240 (dev, type, base, start, count)
//       D3DDevice_DrawVerticesUP      sub_829C2800 (dev, type, count, data, stride)
//
// Every hook calls the original and changes no PPC register itself. The direct renderer's steps (BeginDrawDirect /
// EndDrawDirect, the flush hooks) and the shader dump of sa2-nx are not wired here yet: the shader containers come
// from the disc (tools/extract_shaders.py), and the direct steps wait for the plain ring path to draw this game right.

#include <cstdint>
#include <cstring>

#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc.h>

#include "me/me_native_system.h"

namespace {

inline uint32_t Load32(const uint8_t* base, uint32_t address) {
  uint32_t v;
  std::memcpy(&v, base + address, 4);
  return __builtin_bswap32(v);
}

constexpr uint32_t kDeviceVertexShader = 13072;  // 0x3310
constexpr uint32_t kDevicePixelShader = 13068;   // 0x330C

// The draw records (NoteDrawCall) read the bound shaders from the same device fields.
const bool g_device_layout_set = [] {
  me::native::SetDeviceShaderOffsets(kDeviceVertexShader, kDevicePixelShader);
  return true;
}();

}  // namespace

// Draw-time shader load (FlushState's shader part, called from every Draw*).
REX_EXTERN(__imp__sub_829BF328);
REX_HOOK_RAW(sub_829BF328) {
  const uint32_t device = ctx.r3.u32;
  if (device >= 0x40000000 && device < 0x7F000000) {
    const uint32_t vs = Load32(base, device + kDeviceVertexShader), ps = Load32(base, device + kDevicePixelShader);
    if (!me::native::ShaderObjectsFinished(vs, ps)) {
      me::native::NoteShaderObject(base, vs, true);
      me::native::NoteShaderObject(base, ps, false);
    }
  }
  __imp__sub_829BF328(ctx, base);
}

REX_EXTERN(__imp__sub_829C2E28);
REX_HOOK_RAW(sub_829C2E28) {  // D3DDevice_DrawVertices
  me::native::NoteDrawCall(base, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32);
  __imp__sub_829C2E28(ctx, base);
}

REX_EXTERN(__imp__sub_829C3240);
REX_HOOK_RAW(sub_829C3240) {  // D3DDevice_DrawIndexedVertices
  me::native::NoteDrawCall(base, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32);
  __imp__sub_829C3240(ctx, base);
}

REX_EXTERN(__imp__sub_829C2800);
REX_HOOK_RAW(sub_829C2800) {  // D3DDevice_DrawVerticesUP
  me::native::NoteDrawCall(base, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32);
  __imp__sub_829C2800(ctx, base);
}
