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
public:
    Skybox() = default;
    void Init(ID3D11Device* pDevice);
    void SetCubmapTexture(SimpleTextureManager, std::string);



    void Cleanup() {
        VS.reset();
        PS.reset();
        IL.reset();
        geometry.reset();
        texture.reset();
    };

    ~Skybox() = default;
private:
    void GenerateSphere(ID3D11Device* pDevice);


    XMMATRIX worldMatrix;
    float size;
    static const UINT vertexSize = sizeof(SimpleVertex);
    std::shared_ptr<ID3D11VertexShader> VS;
    //std::shared_ptr<ID3D11InputLayout> IL;
    std::unique_ptr<ID3D11InputLayout> IL;
    std::shared_ptr<ID3D11PixelShader> PS;

    std::unique_ptr<Geometry> geometry;
    std::shared_ptr<SimpleTexture> texture;
};