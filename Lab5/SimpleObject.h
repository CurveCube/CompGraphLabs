#pragma once

#include "framework.h"
#include "SimpleManager.h"
#include <string>
#include <memory>


struct Vertex {
    XMFLOAT3 pos;
    XMFLOAT3 norm;
};


// Описание простого объекта.
template<typename VertexType>
struct SimpleObject {
    XMMATRIX worldMatrix;
    XMFLOAT3 color;
    float roughness;
    float metalness;
    static const UINT vertexSize;
    std::shared_ptr<ID3D11VertexShader> VS;
    std::shared_ptr<ID3D11InputLayout> IL;
    std::shared_ptr<ID3D11PixelShader> PS;
    std::shared_ptr<Geometry> geometry;
    std::shared_ptr<SimpleTexture> irradianceMap;

    void Cleanup() {
        VS.reset();
        PS.reset();
        IL.reset();
        geometry.reset();
        irradianceMap.reset();
    };

    ~SimpleObject() = default;
};


template<typename VertexType>
const UINT SimpleObject<VertexType>::vertexSize = sizeof(VertexType);