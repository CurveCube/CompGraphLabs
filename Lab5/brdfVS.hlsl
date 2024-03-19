struct VS_INPUT {
    uint vertexId : SV_VertexID;
};

struct PS_INPUT {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

PS_INPUT main(VS_INPUT input) {
    PS_INPUT output;

    float4 pos = float4(0, 0, 0, 0);
    float2 uv = float2(0.0f, 0.0f);

    switch (input.vertexId) {
    case 0:
        pos = float4(1, 1, 0, 1);
        uv = float2(1.0f, 0.0f);
        break;
    case 1:
        pos = float4(1, -1, 0, 1);
        uv = float2(1.0f, 1.0f);
        break;
    case 2:
        pos = float4(-1, -1, 0, 1);
        uv = float2(0.0f, 1.0f);
        break;
    case 3:
        pos = float4(-1, 1, 0, 1);
        uv = float2(0.0f, 0.0f);
        break;
    case 4:
        pos = float4(1, 1, 0, 1);
        uv = float2(1.0f, 0.0f);
        break;
    case 5:
        pos = float4(-1, -1, 0, 1);
        uv = float2(0.0f, 1.0f);
        break;
    }

    output.position = pos;
    output.uv = uv;

    return output;
}
