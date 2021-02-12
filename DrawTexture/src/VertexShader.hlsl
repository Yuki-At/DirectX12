// Shader Model 5.0
#include "Header.hlsli"

PSInput Main(float4 position : POSITION, float2 uv : TEXCOORD) {
    PSInput result;
    result.position = position;
    result.uv = uv;

    return result;
}
