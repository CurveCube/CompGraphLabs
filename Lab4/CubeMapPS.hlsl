TextureCube colorTexture : register (t0);
SamplerState colorSampler : register (s0);

struct PS_INPUT {
    float4 position : SV_POSITION;
    float3 localPos : POSITION;
};

float4 main(PS_INPUT input) : SV_TARGET {
    return float4(colorTexture.Sample(colorSampler, input.localPos).xyz, 1.0f);
}