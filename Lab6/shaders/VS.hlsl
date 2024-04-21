#include "shaders/SceneMatrixBuffer.hlsli"

cbuffer WorldMatrixBuffer : register (b0) {
    float4x4 worldMatrix;
};

struct VS_INPUT {
    float3 position : POSITION;
    float3 normal : NORMAL;
#ifdef HAS_TANGENT
    float4 tangent : TANGENT;
#endif
#ifdef HAS_TEXCOORD_0
    float2 texCoord0 : TEXCOORD0;
#endif
#ifdef HAS_TEXCOORD_1
    float2 texCoord1 : TEXCOORD1;
#endif
#ifdef HAS_TEXCOORD_2
    float2 texCoord2 : TEXCOORD2;
#endif
#ifdef HAS_TEXCOORD_3
    float2 texCoord3 : TEXCOORD3;
#endif
#ifdef HAS_TEXCOORD_4
    float2 texCoord4 : TEXCOORD4;
#endif
#ifdef HAS_COLOR
    float4 color : COLOR;
#endif
};

struct PS_INPUT {
    float4 position : SV_POSITION;
    float4 worldPos : POSITION;
    float3 normal : NORMAL;
#ifdef HAS_TANGENT
    float4 tangent : TANGENT;
#endif
#ifdef HAS_TEXCOORD_0
    float2 texCoord0 : TEXCOORD0;
#endif
#ifdef HAS_TEXCOORD_1
    float2 texCoord1 : TEXCOORD1;
#endif
#ifdef HAS_TEXCOORD_2
    float2 texCoord2 : TEXCOORD2;
#endif
#ifdef HAS_TEXCOORD_3
    float2 texCoord3 : TEXCOORD3;
#endif
#ifdef HAS_TEXCOORD_4
    float2 texCoord4 : TEXCOORD4;
#endif
#ifdef HAS_COLOR
    float4 color : COLOR;
#endif
};

PS_INPUT main(VS_INPUT input) {
    PS_INPUT output;

    output.worldPos = mul(worldMatrix, float4(input.position, 1.0f));
    output.position = mul(viewProjectionMatrix, output.worldPos);
    output.normal = mul(worldMatrix, input.normal);
#ifdef HAS_TANGENT
    output.tangent = input.tangent;
#endif
#ifdef HAS_TEXCOORD_0
    output.texCoord0 = input.texCoord0;
#endif
#ifdef HAS_TEXCOORD_1
    output.texCoord1 = input.texCoord1;
#endif
#ifdef HAS_TEXCOORD_2
    output.texCoord2 = input.texCoord2;
#endif
#ifdef HAS_TEXCOORD_3
    output.texCoord3 = input.texCoord3;
#endif
#ifdef HAS_TEXCOORD_4
    output.texCoord4 = input.texCoord4;
#endif
#ifdef HAS_COLOR
    output.color = input.color;
#endif

    return output;
}