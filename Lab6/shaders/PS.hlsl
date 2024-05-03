#include "shaders/LightCalc.hlsli"

#ifdef HAS_COLOR_TEXTURE
Texture2D baseColorTexture : register (t8);
SamplerState baseColorSampler : register (s2);
#endif
#ifdef HAS_MR_TEXTURE
Texture2D mrTexture : register (t9);
SamplerState mrSampler : register (s3);
#endif
#ifdef HAS_NORMAL_TEXTURE
Texture2D normalTexture : register (t10);
SamplerState normalSampler : register (s4);
#endif
#ifdef HAS_OCCLUSION_TEXTURE
Texture2D occlusionTexture : register (t11);
SamplerState occlusionSampler : register (s5);
#endif
#ifdef HAS_EMISSIVE_TEXTURE
Texture2D emissiveTexture : register (t12);
SamplerState emissiveSampler : register (s6);
#endif

cbuffer MaterialParamsBuffer : register (b0) {
    float4 baseColorFactor;
    float4 MRONFactors;
    float4 emissiveFactorAlphaCutoff;
    int4 baseColorTA;
    int4 roughMetallicTA;
    int4 normalTA;
    int4 emissiveTA;
    int4 occlusionTA;
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

float4 main(PS_INPUT input) : SV_TARGET {
#if defined(HAS_COLOR_TEXTURE) || defined(HAS_MR_TEXTURE) || defined(HAS_NORMAL_TEXTURE) || defined(HAS_OCCLUSION_TEXTURE) || defined(HAS_EMISSIVE_TEXTURE)
    float2 texCoords[] = {
    #ifdef HAS_TEXCOORD_0
        input.texCoord0
    #endif
    #ifdef HAS_TEXCOORD_1
        , input.texCoord1
    #endif
    #ifdef HAS_TEXCOORD_2
        , input.texCoord2
    #endif
    #ifdef HAS_TEXCOORD_3
        , input.texCoord3
    #endif
    #ifdef HAS_TEXCOORD_4
        , input.texCoord4
    #endif
    };
#endif

    float4 color = baseColorFactor;
#ifdef HAS_COLOR
    color *= input.color;
#endif
#ifdef HAS_COLOR_TEXTURE
    color *= baseColorTexture.Sample(baseColorSampler, texCoords[baseColorTA.y]);
#endif
#ifdef HAS_ALPHA_CUTOFF
    if (color.w < emissiveFactorAlphaCutoff.w) {
        discard;
    }
#endif

    float3 normal = input.normal;
#if defined(HAS_NORMAL_TEXTURE) && defined(HAS_TANGENT)
    float3 binorm = cross(input.normal, input.tangent.xyz);
    float3 localNorm = normalTexture.Sample(normalSampler, texCoords[normalTA.y]).xyz;
    localNorm = (normalize(localNorm) * 2 - 1.0f) * float3(MRONFactors.w, MRONFactors.w, 1.0f);
    normal = localNorm.x * normalize(input.tangent.xyz) + localNorm.y * normalize(binorm) + localNorm.z * normalize(input.normal);
#endif

    float metallic = MRONFactors.x;
    float roughness = MRONFactors.y;
#ifdef HAS_MR_TEXTURE
    metallic *= mrTexture.Sample(mrSampler, texCoords[roughMetallicTA.y]).z;
    roughness *= mrTexture.Sample(mrSampler, texCoords[roughMetallicTA.y]).y;
#endif

    float occlusionFactor = 1.0f;
#if defined(HAS_OCCLUSION_TEXTURE) && defined(DEFAULT)
    occlusionFactor = 1.0f + MRONFactors.z * (occlusionTexture.Sample(occlusionSampler, texCoords[occlusionTA.y]).x - 1.0f);
#endif
#if defined(WITH_SSAO)
    float res = calculateOcclusion(input.position, normalize(normal));
    return float4(res, res, res, 1.0f);
    //occlusionFactor *= calculateOcclusion(input.position, normalize(normal));
#endif

    float4 finalColor = float4(CalculateColor(color.xyz, normalize(normal), input.worldPos.xyz,
        clamp(roughness, 0.0001f, 1.0f), clamp(metallic, 0.0f, 1.0f), clamp(occlusionFactor, 0.0f, 1.0f)), color.w);
#if defined(HAS_EMISSIVE_TEXTURE) && defined(DEFAULT)
    finalColor += float4(emissiveFactorAlphaCutoff.xyz * emissiveTexture.Sample(emissiveSampler, texCoords[emissiveTA.y]).xyz, 0.0f);
#endif

    return finalColor;
}