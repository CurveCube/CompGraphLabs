cbuffer SceneMatrixBuffer : register (b0) {
    float4x4 worldMatrix;
    float4x4 viewProjectionMatrix;
    float4 cameraPos;
}

struct VS_INPUT {
    float3 position : POSITION;
};

struct PS_INPUT {
    float4 position : SV_POSITION;
    float4 worldPos : POSITION;
};

PS_INPUT main(VS_INPUT input) {
    PS_INPUT output;

    output.worldPos = mul(worldMatrix, float4(input.position, 1.0f));
    output.position = mul(viewProjectionMatrix, output.worldPos);

    return output;
}