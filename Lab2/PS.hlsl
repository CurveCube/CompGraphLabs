#include "LightCalc.h"

Texture2D colorTexture : register (t0);
SamplerState colorSampler : register(s0);

cbuffer WorldMatrixBuffer : register (b0) {
    float4x4 worldMatrix;
    float4 shine;
};

struct PS_INPUT {
    float4 position : SV_POSITION;
    float4 worldPos : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
};

float4 main(PS_INPUT input) : SV_TARGET {
    return float4(CalculateColor(colorTexture.Sample(colorSampler, input.uv).xyz,
        normalize(input.normal), input.worldPos.xyz, shine.x), 1.0f);
}