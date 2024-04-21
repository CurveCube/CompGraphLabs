cbuffer WorldMatrixBuffer : register (b0) {
    float4x4 worldMatrix;
};

cbuffer ViewMatrixBuffer : register (b1) {
    float4x4 viewProjectionMatrix;
}

struct VS_INPUT {
    float3 position : POSITION;
    float3 normal : NORMAL;
#ifdef HAS_TANGENT
    float4 tangent : TANGENT;
#endif
#ifdef HAS_TEXCOORD_0
    float2 texCoord0 : TEXCOORD_0;
#endif
#ifdef HAS_TEXCOORD_1
    float2 texCoord1 : TEXCOORD_1;
#endif
#ifdef HAS_TEXCOORD_2
    float2 texCoord2 : TEXCOORD_2;
#endif
#ifdef HAS_TEXCOORD_3
    float2 texCoord3 : TEXCOORD_3;
#endif
#ifdef HAS_TEXCOORD_4
    float2 texCoord4 : TEXCOORD_4;
#endif
#ifdef HAS_COLOR
    float4 color : COLOR;
#endif
};

struct PS_INPUT {
    float4 position : SV_POSITION;
#ifdef HAS_TEXCOORD_0
    float2 texCoord0 : TEXCOORD_0;
#endif
#ifdef HAS_TEXCOORD_1
    float2 texCoord1 : TEXCOORD_1;
#endif
#ifdef HAS_TEXCOORD_2
    float2 texCoord2 : TEXCOORD_2;
#endif
#ifdef HAS_TEXCOORD_3
    float2 texCoord3 : TEXCOORD_3;
#endif
#ifdef HAS_TEXCOORD_4
    float2 texCoord4 : TEXCOORD_4;
#endif
#ifdef HAS_COLOR
    float4 color : COLOR;
#endif
};

PS_INPUT main(VS_INPUT input) {
    PS_INPUT output;

    float4 worldPos = mul(worldMatrix, float4(input.position, 1.0f));
    output.position = mul(viewProjectionMatrix, worldPos);
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