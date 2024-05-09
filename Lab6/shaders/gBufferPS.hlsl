#ifdef HAS_COLOR_TEXTURE
Texture2D baseColorTexture : register (t0);
SamplerState baseColorSampler : register (s0);
#endif
#ifdef HAS_MR_TEXTURE
Texture2D mrTexture : register (t1);
SamplerState mrSampler : register (s1);
#endif
#ifdef HAS_NORMAL_TEXTURE
Texture2D normalTexture : register (t2);
SamplerState normalSampler : register (s2);
#endif
#ifdef HAS_OCCLUSION_TEXTURE
Texture2D occlusionTexture : register (t3);
SamplerState occlusionSampler : register (s3);
#endif
#ifdef HAS_EMISSIVE_TEXTURE
Texture2D emissiveTexture : register (t4);
SamplerState emissiveSampler : register (s4);
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

struct PS_OUTPUT {
    float4 color : SV_TARGET0;
    float4 feature : SV_TARGET1;
    float4 normal : SV_TARGET2;
    float4 emissive : SV_TARGET3;
};

PS_OUTPUT main(PS_INPUT input) : SV_TARGET {
    PS_OUTPUT output;

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
    output.color = color;

    float occlusion = 1.0f;
    float roughness = MRONFactors.y;
    float metallic = MRONFactors.x;
#ifdef HAS_MR_TEXTURE
    roughness *= mrTexture.Sample(mrSampler, texCoords[roughMetallicTA.y]).y;
    metallic *= mrTexture.Sample(mrSampler, texCoords[roughMetallicTA.y]).z;
#endif
#ifdef HAS_OCCLUSION_TEXTURE
    occlusion = 1.0f + MRONFactors.z * (occlusionTexture.Sample(occlusionSampler, texCoords[occlusionTA.y]).x - 1.0f);
#endif
    output.feature = float4(occlusion, roughness, metallic, 0.04f);

    float3 normal = input.normal;
#if defined(HAS_NORMAL_TEXTURE) && defined(HAS_TANGENT)
    float3 binorm = cross(input.normal, input.tangent.xyz);
    float3 localNorm = normalTexture.Sample(normalSampler, texCoords[normalTA.y]).xyz;
    localNorm = (normalize(localNorm) * 2 - 1.0f) * float3(MRONFactors.w, MRONFactors.w, 1.0f);
    normal = localNorm.x * normalize(input.tangent.xyz) + localNorm.y * normalize(binorm) + localNorm.z * normalize(input.normal);
#endif
    output.normal = float4(normal * 0.5f + 0.5f, 0.0f);

    float4 emissive = float4(0.0f, 0.0f, 0.0f, 0.0f);
#if defined(HAS_EMISSIVE_TEXTURE)
    emissive = float4(emissiveFactorAlphaCutoff.xyz * emissiveTexture.Sample(emissiveSampler, texCoords[emissiveTA.y]).xyz, 0.0f);
#endif
    output.emissive = emissive;

    return output;
}