TextureCube colorTexture : register (t0);
SamplerState colorSampler : register (s0);

struct PS_INPUT {
    float4 position : SV_POSITION;
    float3 localPos : POSITION;
};

float4 main(PS_INPUT input) : SV_TARGET{
    float3 normal = normalize(input.localPos);
    float3 dir = abs(normal.z) < 0.999 ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 tangent = normalize(cross(dir, normal));
    float3 binormal = cross(normal, tangent);
    float3 irradiance = float3(0.0f, 0.0f, 0.0f);
    static int N1 = 1000;
    static int N2 = 250;
    static float PI = 3.14159265359f;
    for (int i = 0; i < N1; i++) {
        for (int j = 0; j < N2; j++) {
            float phi = i * (2 * PI / N1);
            float theta = j * (PI / 2 / N2);
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            float3 sampleVec = tangentSample.x * tangent + tangentSample.y * binormal + tangentSample.z * normal;
            irradiance += colorTexture.Sample(colorSampler, sampleVec) * cos(theta) * sin(theta);
        }
    }
    irradiance = PI * irradiance / (N1 * N2);

    return float4(irradiance, 1.0f);
}