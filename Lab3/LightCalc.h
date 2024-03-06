#include "SceneMatrixBuffer.h"

float3 fresnelFunction(in float3 objColor, in float3 h, in float3 v, in float metalness)
{
    float3 f = float3(0.04f, 0.04f, 0.04f) * (1 - metalness) + objColor * metalness;
    return f + (float3(1.0f, 1.0f, 1.0f) - f) * pow(1.0f - max(dot(h, v), 0.0f), 5);
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
#if defined(DEFAULT)
    float3 finalColor = ambientColor.xyz * objColor;
#else
    float3 finalColor = float3(0.0f, 0.0f, 0.0f);
#endif

    float3 viewDir = normalize(cameraPos.xyz - pos);
    float pi = 3.14159265359f;
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
        kd = kd * (1.0f - metalness);
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
