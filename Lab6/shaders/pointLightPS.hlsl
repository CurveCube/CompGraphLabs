#include "shaders/PBR.hlsli"

Texture2D colorTexture : register (t0);
Texture2D featureTexture : register (t1);
Texture2D normalTexture : register (t2);
Texture2D depthTexture : register (t3);
SamplerState textureSampler : register (s0);

cbuffer MatricesBuffer : register (b0) {
    float4 cameraPos;
    float4x4 projectionMatrix;
    float4x4 invProjectionMatrix;
    float4x4 viewMatrix;
    float4x4 invViewMatrix;
};

cbuffer LightBuffer : register (b1) {
    float4 parameters[64];
    float4 lightPos[64];
    float4 lightColor[64];
};

struct PS_INPUT {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD5;
    nointerpolation uint instanceId : INST_ID;
};

float4 main(PS_INPUT input) : SV_TARGET {
    float4 ndc = float4(input.uv.x * 2.0f - 1.0f, 1.0f - input.uv.y * 2.0f, depthTexture.Sample(textureSampler, input.uv).x, 1.0f);
    float4 viewPosition = mul(invProjectionMatrix, ndc);
    viewPosition /= viewPosition.w;
    float4 worldPosition = mul(invViewMatrix, viewPosition);

    float3 lightDir = lightPos[input.instanceId].xyz - worldPosition.xyz;
    float lightDist = length(lightDir);
    if (lightDist > parameters[input.instanceId].x) {
        discard;
    }

    float3 objectNormal = normalize(normalTexture.Sample(textureSampler, input.uv).xyz * 2.0f - 1.0f);
    float3 objectColor = colorTexture.Sample(textureSampler, input.uv).xyz;
    float4 feature = featureTexture.Sample(textureSampler, input.uv);
    float3 viewDirection = normalize(cameraPos.xyz - worldPosition.xyz);

    lightDir /= lightDist;
    float atten = clamp(1.0 / (lightDist * lightDist), 0, 1.0f);
    float3 radiance = lightColor[input.instanceId].xyz * lightColor[input.instanceId].w * atten;
    radiance = lightColor[input.instanceId].w * atten < parameters[input.instanceId].y ? radiance * smoothstep(parameters[input.instanceId].z, parameters[input.instanceId].y, lightColor[input.instanceId].w * atten) : radiance;

    float3 h = normalize((viewDirection + lightDir) / 2.0f);
#if defined(DEFAULT) || defined(FRESNEL)
    float3 F = fresnelFunction(objectColor, h, viewDirection, feature.z, feature.w);
#endif
#if defined(DEFAULT) || defined(ND)
    float NDF = distributionGGX(objectNormal, h, feature.y);
#endif
#if defined(DEFAULT) || defined(GEOMETRY)
    float G = geometrySmith(objectNormal, viewDirection, lightDir, feature.y);
#endif

#if defined(DEFAULT) || defined(FRESNEL)
    float3 kd = 1.0f - F;
    kd *= 1.0f - feature.z;
#endif

#if defined(DEFAULT)
    return float4((kd * objectColor / PI +
        NDF * G * F / max(4.0f * max(dot(objectNormal, viewDirection), 0.0f) * max(dot(objectNormal, lightDir), 0.0f), 0.0001f)) *
        radiance * max(dot(objectNormal, lightDir), 0.0f), 1.0f);
#elif defined(FRESNEL)
    return float4((F / max(4.0f * max(dot(objectNormal, viewDirection), 0.0f) * max(dot(objectNormal, lightDir), 0.0f), 0.0001f)) *
        max(dot(objectNormal, lightDir), 0.0f), 1.0f);
#elif defined(ND)
    return float4(NDF, NDF, NDF, 1.0f);
#elif defined(GEOMETRY)
    return float4(G, G, G, 1.0f);
#endif
}