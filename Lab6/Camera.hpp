#pragma once

#include "framework.h"


class Camera {
public:
    Camera(const XMFLOAT3& focus, const XMFLOAT3& position, float fov, float width, float height, float nearPlane, float farPlane) { // near and far will be swapped
        SetViewParameters(focus, position);
        SetProjectionParameters(fov, width, height, nearPlane, farPlane);
    };

    void Rotate(float dphi, float dtheta) {
        phi_ -= dphi;
        theta_ -= dtheta;
        theta_ = min(max(theta_, -XM_PIDIV2), XM_PIDIV2);
        focus_ = XMFLOAT3(cosf(theta_) * cosf(phi_) * r_ + position_.x,
            sinf(theta_) * r_ + position_.y,
            cosf(theta_) * sinf(phi_) * r_ + position_.z);

        UpdateViewMatrix();
    };

    void Zoom(float dr) {
        r_ += dr;
        if (r_ < 1.0f) {
            r_ = 1.0f;
        }
        position_ = XMFLOAT3(focus_.x - cosf(theta_) * cosf(phi_) * r_,
            focus_.y - sinf(theta_) * r_,
            focus_.z - cosf(theta_) * sinf(phi_) * r_);

        UpdateViewMatrix();
    };

    void Move(float dx, float dy, float dz) {
        focus_ = XMFLOAT3(focus_.x + dx * cosf(phi_) - dz * sinf(phi_), focus_.y + dy, focus_.z + dx * sinf(phi_) + dz * cosf(phi_));
        position_ = XMFLOAT3(position_.x + dx * cosf(phi_) - dz * sinf(phi_), position_.y + dy, position_.z + dx * sinf(phi_) + dz * cosf(phi_));

        UpdateViewMatrix();
    };

    void SetViewParameters(const XMFLOAT3& focus, const XMFLOAT3& position) {
        focus_ = focus;
        position_ = position;

        float dx = focus.x - position.x;
        float dy = focus.y - position.y;
        float dz = focus.z - position.z;
        r_ = sqrtf(dx * dx + dy * dy + dz * dz);

        if (r_ == 0.0f) {
            theta_ = 0.0f;
        }
        else {
            theta_ = asinf(dy / r_);
        }
        if (dx == 0.0f) {
            if (dz == 0.0f) {
                phi_ = 0.0f;
            }
            else {
                phi_ = XM_PIDIV2;
            }
        }
        else {
            phi_ = atanf(dz / dx);
        }
        phi_ = dz >= 0 ? phi_ : -phi_;

        UpdateViewMatrix();
    };

    void Resize(float width, float height) {
        SetProjectionParameters(fov_, width, height, near_, far_);
    };

    void SetProjectionParameters(float fov, float width, float height, float nearPlane, float farPlane) { // near and far will be swapped
        fov_ = fov;
        near_ = nearPlane;
        far_ = farPlane;
        projectionMatrix_ = XMMatrixPerspectiveFovRH(fov_, width / height, far_, near_);
    };

    const XMMATRIX& GetViewMatrix() const {
        return viewMatrix_;
    };

    const XMMATRIX& GetProjectionMatrix() const {
        return projectionMatrix_;
    };

    XMMATRIX GetViewProjectionMatrix() const {
        return XMMatrixMultiply(viewMatrix_, projectionMatrix_);
    };

    const XMFLOAT3& GetFocusPosition() const {
        return focus_;
    };

    const XMFLOAT3& GetPosition() const {
        return position_;
    };

    float GetFov() const {
        return fov_;
    };

    float GetNearPlane() const {
        return near_;
    };

    float GetFarPlane() const {
        return far_;
    };

    float GetDistanceToFocus() const {
        return r_;
    };

    ~Camera() = default;

private:
    void UpdateViewMatrix() {
        float upTheta = theta_ + XM_PIDIV2;
        XMFLOAT3 up = XMFLOAT3(cosf(upTheta) * cosf(phi_), sinf(upTheta), cosf(upTheta) * sinf(phi_));

        viewMatrix_ = XMMatrixLookAtRH(
            XMVectorSet(position_.x, position_.y, position_.z, 0.0f),
            XMVectorSet(focus_.x, focus_.y, focus_.z, 0.0f),
            XMVectorSet(up.x, up.y, up.z, 0.0f)
        );
    };

    XMMATRIX viewMatrix_;
    XMMATRIX projectionMatrix_;
    XMFLOAT3 focus_;
    XMFLOAT3 position_;
    float r_;
    float theta_;
    float phi_;
    float fov_;
    float near_;
    float far_;
};