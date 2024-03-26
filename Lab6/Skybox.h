#pragma once

#include "SimpleManager.h"

class Skybox {
private:
    struct Geometry {
    public:
        Geometry(ID3D11Buffer* vertexBuffer, ID3D11Buffer* indexBuffer, UINT numIndices) :
            vertexBuffer_(vertexBuffer), indexBuffer_(indexBuffer), numIndices_(numIndices) {};

        Geometry(Geometry&) = delete;
        Geometry& operator=(Geometry&) = delete;

        ID3D11Buffer* getVertexBuffer() {
            return vertexBuffer_;
        };

        ID3D11Buffer* getIndexBuffer() {
            return indexBuffer_;
        };

        UINT getNumIndices() {
            return numIndices_;
        };

        ~Geometry() {
            if (vertexBuffer_ != nullptr)
                vertexBuffer_->Release();
            if (indexBuffer_ != nullptr)
                indexBuffer_->Release();
        };

    private:
        ID3D11Buffer* vertexBuffer_;
        ID3D11Buffer* indexBuffer_;
        UINT numIndices_;
    };

    struct SimpleVertex {
        XMFLOAT3 pos;
    };

    struct SkyboxWorldMatrixBuffer {
        XMMATRIX worldMatrix;
        XMFLOAT4 size;
    };
public:
    Skybox(const std::shared_ptr<ID3D11Device>& pDevice, const std::shared_ptr <ID3D11DeviceContext>& pDeviseContext) :
        pDevice_(pDevice),
        pDeviseContext_(pDeviseContext),
        size_(1.0f),
        worldMatrix_(DirectX::XMMatrixIdentity())
    {};

    HRESULT Init(std::string textureName);

    void Cleanup() {
        VS_.reset();
        PS_.reset();
        IL_.reset();
        geometry_.reset();
        texture_.reset();
        pDevice_.reset();
        pDeviseContext_.reset();
    };

    ~Skybox() = default;
private:
    HRESULT GenerateSphere();

    XMMATRIX worldMatrix_;
    float size_;
    std::shared_ptr<ID3D11VertexShader> VS_;
    //std::shared_ptr<ID3D11InputLayout> IL_;
    std::unique_ptr<ID3D11InputLayout> IL_;
    std::shared_ptr<ID3D11PixelShader> PS_;

    std::unique_ptr<Geometry> geometry_;
    std::shared_ptr<SimpleTexture> texture_;

    std::shared_ptr<ID3D11Device> pDevice_;
    std::shared_ptr<ID3D11DeviceContext> pDeviseContext_;
};