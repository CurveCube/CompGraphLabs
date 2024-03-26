#include "shaders/LightCalc.hlsli"

cbuffer WorldMatrixBuffer : register (b0) {
    float4x4 worldMatrix;
    float3 color;
    float roughness;
    float metalness;
};

struct PS_INPUT {
    float4 position : SV_POSITION;
    float4 worldPos : POSITION;
    float3 normal : NORMAL;
};

float4 main(PS_INPUT input) : SV_TARGET {
    return float4(CalculateColor(color, normalize(input.normal), input.worldPos.xyz,
        clamp(roughness, 0.0001f, 1.0f), clamp(metalness, 0.0f, 1.0f)), 1.0f);
}