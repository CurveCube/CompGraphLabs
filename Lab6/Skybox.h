#pragma once

#include "ManagerStorage.hpp"
#include "Camera.hpp"


class Skybox {
    struct Vertex {
        XMFLOAT3 pos;
    };

    struct WorldMatrixBuffer {
        XMMATRIX worldMatrix;
        XMFLOAT4 size;
    };

    struct ViewMatrixBuffer {
        XMMATRIX viewProjectionMatrix;
        XMFLOAT4 cameraPos;
    };

public:
    Skybox() : worldMatrix_(XMMatrixIdentity()) {};

    HRESULT Init(const std::shared_ptr<Device>& device, const std::shared_ptr<ManagerStorage>& managerStorage, const std::shared_ptr<Camera>& camera,
        const std::shared_ptr<ID3D11ShaderResourceView>& cubemapTexture);
    bool Resize(UINT width, UINT height);
    bool Render();
    void Cleanup();

    void SetCubeMapTexture(const std::shared_ptr<ID3D11ShaderResourceView>& cubemapTexture) {
        cubemapTexture_ = cubemapTexture;
    };

    bool IsInit() const {
        return !!vertexBuffer_ && !!cubemapTexture_;
    };

    ~Skybox() {
        Cleanup();
    };

private:
    HRESULT GenerateSphere();

    static const std::vector<D3D11_INPUT_ELEMENT_DESC> VertexDesc;

    std::shared_ptr<Device> device_; // provided externally <-
    std::shared_ptr<ManagerStorage> managerStorage_; // provided externally <-
    std::shared_ptr<Camera> camera_; // provided externally <-

    std::shared_ptr<ID3D11ShaderResourceView> cubemapTexture_; // provided externally <-

    ID3D11Buffer* vertexBuffer_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* indexBuffer_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* worldMatrixBuffer_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* viewMatrixBuffer_ = nullptr; // always remains only inside the class #

    std::shared_ptr<VertexShader> VS_; // provided externally <-
    std::shared_ptr<PixelShader> PS_; // provided externally <-
    std::shared_ptr<ID3D11DepthStencilState> dsState_; // provided externally <-

    UINT numIndices_ = 0;
    XMMATRIX worldMatrix_;
    float size_ = 1.0f;
};