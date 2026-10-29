// ds2-nx (2026-10-29): the 7e3 code view of a float resolve destination (render/textures.cpp, Code7e3SlotLocked).
// Dark Souls II resolves its 7e3 scene and reads the copy through a 2_10_10_10 fetch: on the Xbox 360 each channel is
// the 10-bit 7e3 code the EDRAM held (and the alpha its 2 bits), which the game decodes itself. The host keeps the
// values (RGBA16F), so this pass encodes them back once per resolve into an RGB10A2 texture (whose unorm read is the
// code over 1023, as the console's), instead of every fetch of every pixel doing it (shader_common.h's
// renutFetchAdjust, now only the fallback). The source is read with Load through the view the draw would have bound
// (the fetch's swizzle applied), at the target's own pixels; present_vs draws the fullscreen triangle.
#include "utility.hlsli"

// shader_common.h's renut7e3Code (Float32To7e3, as this port's PM4 renderer and Xenia), over 1023.
float Code7e3(float value)
{
    uint f32 = asuint(clamp(value, 0.0, 31.875));
    uint denormal = ((f32 & 0x7FFFFFu) | 0x800000u) >> min(125u - (f32 >> 23u), 24u);
    uint normal = f32 - (124u << 23u);
    uint biased = f32 < 0x3E800000u ? denormal : normal;
    return float(((biased + 0x7FFFu + ((biased >> 16u) & 1u)) >> 16u) & 0x3FFu) / 1023.0;
}

float4 main(float4 position : SV_Position) : SV_Target0
{
    Texture2D<float4> source = g_Textures[NonUniformResourceIndex(g_Push.textureIndex)];
    const float4 v = source.Load(int3(int2(position.xy), 0));
    return float4(Code7e3(v.x), Code7e3(v.y), Code7e3(v.z), round(saturate(v.w) * 3.0) / 3.0);
}
