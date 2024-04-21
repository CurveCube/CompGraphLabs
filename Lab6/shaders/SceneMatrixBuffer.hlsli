struct LIGHT {
    float4 lightPos;
    float4 lightColor;
};

struct DIRECTIONAL_LIGHT {
    float4 lightDir;
    float4 lightColor;
    float4x4 viewProjectionMatrix;
    uint splitSizeRatio[4];
};

cbuffer SceneMatrixBuffer : register (b1) {
    float4x4 viewProjectionMatrix;
    float4 cameraPos;
    int4 lightParams;
    LIGHT lights[3];
    DIRECTIONAL_LIGHT directionalLight;
};