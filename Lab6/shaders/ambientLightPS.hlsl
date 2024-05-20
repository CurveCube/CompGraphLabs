#include "shaders/PBR.hlsli"

Texture2D colorTexture : register (t0);
Texture2D featureTexture : register (t1);
Texture2D normalTexture : register (t2);
Texture2D depthTexture : register (t3);
#ifndef SSAO_MASK
TextureCube irradianceTexture : register (t4);
TextureCube prefilteredTexture : register (t5);
Texture2D brdfTexture : register (t6);
#endif
SamplerState textureSampler : register (s0);
#ifndef SSAO_MASK
SamplerState samplerAvg : register (s1);
#endif

cbuffer MatricesBuffer : register (b0) {
    float4 cameraPos;
    float4x4 projectionMatrix;
    float4x4 invProjectionMatrix;
    float4x4 viewMatrix;
    float4x4 invViewMatrix;
};

#if defined(SSAO_MASK) || defined(WITH_SSAO)
cbuffer SSAOParamsBuffer : register (b1) {
    float4 parameters;
    float4 sizes;
    float4 samples[64];
    float4 noise[16];
};
#endif

struct PS_INPUT {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

static float MAX_REFLECTION_LOD = 4.0f;


#if defined(SSAO_MASK) || defined(WITH_SSAO)
float calculateOcclusion(in float2 uv, in float4 viewPos, in float3 normal) {
    int i = ((int)floor(uv.x * sizes.x)) % 4;
    int j = ((int)floor(uv.y * sizes.y)) % 4;

    float3 rvec = noise[j * 4 + i].xyz;
    float3 tangent = normalize(rvec - normal * dot(rvec, normal));
    float3 bitangent = cross(normal, tangent);

    normal = mul(viewMatrix, float4(normal, 0.0f)).xyz;
    tangent = mul(viewMatrix, float4(tangent, 0.0f)).xyz;
    bitangent = mul(viewMatrix, float4(bitangent, 0.0f)).xyz;

    int occlusion = 0;
    for (int k = 0; k < (int)parameters.x; ++k) {
        float3 samplePos = viewPos + (samples[k].x * tangent + samples[k].y * bitangent + samples[k].z * normal) * parameters.y;
        float4 samplePosProj = mul(projectionMatrix, float4(samplePos, 1.0f));
        float3 samplePosNDC = samplePosProj.xyz / samplePosProj.w;
        float2 samplePosUV = float2(samplePosNDC.x * 0.5 + 0.5, 0.5 - 0.5 * samplePosNDC.y);
        float sampleDepth = depthTexture.Sample(textureSampler, samplePosUV).x;
        if (sampleDepth < samplePosNDC.z || abs(samplePosNDC.z - sampleDepth) > parameters.z) {
            ++occlusion;
        }
    }
    return occlusion / parameters.x;
}
#endif


float4 main(PS_INPUT input) : SV_TARGET {
    float4 ndc = float4(input.uv.x * 2.0f - 1.0f, 1.0f - input.uv.y * 2.0f, depthTexture.Sample(textureSampler, input.uv).x, 1.0f);
    float4 viewPosition = mul(invProjectionMatrix, ndc);
    viewPosition /= viewPosition.w;
    float4 worldPosition = mul(invViewMatrix, viewPosition);

    float3 objectNormal = normalize(normalTexture.Sample(textureSampler, input.uv).xyz * 2.0f - 1.0f);

#if defined(SSAO_MASK) || defined(WITH_SSAO)
    float occlusionFactor = calculateOcclusion(input.uv, viewPosition, objectNormal);
#endif
#ifdef SSAO_MASK
    return float4(occlusionFactor, occlusionFactor, occlusionFactor, 1.0f);
#else
    float3 objectColor = colorTexture.Sample(textureSampler, input.uv).xyz;
    float4 feature = featureTexture.Sample(textureSampler, input.uv);
    float3 viewDirection = normalize(cameraPos.xyz - worldPosition.xyz);

    float3 R = reflect(-viewDirection, objectNormal);
    float3 prefilteredColor = prefilteredTexture.SampleLevel(samplerAvg, R, feature.y * MAX_REFLECTION_LOD).xyz;
    float3 F0 = lerp(float3(feature.w, feature.w, feature.w), objectColor.xyz, feature.z);
    float2 envBRDF = brdfTexture.Sample(samplerAvg, float2(max(dot(objectNormal, viewDirection), 0.0f), feature.y)).xy;
    float3 specular = prefilteredColor * (F0 * envBRDF.x + envBRDF.y);
    float3 FR = fresnelRoughnessFunction(objectColor, objectNormal, viewDirection, feature.z, feature.y, feature.w);
    float3 kD = 1.0f - FR;
    kD *= 1.0f - feature.z;
    float3 irradiance = irradianceTexture.Sample(samplerAvg, objectNormal).xyz;
    float3 ambient = irradiance * objectColor * kD + specular;
    float3 color = ambient * feature.x;
#ifdef WITH_SSAO
    color *= occlusionFactor;
#endif

    return float4(color, 1.0f);
#endif
}