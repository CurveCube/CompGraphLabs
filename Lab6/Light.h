#pragma once

#include "framework.h"

#define MAX_LIGHT 3
#define CSM_SPLIT_COUNT 4


struct SpotLight {
    XMFLOAT4 pos;
    XMFLOAT4 color;
};


struct DirectionalLight {
    XMFLOAT4 pos;
    XMFLOAT4 direction;
    XMFLOAT4 color;
    XMMATRIX viewProjectionMatrices[CSM_SPLIT_COUNT];
    UINT splitSizeRatio[CSM_SPLIT_COUNT];
};