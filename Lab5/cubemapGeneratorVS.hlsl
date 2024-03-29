cbuffer CameraBuffer : register (b0) {
    float4x4 viewProjectionMatrix;
}

struct VS_INPUT {
    float3 position : POSITION;
};

struct PS_INPUT {
    float4 position : SV_POSITION;
    float3 localPos : POSITION;
};

PS_INPUT main(VS_INPUT input) {
    PS_INPUT output;

    output.position = mul(viewProjectionMatrix, float4(input.position, 1.0f));
    output.localPos = input.position;

    return output;
}