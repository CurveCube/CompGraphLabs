#pragma once

#include "framework.h"

#define MAX_LIGHT 3
#define CSM_SPLIT_COUNT 4


struct SpotLight {
    XMFLOAT4 pos;
    XMFLOAT4 color;
};


struct DirectionalLight {
    struct DirectionalLightInfo {
        XMFLOAT4 direction;
        XMFLOAT4 color;
        XMMATRIX viewProjectionMatrix;
        UINT splitSizeRatio[CSM_SPLIT_COUNT];
    };

    float r = 10.0f;
    float theta = XM_PIDIV4;
    float phi = 0.0f;

    XMFLOAT4 direction;
    XMFLOAT4 color;
    XMMATRIX projectionMatrices[CSM_SPLIT_COUNT];
    XMMATRIX viewProjectionMatrices[CSM_SPLIT_COUNT];
    UINT splitSizeRatio[CSM_SPLIT_COUNT];

    DirectionalLight() {
        for (UINT i = 0; i < CSM_SPLIT_COUNT; ++i) {
            splitSizeRatio[i] = pow(2, i);
        }

        color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        XMFLOAT4 pos = XMFLOAT4(
            r * cosf(theta) * cosf(phi),
            r * sinf(theta),
            r * cosf(theta) * sinf(phi),
            0.0f
        );
        direction = XMFLOAT4(-cosf(theta) * cosf(phi), -sinf(theta), -cosf(theta) * sinf(phi), 0.0f);

        float upTheta = theta + XM_PIDIV2;
        XMMATRIX viewMatrix = XMMatrixLookAtRH(
            XMVectorSet(pos.x, pos.y, pos.z, 0.0f),
            XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
            XMVectorSet(cosf(upTheta) * cosf(phi), sinf(upTheta), cosf(upTheta) * sinf(phi), 0.0f)
        );

        for (UINT i = 0; i < CSM_SPLIT_COUNT; ++i) {
            float ratio = splitSizeRatio[i];
            projectionMatrices[i] = XMMATRIX(
                XMVECTOR{ 2.0f / (baseWidth_ * ratio), 0.0f, 0.0f, 0.0f },
                XMVECTOR{ 0.0f, 2.0f / (baseHeight_ * ratio), 0.0f, 0.0f },
                XMVECTOR{ 0.0f, 0.0f, 1.0f / (baseFar_ - baseNear_), 0.0f },
                XMVECTOR{ 0.0f, 0.0f, -baseNear_ / (baseFar_ - baseNear_), 1.0f }
            );
            viewProjectionMatrices[i] = XMMatrixMultiply(viewMatrix, projectionMatrices[i]);
        }
    };

    DirectionalLightInfo GetInfo() {
        DirectionalLightInfo info;
        info.color = color;
        info.direction = direction;
        info.viewProjectionMatrix = viewProjectionMatrices[0];
        for (int i = 0; i < CSM_SPLIT_COUNT; ++i) {
            info.splitSizeRatio[i] = splitSizeRatio[i];
        }
        return info;
    };

    void Update(const XMFLOAT3& focus) {
        XMFLOAT4 pos = XMFLOAT4(
            r * cosf(theta) * cosf(phi),
            r * sinf(theta),
            r * cosf(theta) * sinf(phi),
            0.0f
        );
        direction = XMFLOAT4(-cosf(theta) * cosf(phi), -sinf(theta), -cosf(theta) * sinf(phi), 0.0f);

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
    float baseWidth_ = 10.0f;
    float baseHeight_ = 10.0f;
    float baseNear_ = 0.01f;
    float baseFar_ = 100.0f;
};