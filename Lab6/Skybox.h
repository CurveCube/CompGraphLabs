#pragma once

class Texture;

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
    Skybox(const std::shared_ptr<ID3D11Device>& pDevice, const std::shared_ptr <ID3D11DeviceContext>& pDeviceContext) :
        worldMatrix_(DirectX::XMMatrixIdentity()),
        size_(1.0f),
        pDevice_(pDevice),
        pDeviceContext_(pDeviceContext)
    {};

    HRESULT Init(const std::wstring& textureName);

    void UpdateBuffer();
    void SetSize(float size);
    float GetSize();
    void Render(ID3D11Buffer* pViewMatrixBuffer);

    void Cleanup() {
        VS_.reset();
        PS_.reset();
        IL_.reset();
        geometry_.reset();
        texture_.reset();
        pDevice_.reset();
        pDeviceContext_.reset();
        if (pWorldMatrixBuffer_ != nullptr) {
            pWorldMatrixBuffer_->Release();
        }
    };

    ~Skybox() = default;
private:
    HRESULT GenerateSphere();

    XMMATRIX worldMatrix_;
    float size_;
    ID3D11Buffer* pWorldMatrixBuffer_ = nullptr;

    std::shared_ptr<ID3D11VertexShader> VS_;
    //std::shared_ptr<ID3D11InputLayout> IL_;
    std::unique_ptr<ID3D11InputLayout> IL_;
    std::shared_ptr<ID3D11PixelShader> PS_;

    std::unique_ptr<Geometry> geometry_;
    std::shared_ptr<Texture> texture_;

    std::shared_ptr<ID3D11Device> pDevice_;
    std::shared_ptr<ID3D11DeviceContext> pDeviceContext_;

    static const D3D11_INPUT_ELEMENT_DESC SimpleVertexDesc[];
};