struct PS_INPUT {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

cbuffer dtimeBuffer : register (b0) {
    float4 dt_s;
};

Texture2D avgTexture : register (t0);
Texture2D adaptTexture : register (t1);
SamplerState avgSampler : register(s0);

float main(PS_INPUT input) : SV_TARGET{
     float dt = dt_s.x;
    float s = dt_s.y;

    float adapt = adaptTexture.Sample(avgSampler, float2(0.5f, 0.5f)).x + (avgTexture.Sample(avgSampler, float2(0.5f, 0.5f)).x - adaptTexture.Sample(avgSampler, float2(0.5f, 0.5f)).x) * (1.0f - exp(-1.0f * dt / s));

    return adapt;
}