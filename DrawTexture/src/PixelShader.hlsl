// Shader Model 5.0
#include "Header.hlsli"

float4 Main(PSInput input) : SV_TARGET {
    return float4(input.uv, 1.0f, 1.0f);
}
