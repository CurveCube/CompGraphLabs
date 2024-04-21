#include "shaders/LightCalc.hlsli"

Texture2D textures[16] : register (t7);
SamplerState samplers[16] : register (s2);

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

float4 main(PS_INPUT input) : SV_TARGET {
#if defined(HAS_COLOR_TEXTURE) || defined(HAS_MR_TEXTURE) || defined(HAS_NORMAL_TEXTURE) || defined(HAS_OCCLUSION_TEXTURE) || defined(HAS_EMISSIVE_TEXTURE)
    float2 texCoords[] = {
    #ifdef HAS_TEXCOORD_0
        texCoord0
    #endif
    #ifdef HAS_TEXCOORD_1
        , texCoord1
    #endif
    #ifdef HAS_TEXCOORD_2
        , texCoord2
    #endif
    #ifdef HAS_TEXCOORD_3
        , texCoord3
    #endif
    #ifdef HAS_TEXCOORD_4
        , texCoord4
    #endif
    };
#endif

    float4 color = baseColorFactor;
#ifdef HAS_COLOR
    color *= input.color;
#endif
#ifdef HAS_COLOR_TEXTURE
    color *= textures[baseColorTA.x * 2 + baseColorTA.w].Sample(samplers[baseColorTA.z], texCoords[baseColorTA.y]);
#endif
#ifdef HAS_ALPHA_CUTOFF
    if (color.w < emissiveFactorAlphaCutoff.w) {
        discard;
    }
#endif

    float3 normal = input.normal;
#if defined(HAS_NORMAL_TEXTURE) && defined(HAS_TANGENT)
    float3 binorm = cross(input.normal, input.tangent);
    float3 localNorm = textures[normalTA.x * 2 + normalTA.w].Sample(samplers[normalTA.z], texCoords[normalTA.y]).xyz;
    localNorm = (normalize(localNorm) * 2 - 1.0f) * float3(MRONFactors.w, MRONFactors.w, 1.0f);
    normal = localNorm.x * normalize(input.tangent) + localNorm.y * normalize(binorm) + localNorm.z * normalize(input.normal);
#endif

    float metallic = MRONFactors.x;
    float roughness = MRONFactors.y;
#ifdef HAS_MR_TEXTURE
    metallic *= textures[roughMetallicTA.x * 2 + roughMetallicTA.w].Sample(samplers[roughMetallicTA.z], texCoords[roughMetallicTA.y]).z;
    roughness *= textures[roughMetallicTA.x * 2 + roughMetallicTA.w].Sample(samplers[roughMetallicTA.z], texCoords[roughMetallicTA.y]).y;
#endif

    float occlusionFactor = 1.0f;
#if defined(HAS_OCCLUSION_TEXTURE) && defined(DEFAULT)
    occlusionFactor = 1.0f + MRONFactors.z * (textures[occlusionTA.x * 2 + occlusionTA.w].Sample(samplers[occlusionTA.z], texCoords[occlusionTA.y]).x - 1.0f);
#endif

    float4 finalColor = float4(CalculateColor(color.xyz, normalize(normal), input.worldPos.xyz,
        clamp(roughness, 0.0001f, 1.0f), clamp(metalness, 0.0f, 1.0f), clamp(occlusionFactor, 0.0f, 1.0f)), color.w);
#if defined(HAS_EMISSIVE_TEXTURE) && defined(DEFAULT)
    finalColor += float4(emissiveFactorAlphaCutoff.xyz * textures[emissiveTA.x * 2 + emissiveTA.w].Sample(samplers[emissiveTA.z], texCoords[emissiveTA.y]).xyz, 0.0f);
#endif

    return finalColor;
}