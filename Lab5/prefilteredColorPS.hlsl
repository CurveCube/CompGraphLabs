TextureCube colorTexture : register (t0);
SamplerState colorSampler : register (s0);

cbuffer roughnessBuffer : register (b0) {
	float4 roughness;
}

struct PS_INPUT {
    float4 position : SV_POSITION;
    float3 localPos : POSITION;
};

static float PI = 3.14159265359f;

float RadicalInverse_Vdc(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_Vdc(i));
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
	float a = roughness * roughness;

	float phi = 2.0 * PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

	float3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;

	float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
	float3 tangent = normalize(cross(up, N));
	float3 bitangent = cross(N, tangent);

	return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

float distributionGGX(float3 n, float3 h, float roughness)
{
	float pi = 3.14159265359f;
	float num = roughness * roughness;
	float denom = max(dot(n, h), 0.0f);
	denom = denom * denom * (num - 1.0f) + 1.0f;
	denom = pi * denom * denom;
	return num / denom;
}

float4 main(PS_INPUT input) : SV_TARGET
{
	float3 norm = normalize(input.localPos);
	float3 view = norm;
	float totalWeight = 0.0f;
	float3 prefilteredColor = float3(0.0f, 0.0f, 0.0f);
	static const uint SAMPLE_COUNT = 1024u;

	for (uint i = 0u; i < SAMPLE_COUNT; ++i)
	{
		float2 Xi = Hammersley(i, SAMPLE_COUNT);
		float3 H = ImportanceSampleGGX(Xi, norm, roughness.x);
		float3 L = normalize(2.0f * dot(view, H) * H - view);
		float ndotl = max(dot(norm, L), 0.0f);
		float ndoth = max(dot(norm, H), 0.0f);
		float hdotv = max(dot(H, view), 0.0f);
		float D = distributionGGX(norm, H, roughness.x);
		float pdf = (D * ndoth / (4.0f * hdotv)) + 0.0001f;
		float resolution = 512.0f;
		float saTexel = 4.0f * PI / (6.0f * resolution * resolution);
		float saSample = 1.0f / (float(SAMPLE_COUNT) * pdf + 0.0001f);
		float mipLevel = roughness.x == 0.0f ? 0.0f : 0.5f * log2(saSample / saTexel);

		if (ndotl > 0.0f)
		{
			prefilteredColor += colorTexture.SampleLevel(colorSampler, L, mipLevel).xyz * ndotl;
			totalWeight += ndotl;
		}
	}
	prefilteredColor = prefilteredColor / totalWeight;

	return float4(prefilteredColor, 1.0f);
}