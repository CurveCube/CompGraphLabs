#include "SceneMatrixBuffer.h"

cbuffer WorldMatrixBuffer : register (b0) {
    float4x4 worldMatrix;
    float4 shine;
};

struct VS_INPUT {
    float3 position : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
};

struct PS_INPUT {
    float4 position : SV_POSITION;
    float4 worldPos : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
};

PS_INPUT main(VS_INPUT input) {
    PS_INPUT output;

    output.worldPos = mul(worldMatrix, float4(input.position, 1.0f));
    output.position = mul(viewProjectionMatrix, output.worldPos);
    output.uv = input.uv;
    output.normal = mul(worldMatrix, input.normal);

    return output;
}