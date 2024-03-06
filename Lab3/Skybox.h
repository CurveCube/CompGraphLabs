#pragma once

#include "SimpleManager.h"


struct SimpleVertex {
    XMFLOAT3 pos;
};


struct Skybox {
    XMMATRIX worldMatrix;
    float size;
    static const UINT vertexSize = sizeof(SimpleVertex);
    std::shared_ptr<ID3D11VertexShader> VS;
    std::shared_ptr<ID3D11InputLayout> IL;
    std::shared_ptr<ID3D11PixelShader> PS;
    std::shared_ptr<Geometry> geometry;
    std::shared_ptr<SimpleTexture> texture;

    void Cleanup() {
        VS.reset();
        PS.reset();
        IL.reset();
        geometry.reset();
        texture.reset();
    };

    ~Skybox() = default;
};