// Shader Model 5.0
#include "Header.hlsli"

float4 Main(PSInput input) : SV_TARGET {
    return g_texture.Sample(g_sampler, input.uv);
}
