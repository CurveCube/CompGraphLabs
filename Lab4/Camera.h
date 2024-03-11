#pragma once

#include "framework.h"

class Camera {
public:
    Camera();

    void Rotate(float dphi, float dtheta);
    void Zoom(float dr);
    void Move(float dx, float dy, float dz);

    XMMATRIX& GetViewMatrix() {
        return viewMatrix_;
    };

    XMFLOAT3& GetPosition() {
        return position_;
    };

    float GetDistance() {
        return r_;
    };
private:
    XMMATRIX viewMatrix_;
    XMFLOAT3 focus_;
    XMFLOAT3 position_;
    float r_;
    float theta_;
    float phi_;

    void UpdateViewMatrix();
};