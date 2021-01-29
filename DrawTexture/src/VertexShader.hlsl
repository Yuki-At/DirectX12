// Shader Model 5.0
#include "Header.hlsli"

PSInput Main(float4 position : POSITION, float2 uv : TEXCOORD) {
    PSInput output;
    output.position = position;
    output.uv = uv;

    return output;
}
