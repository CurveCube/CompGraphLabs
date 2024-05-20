#pragma once

#include "framework.h"

#define MAX_LIGHT 64
#define CSM_SPLIT_COUNT 4


struct PointLight {
    XMFLOAT4 pos;
    XMFLOAT4 color;
};


struct DirectionalLight {
    struct DirectionalLightInfo {
        XMFLOAT4 direction;
        XMFLOAT4 color;
        XMMATRIX viewProjectionMatrix;
        XMUINT4 splitSizeRatio;
    };

    float r = 250.0f;
    float theta = 0.595f;
    float phi = 1.3f;

    XMFLOAT4 direction;
    XMFLOAT4 color;
    XMMATRIX projectionMatrices[CSM_SPLIT_COUNT];
    XMMATRIX viewProjectionMatrices[CSM_SPLIT_COUNT];
    UINT splitSizeRatio[CSM_SPLIT_COUNT];

    DirectionalLight() {
        for (UINT i = 0; i < CSM_SPLIT_COUNT; ++i) {
            splitSizeRatio[i] = i + 1;
        }
        
        color = XMFLOAT4(1.0f, 1.0f, 1.0f, 5.0f);
        XMFLOAT4 pos = XMFLOAT4(
            r * cosf(theta) * cosf(phi),
            r * sinf(theta),
            r * cosf(theta) * sinf(phi),
            0.0f
        );
        direction = XMFLOAT4(cosf(theta) * cosf(phi), sinf(theta), cosf(theta) * sinf(phi), 0.0f);

        float upTheta = theta + XM_PIDIV2;
        XMMATRIX viewMatrix = XMMatrixLookAtRH(
            XMVectorSet(pos.x, pos.y, pos.z, 0.0f),
            XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
            XMVectorSet(cosf(upTheta) * cosf(phi), sinf(upTheta), cosf(upTheta) * sinf(phi), 0.0f)
        );

        for (UINT i = 0; i < CSM_SPLIT_COUNT; ++i) {
            float ratio = splitSizeRatio[i];
            projectionMatrices[i] = XMMatrixOrthographicRH(baseWidth_ * ratio, baseHeight_ * ratio, baseNear_, baseFar_);
            viewProjectionMatrices[i] = XMMatrixMultiply(viewMatrix, projectionMatrices[i]);
        }
    };

    DirectionalLightInfo GetInfo() {
        DirectionalLightInfo info;
        info.color = color;
        info.direction = direction;
        info.viewProjectionMatrix = viewProjectionMatrices[0];
        info.splitSizeRatio.x = splitSizeRatio[0];
        info.splitSizeRatio.y = splitSizeRatio[1];
        info.splitSizeRatio.z = splitSizeRatio[2];
        info.splitSizeRatio.w = splitSizeRatio[3];
        return info;
    };

    void Update(const XMFLOAT3& focus) {
        XMFLOAT4 pos = XMFLOAT4(
            r * cosf(theta) * cosf(phi) + focus.x,
            r * sinf(theta) + focus.y,
            r * cosf(theta) * sinf(phi) + focus.z,
            0.0f
        );
        direction = XMFLOAT4(cosf(theta) * cosf(phi), sinf(theta), cosf(theta) * sinf(phi), 0.0f);

        float upTheta = theta + XM_PIDIV2;
        XMMATRIX viewMatrix = XMMatrixLookAtRH(
            XMVectorSet(pos.x, pos.y, pos.z, 0.0f),
            XMVectorSet(focus.x, focus.y, focus.z, 0.0f),
            XMVectorSet(cosf(upTheta) * cosf(phi), sinf(upTheta), cosf(upTheta) * sinf(phi), 0.0f)
        );

        for (UINT i = 0; i < CSM_SPLIT_COUNT; ++i) {
            viewProjectionMatrices[i] = XMMatrixMultiply(viewMatrix, projectionMatrices[i]);
        }
    };

private:
    float baseWidth_ = 100.0f;
    float baseHeight_ = 100.0f;
    float baseNear_ = 0.01f;
    float baseFar_ = 1000.0f;
};