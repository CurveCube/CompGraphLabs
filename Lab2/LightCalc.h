#include "SceneMatrixBuffer.h"

float3 CalculateColor(in float3 objColor, in float3 objNormal, in float3 pos, in float shine) {
    float3 finalColor = ambientColor.xyz * objColor;

    [unroll]
    for (int i = 0; i < lightParams.x; i++) {
        float3 lightDir = lights[i].lightPos.xyz - pos;
        float lightDist = length(lightDir);
        lightDir /= lightDist;

        float3 color = objColor * lights[i].lightColor.xyz * lights[i].lightColor.w;
        float atten = clamp(1.0 / (lightDist * lightDist), 0, 1);

        finalColor += max(dot(lightDir, objNormal), 0) * atten * color;

        float3 viewDir = normalize(cameraPos.xyz - pos);
        float3 reflectDir = reflect(-lightDir, objNormal);
        float spec = shine > 0 ? pow(max(dot(viewDir, reflectDir), 0.0), shine) : 0.0;

        finalColor += spec * color;
    }

    return finalColor;
}
