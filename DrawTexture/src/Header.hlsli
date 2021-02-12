struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D<float4> g_texture : register(t0);
SamplerState g_sampler : register(s0);
