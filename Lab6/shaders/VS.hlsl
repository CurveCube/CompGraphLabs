#ifndef INSTANCING
cbuffer WorldMatrixBuffer : register (b0) {
    float4x4 worldMatrix;
};
#else
cbuffer WorldMatrixBuffer : register (b0) {
    float4x4 worldMatrix[64];
};
#endif

cbuffer ViewMatrixBuffer : register (b1) {
    float4x4 viewProjectionMatrix;
};

struct VS_INPUT {
    float3 position : POSITION;

#ifdef HAS_NORMAL
    float3 normal : NORMAL;
#endif
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
#ifdef INSTANCING
    uint instanceId : SV_InstanceID;
#endif
};

struct PS_INPUT {
    float4 position : SV_POSITION;

#ifdef HAS_UV_OUT
    float2 uv : TEXCOORD5;
#endif
#ifdef HAS_WORLD_POS_OUT
    float4 worldPos : POSITION;
#endif
#ifdef HAS_NORMAL_OUT
    float3 normal : NORMAL;
#endif
#ifdef HAS_TANGENT_OUT
    float4 tangent : TANGENT;
#endif
#if defined(HAS_TEXCOORD_0) && defined(HAS_TEXCOORD_OUT)
    float2 texCoord0 : TEXCOORD0;
#endif
#if defined(HAS_TEXCOORD_1) && defined(HAS_TEXCOORD_OUT)
    float2 texCoord1 : TEXCOORD1;
#endif
#if defined(HAS_TEXCOORD_2) && defined(HAS_TEXCOORD_OUT)
    float2 texCoord2 : TEXCOORD2;
#endif
#if defined(HAS_TEXCOORD_3) && defined(HAS_TEXCOORD_OUT)
    float2 texCoord3 : TEXCOORD3;
#endif
#if defined(HAS_TEXCOORD_4) && defined(HAS_TEXCOORD_OUT)
    float2 texCoord4 : TEXCOORD4;
#endif
#ifdef HAS_COLOR_OUT
    float4 color : COLOR;
#endif
#ifdef INSTANCING
    nointerpolation uint instanceId : INST_ID;
#endif
};

PS_INPUT main(VS_INPUT input) {
    PS_INPUT output;

#ifdef INSTANCING
    float4 worldPos = mul(worldMatrix[input.instanceId], float4(input.position, 1.0f));
#else
    float4 worldPos = mul(worldMatrix, float4(input.position, 1.0f));
#endif
    output.position = mul(viewProjectionMatrix, worldPos);

#ifdef HAS_UV_OUT
    output.uv = float2(output.position.x / output.position.w * 0.5 + 0.5, 0.5 - output.position.y / output.position.w * 0.5);
#endif
#ifdef HAS_WORLD_POS_OUT
    output.worldPos = worldPos;
#endif
#if defined(HAS_NORMAL_OUT) && defined(HAS_NORMAL)
#ifdef INSTANCING
    output.normal = mul(worldMatrix[input.instanceId], input.normal);
#else
    output.normal = mul(worldMatrix, input.normal);
#endif
#endif
#if defined(HAS_TANGENT_OUT) && defined(HAS_TANGENT)
    output.tangent = input.tangent;
#endif
#if defined(HAS_TEXCOORD_0) && defined(HAS_TEXCOORD_OUT)
    output.texCoord0 = input.texCoord0;
#endif
#if defined(HAS_TEXCOORD_1) && defined(HAS_TEXCOORD_OUT)
    output.texCoord1 = input.texCoord1;
#endif
#if defined(HAS_TEXCOORD_2) && defined(HAS_TEXCOORD_OUT)
    output.texCoord2 = input.texCoord2;
#endif
#if defined(HAS_TEXCOORD_3) && defined(HAS_TEXCOORD_OUT)
    output.texCoord3 = input.texCoord3;
#endif
#if defined(HAS_TEXCOORD_4) && defined(HAS_TEXCOORD_OUT)
    output.texCoord4 = input.texCoord4;
#endif
#if defined(HAS_COLOR_OUT) && defined(HAS_COLOR)
    output.color = input.color;
#endif
#ifdef INSTANCING
    output.instanceId = input.instanceId;
#endif

    return output;
}