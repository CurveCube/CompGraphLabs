Texture2D maxTexture : register (t0);
SamplerState maxSampler : register(s0);

struct PS_INPUT {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

float main(PS_INPUT input) : SV_TARGET {
    return maxTexture.Sample(maxSampler, input.uv).x;
}