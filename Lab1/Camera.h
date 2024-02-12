#pragma once

#include "framework.h"

class Camera {
public:
    Camera();

    void ChangePos(float dphi, float dtheta, float dr);

    XMMATRIX& GetViewMatrix() {
        return viewMatrix_;
    };
private:
    XMMATRIX viewMatrix_;
    XMFLOAT3 center_;
    float r_;
    float theta_;
    float phi_;

    void UpdateViewMatrix();
};