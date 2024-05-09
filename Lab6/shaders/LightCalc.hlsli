#include "shaders/PBR.hlsli"

#ifdef DEFAULT
#ifdef TRANSPARENT
TextureCube irradianceTexture : register (t0);
TextureCube prefilteredTexture : register (t1);
Texture2D brdfTexture : register (t2);
#endif
Texture2D shadowMaps[4] : register (t3);
#if defined(TRANSPARENT) && (defined(WITH_SSAO) || defined(SSAO_MASK))
Texture2D depthTexture : register (t7);
#endif
#ifdef TRANSPARENT
SamplerState samplerAvg : register (s0);
#endif
SamplerComparisonState samplerPcf : register (s1);
#if defined(TRANSPARENT) && (defined(WITH_SSAO) || defined(SSAO_MASK))
SamplerState textureSampler : register (s2);
#endif
#endif

struct LIGHT {
    float4 lightPos;
    float4 lightColor;
};

struct DIRECTIONAL_LIGHT {
    float4 lightDir;
    float4 lightColor;
    float4x4 viewProjectionMatrix;
    int4 splitSizeRatio;
};

cbuffer SceneMatrixBuffer : register (b1) {
    float4x4 viewProjectionMatrix;
    int4 lightParams;
    DIRECTIONAL_LIGHT directionalLight;
    LIGHT lights[64];
};

cbuffer MatricesBuffer : register (b2) {
    float4 cameraPos;
    float4x4 projectionMatrix;
    float4x4 invProjectionMatrix;
    float4x4 viewMatrix;
    float4x4 invViewMatrix;
};

#if defined(TRANSPARENT) && (defined(WITH_SSAO) || defined(SSAO_MASK))
cbuffer SSAOParamsBuffer : register (b3) {
    float4 parameters;
    float4 sizes;
    float4 samples[64];
    float4 noise[16];
};
#endif

static float4x4 M_uv = float4x4(0.5f, 0, 0, 0, 0, -0.5f, 0, 0, 0, 0, 1.0f, 0, 0.5f, 0.5f, 0, 1.0f);
static float MAX_REFLECTION_LOD = 4.0f;

#if defined(DEFAULT)
float4 shadowFactor(in float3 pos) {
    float4 lightProjPos = mul(directionalLight.viewProjectionMatrix, float4(pos, 1.0f));
    uint splitSizeRatio[4] = { directionalLight.splitSizeRatio[0], directionalLight.splitSizeRatio[1],
        directionalLight.splitSizeRatio[2], directionalLight.splitSizeRatio[3] };
    int i = 0;
    while ((lightProjPos.x > 1.0f || lightProjPos.x < -1.0f || lightProjPos.y > 1.0f || lightProjPos.y < -1.0f) && i < 3) {
        ++i;
        lightProjPos.xy = lightProjPos.xy * splitSizeRatio[i - 1] / splitSizeRatio[i];
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
#endif

#if defined(TRANSPARENT) && (defined(WITH_SSAO) || defined(SSAO_MASK))
float calculateOcclusion(in float4 position, in float4 worldPos, in float3 normal) {
    int i = ((int)floor((position.x / position.w * 0.5 + 0.5) * sizes.x)) % 4;
    int j = ((int)floor((position.y / position.w * 0.5 + 0.5) * sizes.y)) % 4;

    float3 rvec = noise[j * 4 + i].xyz;
    float3 tangent = normalize(rvec - normal * dot(rvec, normal));
    float3 bitangent = cross(normal, tangent);

    normal = mul(viewMatrix, float4(normal, 0.0f)).xyz;
    tangent = mul(viewMatrix, float4(tangent, 0.0f)).xyz;
    bitangent = mul(viewMatrix, float4(bitangent, 0.0f)).xyz;

    float3 viewPos = mul(viewMatrix, worldPos).xyz;
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

float3 CalculateColor(in float3 objColor, in float3 objNormal, in float3 pos, in float roughness, in float metallic, in float occlusionFactor) {
#if defined(DEFAULT) || defined(SHADOW_SPLITS)
    float4 shadowFactors = shadowFactor(pos);
#endif
#ifndef SHADOW_SPLITS
    float3 viewDir = normalize(cameraPos.xyz - pos);
    float3 finalColor = float3(0.0f, 0.0f, 0.0f);
#ifdef DEFAULT
#ifdef TRANSPARENT
    float3 R = reflect(-viewDir, objNormal);
    float3 prefilteredColor = prefilteredTexture.SampleLevel(samplerAvg, R, roughness * MAX_REFLECTION_LOD).xyz;
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), objColor.xyz, metallic);
    float2 envBRDF = brdfTexture.Sample(samplerAvg, float2(max(dot(objNormal, viewDir), 0.0f), roughness)).xy;
    float3 specular = prefilteredColor * (F0 * envBRDF.x + envBRDF.y);

    float3 FR = fresnelRoughnessFunction(objColor, objNormal, viewDir, metallic, roughness, 0.04f);
    float3 kD = float3(1.0f, 1.0f, 1.0f) - FR;
    kD *= 1.0 - metallic;
    float3 irradiance = irradianceTexture.Sample(samplerAvg, objNormal).xyz;
    float3 ambient = irradiance * objColor * kD + specular;
    finalColor += ambient * occlusionFactor;
#endif

    float3 lightDir = normalize(directionalLight.lightDir.xyz);
    float3 radiance = directionalLight.lightColor.xyz * directionalLight.lightColor.w * shadowFactors.x;
    float3 h = normalize((viewDir + lightDir) / 2.0f);
    float3 F = fresnelFunction(objColor, h, viewDir, metallic, 0.04f);
    float NDF = distributionGGX(objNormal, h, roughness);
    float G = geometrySmith(objNormal, viewDir, lightDir, roughness);
    float3 kd = 1.0f - F;
    kd *= 1.0f - metallic;
    finalColor += (kd * objColor / PI +
        NDF * G * F / max(4.0f * max(dot(objNormal, viewDir), 0.0f) * max(dot(objNormal, lightDir), 0.0f), 0.0001f)) *
        radiance * max(dot(objNormal, lightDir), 0.0f);
#endif

    [unroll] for (int i = 0; i < lightParams.x; i++) {
        float3 lightDir = lights[i].lightPos.xyz - pos;
        float lightDist = length(lightDir);
        lightDir /= lightDist;
        float atten = clamp(1.0 / (lightDist * lightDist), 0, 1.0f);
        float3 radiance = lights[i].lightColor.xyz * lights[i].lightColor.w * atten;
        float3 h = normalize((viewDir + lightDir) / 2.0f);

#if defined(DEFAULT) || defined(FRESNEL)
        float3 F = fresnelFunction(objColor, h, viewDir, metallic, 0.04f);
#endif
#if defined(DEFAULT) || defined(ND)
        float NDF = distributionGGX(objNormal, h, roughness);
#endif
#if defined(DEFAULT) || defined(GEOMETRY)
        float G = geometrySmith(objNormal, viewDir, lightDir, roughness);
#endif

#if defined(DEFAULT) || defined(FRESNEL)
        float3 kd = 1.0f - F;
        kd *= 1.0f - metallic;
#endif

#if defined(DEFAULT)
        finalColor += (kd * objColor / PI +
            NDF * G * F / max(4.0f * max(dot(objNormal, viewDir), 0.0f) * max(dot(objNormal, lightDir), 0.0f), 0.0001f)) *
            radiance * max(dot(objNormal, lightDir), 0.0f);
#elif defined(FRESNEL)
        finalColor += (F / max(4.0f * max(dot(objNormal, viewDir), 0.0f) * max(dot(objNormal, lightDir), 0.0f), 0.0001f)) *
            max(dot(objNormal, lightDir), 0.0f);
#elif defined(ND)
        finalColor += float3(NDF, NDF, NDF);
#elif defined(GEOMETRY)
        finalColor += float3(G, G, G);
#endif
    }
#endif

#ifdef SHADOW_SPLITS
    return float3(shadowFactors.x, shadowFactors.x, shadowFactors.x) * float3(shadowFactors.y, shadowFactors.z, shadowFactors.w);
#else
    return finalColor;
#endif
}
