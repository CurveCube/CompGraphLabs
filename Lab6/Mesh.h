#pragma once

#include "framework.h"
#include "SimpleManager.h"
#include <string>
#include <memory>
#include <vector>
#include "Material.h"

struct Geometry {
    ID3D11Buffer* vertexBuffer_;
    ID3D11Buffer* colorBuffer_;
    ID3D11Buffer* normalsBufferView;
    ID3D11Buffer* tangentsBufferView;
    ID3D11Buffer* texCoord0BufferView;
    ID3D11Buffer* texCoord1BufferView;
    ID3D11Buffer* indexBuffer_;
    D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};

//Primitive, accessor
struct Split {
    Geometry geometry;
    unsigned int MaterailId;
};

//Mesh
class Object {
public:
    unsigned int getId();
private:
    std::vector<Split> splits;
    unsigned int m_id;
    //std::string name;
};