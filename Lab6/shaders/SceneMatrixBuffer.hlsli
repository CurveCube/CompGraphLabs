struct LIGHT {
    float4 lightPos;
    float4 lightColor;
};

cbuffer SceneMatrixBuffer : register (b1) {
    float4x4 viewProjectionMatrix;
    float4 cameraPos;
    int4 lightParams;
    LIGHT lights[3];
};