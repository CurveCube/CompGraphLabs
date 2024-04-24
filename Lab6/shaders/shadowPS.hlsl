#ifdef HAS_COLOR_TEXTURE
Texture2D baseColorTexture : register (t0);
SamplerState baseColorSampler : register(s0);
#endif

cbuffer AlphaCutoffBuffer : register (b0) {
    float4 baseColor;
    float4 alphaCutoffTexCoord;
};

struct PS_INPUT {
    float4 position : SV_POSITION;
#ifdef HAS_TEXCOORD_0
    float2 texCoord0 : TEXCOORD0;
#endif
#ifdef HAS_TEXCOORD_1
    float2 texCoord1 : TEXCOORD1;
#endif
#ifdef HAS_TEXCOORD_2
    float2 texCoord2 : TEXCOORD2;
#endif
#ifdef HAS_TEXCOORD_3
    float2 texCoord3 : TEXCOORD3;
#endif
#ifdef HAS_TEXCOORD_4
    float2 texCoord4 : TEXCOORD4;
#endif
#ifdef HAS_COLOR
    float4 color : COLOR;
#endif
};

void main(PS_INPUT input) {
    float4 color = baseColor;
#ifdef HAS_COLOR
    color *= input.color;
#endif
#ifdef HAS_COLOR_TEXTURE
    float2 texCoords[] = {
    #ifdef HAS_TEXCOORD_0
        input.texCoord0
    #endif
    #ifdef HAS_TEXCOORD_1
        , input.texCoord1
    #endif
    #ifdef HAS_TEXCOORD_2
        , input.texCoord2
    #endif
    #ifdef HAS_TEXCOORD_3
        , input.texCoord3
    #endif
    #ifdef HAS_TEXCOORD_4
        , input.texCoord4
    #endif
    };
    color *= baseColorTexture.Sample(baseColorSampler, texCoords[int(alphaCutoffTexCoord.y)]);
#endif
    if (color.w < alphaCutoffTexCoord.x) {
        discard;
    }
}