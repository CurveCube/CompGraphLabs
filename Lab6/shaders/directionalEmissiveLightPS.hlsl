#include "shaders/PBR.hlsli"

Texture2D colorTexture : register (t0);
Texture2D featureTexture : register (t1);
Texture2D normalTexture : register (t2);
Texture2D emissiveTexture : register (t3);
Texture2D depthTexture : register (t4);
Texture2D shadowMaps[4] : register (t5);
SamplerState textureSampler : register (s0);
SamplerComparisonState samplerPcf : register (s1);

cbuffer DirectionalLightBuffer : register (b0) {
    float4 lightDir;
    float4 lightColor;
    float4x4 viewProjectionMatrix;
    int4 splitSizeRatio;
};

cbuffer MatricesBuffer : register (b1) {
    float4 cameraPos;
    float4x4 projectionMatrix;
    float4x4 invProjectionMatrix;
    float4x4 viewMatrix;
    float4x4 invViewMatrix;
};

struct PS_INPUT {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

static float4x4 M_uv = float4x4(0.5f, 0, 0, 0, 0, -0.5f, 0, 0, 0, 0, 1.0f, 0, 0.5f, 0.5f, 0, 1.0f);


float4 shadowFactor(in float4 pos) {
    float4 lightProjPos = mul(viewProjectionMatrix, pos);
    int shadowSplitSizeRatio[4] = { splitSizeRatio[0], splitSizeRatio[1], splitSizeRatio[2], splitSizeRatio[3] };
    int i = 0;
    while ((lightProjPos.x > 1.0f || lightProjPos.x < -1.0f || lightProjPos.y > 1.0f || lightProjPos.y < -1.0f) && i < 3) {
        ++i;
        lightProjPos.xy = lightProjPos.xy * shadowSplitSizeRatio[i - 1] / shadowSplitSizeRatio[i];
    }
    lightProjPos = mul(lightProjPos, M_uv);
    if (i == 0) {
        return float4(shadowMaps[0].SampleCmp(samplerPcf, lightProjPos.xy, lightProjPos.z), 1.0f, 0.5f, 0.5f);
    }
    else if (i == 1) {
        return float4(shadowMaps[1].SampleCmp(samplerPcf, lightProjPos.xy, lightProjPos.z), 0.5f, 1.0f, 0.5f);
    }
    else if (i == 2) {
        return float4(shadowMaps[2].SampleCmp(samplerPcf, lightProjPos.xy, lightProjPos.z), 0.5f, 0.5f, 1.0f);
    }
    else if (lightProjPos.x > 1.0f || lightProjPos.x < 0.0f || lightProjPos.y > 1.0f || lightProjPos.y < 0.0f) {
        return float4(shadowMaps[3].SampleCmp(samplerPcf, lightProjPos.xy, lightProjPos.z), 1.0f, 1.0f, 1.0f);
    }
    else {
        return float4(shadowMaps[3].SampleCmp(samplerPcf, lightProjPos.xy, lightProjPos.z), 0.5f, 0.5f, 0.5f);
    }
}


float4 main(PS_INPUT input) : SV_TARGET {
    float4 ndc = float4(input.uv.x * 2.0f - 1.0f, 1.0f - input.uv.y * 2.0f, depthTexture.Sample(textureSampler, input.uv).x, 1.0f);
    float4 viewPosition = mul(invProjectionMatrix, ndc);
    viewPosition /= viewPosition.w;
    float4 worldPosition = mul(invViewMatrix, viewPosition);
    float4 shadowFactors = shadowFactor(worldPosition);

#ifndef SHADOW_SPLITS
    float3 objectColor = colorTexture.Sample(textureSampler, input.uv).xyz;
    float4 feature = featureTexture.Sample(textureSampler, input.uv);
    float3 objectNormal = normalize(normalTexture.Sample(textureSampler, input.uv).xyz * 2.0f - 1.0f);
    float3 emissive = emissiveTexture.Sample(textureSampler, input.uv).xyz;
    float3 viewDirection = normalize(cameraPos.xyz - worldPosition.xyz);
    float3 lightDirection = normalize(lightDir.xyz);
    float3 radiance = lightColor.xyz * lightColor.w * shadowFactors.x;
    float3 h = normalize((viewDirection + lightDirection) / 2.0f);
    float3 F = fresnelFunction(objectColor, h, viewDirection, feature.z, feature.w);
    float NDF = distributionGGX(objectNormal, h, feature.y);
    float G = geometrySmith(objectNormal, viewDirection, lightDirection, feature.y);
    float3 kd = 1.0f - F;
    kd *= 1.0f - feature.z;
    float3 color = (kd * objectColor / PI +
        NDF * G * F / max(4.0f * max(dot(objectNormal, viewDirection), 0.0f) * max(dot(objectNormal, lightDirection), 0.0f), 0.0001f)) *
        radiance * max(dot(objectNormal, lightDirection), 0.0f);
    color += emissive;
#endif

#ifdef SHADOW_SPLITS
    return float4(shadowFactors.x * float3(shadowFactors.y, shadowFactors.z, shadowFactors.w), 1.0f);
#else
    return float4(color, 1.0f);
#endif
}