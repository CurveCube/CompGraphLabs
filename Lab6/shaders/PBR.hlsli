static float PI = 3.14159265359f;

float3 fresnelFunction(in float3 objColor, in float3 h, in float3 v, in float metallic, in float dielectricF0) {
    float3 f = dielectricF0 * (1 - metallic) + objColor * metallic;
    return f + (1.0f - f) * pow(1.0f - max(dot(h, v), 0.0f), 5);
}

float3 fresnelRoughnessFunction(in float3 objColor, in float3 n, in float3 v, in float metallic, in float roughness, in float dielectricF0) {
    float3 f = dielectricF0 * (1 - metallic) + objColor * metallic;
    return f + (max(1.0f - roughness, f) - f) * pow(1.0f - max(dot(n, v), 0.0f), 5);
}

float distributionGGX(in float3 n, in float3 h, in float roughness) {
    float num = roughness * roughness;
    float denom = max(dot(n, h), 0.0f);
    denom = denom * denom * (num - 1.0f) + 1.0f;
    denom = PI * denom * denom;
    return num / denom;
}

float geometrySchlickGGX(in float nv_l, in float roughness) {
    float k = roughness + 1.0f;
    k = k * k / 8.0f;
    return nv_l / (nv_l * (1.0f - k) + k);
}

float geometrySmith(in float3 n, in float3 v, in float3 l, in float roughness) {
    return geometrySchlickGGX(max(dot(n, v), 0.0f), roughness) * geometrySchlickGGX(max(dot(n, l), 0.0f), roughness);
}