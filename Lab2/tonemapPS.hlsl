Texture2D colorTexture : register (t0);
Texture2D avgTexture : register (t1);
Texture2D minTexture : register (t2);
Texture2D maxTexture : register (t3);
SamplerState colorSampler : register(s0);

struct PS_INPUT {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

static const float A = 0.1f;
static const float B = 0.50f;
static const float C = 0.1f;
static const float D = 0.20f;
static const float E = 0.02f;
static const float F = 0.30f;
static const float W = 11.2f;
float3 Uncharted2Tonemap(float3 x) {
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float3 TonemapFilmic(float3 color) {
    float avg = exp(avgTexture.Sample(colorSampler, float2(0.5f, 0.5f)).x) - 1.0f;
    float keyValue = 1.03f - 2.0f / (2.0f + log(avg + 1.0f));

    float E = keyValue / clamp(avg, minTexture.Sample(colorSampler, float2(0.5f, 0.5f)).x, maxTexture.Sample(colorSampler, float2(0.5f, 0.5f)).x);
    float3 curr = Uncharted2Tonemap(E * color);
    float3 whiteScale = 1.0f / Uncharted2Tonemap(W);
    return curr * whiteScale;
}

float4 main(PS_INPUT input) : SV_TARGET{
    return float4(TonemapFilmic(colorTexture.Sample(colorSampler, input.uv).xyz), 1.0f);
}