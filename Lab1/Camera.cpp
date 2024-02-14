#include "Camera.h"

Camera::Camera() {
    center_ = XMFLOAT3(0.0f, 0.0f, 0.0f);
    r_ = 5.0f;
    theta_ = XM_PIDIV4;
    phi_ = -XM_PIDIV4;
    UpdateViewMatrix();
}

void Camera::ChangePos(float dphi, float dtheta, float dr) {
    phi_ -= dphi;
    theta_ += dtheta;
    theta_ = min(max(theta_, -XM_PIDIV2), XM_PIDIV2);
    r_ += dr;
    if (r_ < 2.0f) {
        r_ = 2.0f;
    }
    UpdateViewMatrix();
}

void Camera::UpdateViewMatrix() {
    XMFLOAT3 pos = XMFLOAT3(cosf(theta_) * cosf(phi_), sinf(theta_), cosf(theta_) * sinf(phi_));
    pos.x = pos.x * r_ + center_.x;
    pos.y = pos.y * r_ + center_.y;
    pos.z = pos.z * r_ + center_.z;
    float upTheta = theta_ + XM_PIDIV2;
    XMFLOAT3 up = XMFLOAT3(cosf(upTheta) * cosf(phi_), sinf(upTheta), cosf(upTheta) * sinf(phi_));

    viewMatrix_ = DirectX::XMMatrixLookAtLH(
        DirectX::XMVectorSet(pos.x, pos.y, pos.z, 0.0f),
        DirectX::XMVectorSet(center_.x, center_.y, center_.z, 0.0f),
        DirectX::XMVectorSet(up.x, up.y, up.z, 0.0f)
    );
}