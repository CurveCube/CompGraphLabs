struct PS_INPUT {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

struct PS_OUTPUT
{
    float avg : SV_Target0;
    float min : SV_Target1;
    float max : SV_Target2;
};

Texture2D avgTexture : register (t0);
SamplerState avgSampler : register(s0);
Texture2D minTexture : register (t1);
SamplerState minSampler : register(s1);
Texture2D maxTexture : register (t2);
SamplerState maxSampler : register(s2);

PS_OUTPUT main(PS_INPUT input) : SV_TARGET{
    PS_OUTPUT output;
    output.avg = avgTexture.Sample(avgSampler, input.uv).x;
    output.min = minTexture.Sample(minSampler, input.uv).x;
    output.max = maxTexture.Sample(maxSampler, input.uv).x;

    return output;
}