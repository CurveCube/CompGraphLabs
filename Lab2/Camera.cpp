#include "Camera.h"

Camera::Camera() {
    focus_ = XMFLOAT3(0.0f, 0.0f, 0.0f);
    r_ = 5.0f;
    theta_ = -XM_PIDIV4;
    phi_ = XM_PIDIV4;
    position_ = XMFLOAT3(focus_.x - cosf(theta_) * cosf(phi_) * r_,
        focus_.y - sinf(theta_) * r_,
        focus_.z - cosf(theta_) * sinf(phi_) * r_);

    UpdateViewMatrix();
}

void Camera::Rotate(float dphi, float dtheta) {
    phi_ -= dphi;
    theta_ -= dtheta;
    theta_ = min(max(theta_, -XM_PIDIV2), XM_PIDIV2);
    focus_ = XMFLOAT3(cosf(theta_) * cosf(phi_) * r_ + position_.x,
        sinf(theta_) * r_ + position_.y,
        cosf(theta_) * sinf(phi_) * r_ + position_.z);

    UpdateViewMatrix();
}

void Camera::Zoom(float dr) {
    r_ += dr;
    if (r_ < 1.0f) {
        r_ = 1.0f;
    }
    position_ = XMFLOAT3(focus_.x - cosf(theta_) * cosf(phi_) * r_,
        focus_.y - sinf(theta_) * r_,
        focus_.z - cosf(theta_) * sinf(phi_) * r_);

    UpdateViewMatrix();
}

void Camera::Move(float dx, float dy, float dz) {
    focus_ = XMFLOAT3(focus_.x + dx * cosf(phi_) - dz * sinf(phi_), focus_.y + dy, focus_.z + dx * sinf(phi_) + dz * cosf(phi_));
    position_ = XMFLOAT3(position_.x + dx * cosf(phi_) - dz * sinf(phi_), position_.y + dy, position_.z + dx * sinf(phi_) + dz * cosf(phi_));

    UpdateViewMatrix();
}

void Camera::UpdateViewMatrix() {
    float upTheta = theta_ + XM_PIDIV2;
    XMFLOAT3 up = XMFLOAT3(cosf(upTheta) * cosf(phi_), sinf(upTheta), cosf(upTheta) * sinf(phi_));

    viewMatrix_ = DirectX::XMMatrixLookAtLH(
        DirectX::XMVectorSet(position_.x, position_.y, position_.z, 0.0f),
        DirectX::XMVectorSet(focus_.x, focus_.y, focus_.z, 0.0f),
        DirectX::XMVectorSet(up.x, up.y, up.z, 0.0f)
    );
}