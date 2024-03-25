#include "shaders/SceneMatrixBuffer.h"

TextureCube irradianceTexture : register (t0);
SamplerState irradianceSampler : register (s0);
TextureCube prefilteredTexture : register (t1);
SamplerState prefilteredSampler : register (s1);
Texture2D brdfTexture : register (t2);

float3 fresnelFunction(in float3 objColor, in float3 h, in float3 v, in float metalness)
{
    float3 f = float3(0.04f, 0.04f, 0.04f) * (1 - metalness) + objColor * metalness;
    return f + (float3(1.0f, 1.0f, 1.0f) - f) * pow(1.0f - max(dot(h, v), 0.0f), 5);
}

float3 fresnelRoughnessFunction(in float3 objColor, in float3 n, in float3 v, in float metalness, in float roughness)
{
    float3 f = float3(0.04f, 0.04f, 0.04f) * (1 - metalness) + objColor * metalness;
    return f + (max(float3(1.0f, 1.0f, 1.0f) - float3(roughness, roughness, roughness), f) - f) * pow(1.0f - max(dot(n, v), 0.0f), 5);
}

float distributionGGX(in float3 n, in float3 h, in float roughness)
{
    float pi = 3.14159265359f;
    float num = roughness * roughness;
    float denom = max(dot(n, h), 0.0f);
    denom = denom * denom * (num - 1.0f) + 1.0f;
    denom = pi * denom * denom;
    return num / denom;
}

float geometrySchlickGGX(in float nv_l, in float roughness)
{
    float k = roughness + 1.0f;
    k = k * k / 8.0f;

    return nv_l / (nv_l * (1.0f - k) + k);
}

float geometrySmith(in float3 n, in float3 v, in float3 l, in float roughness)
{
    return geometrySchlickGGX(max(dot(n, v), 0.0f), roughness) * geometrySchlickGGX(max(dot(n, l), 0.0f), roughness);
}

float3 CalculateColor(in float3 objColor, in float3 objNormal, in float3 pos, in float roughness, in float metalness)
{
    float3 viewDir = normalize(cameraPos.xyz - pos);
    static float pi = 3.14159265359f;
    float3 finalColor = float3(0.0f, 0.0f, 0.0f);
#if defined(DEFAULT)
    float3 R = reflect(-viewDir, objNormal);
    static const float MAX_REFLECTION_LOD = 4.0f;
    float3 prefilteredColor = prefilteredTexture.SampleLevel(prefilteredSampler, R, roughness * MAX_REFLECTION_LOD);
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), objColor.xyz, metalness);
    float2 envBRDF = brdfTexture.Sample(prefilteredSampler, float2(max(dot(objNormal, viewDir), 0.0f), roughness));
    float3 specular = prefilteredColor * (F0 * envBRDF.x + envBRDF.y);

    float3 FR = fresnelRoughnessFunction(objColor, objNormal, viewDir, metalness, roughness);
    float3 kD = float3(1.0f, 1.0f, 1.0f) - FR;
    kD *= 1.0 - metalness;
    float3 irradiance = irradianceTexture.Sample(prefilteredSampler, objNormal).xyz;
    float3 ambient = irradiance * objColor * kD + specular;
    finalColor += ambient;
#endif

    [unroll] for (int i = 0; i < lightParams.x; i++)
    {
        float3 lightDir = lights[i].lightPos.xyz - pos;
        float lightDist = length(lightDir);
        lightDir /= lightDist;
        float atten = clamp(1.0 / (lightDist * lightDist), 0, 1.0f);
        float3 radiance = lights[i].lightColor.xyz * lights[i].lightColor.w * atten;

#if defined(DEFAULT) || defined(FRESNEL)
        float3 F = fresnelFunction(objColor, normalize((viewDir + lightDir) / 2.0f), viewDir, metalness);
#endif
#if defined(DEFAULT) || defined(ND)
        float NDF = distributionGGX(objNormal, normalize((viewDir + lightDir) / 2.0f), roughness);
#endif
#if defined(DEFAULT) || defined(GEOMETRY)
        float G = geometrySmith(objNormal, viewDir, lightDir, roughness);
#endif

#if defined(DEFAULT) || defined(FRESNEL)
        float3 kd = float3(1.0f, 1.0f, 1.0f) - F;
        kd *= 1.0f - metalness;
#endif

#if defined(DEFAULT)
        finalColor += (kd * objColor / pi +
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

    return finalColor;
}
