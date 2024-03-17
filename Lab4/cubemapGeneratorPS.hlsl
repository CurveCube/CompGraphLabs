Texture2D colorTexture : register (t0);
SamplerState colorSampler : register(s0);

struct PS_INPUT {
    float4 position : SV_POSITION;
    float4 worldPos : POSITION;
};

float4 main(PS_INPUT input) : SV_TARGET{
    float4 pos = normalize(input.worldPos);
    float PI = 3.14159265359f;
    float u = 1 - atan2(pos.z , pos.x) / (2 * PI);
    //Y - константный дл€ верхней и нижней поверхности куба
    float v = -asin(pos.y) / PI + 0.5f;
    //¬роде математически верно но пространство выворачивает наизнанку от таких трюков
    //float v = -(acos(sqrt(pos.x * pos.x + pos.z * pos.z)) - PI/2) / PI + 0.5f;

    return float4(colorTexture.Sample(colorSampler, float2(u, v)).xyz, 1.0f);
}