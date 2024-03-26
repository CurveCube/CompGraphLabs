#include "Skybox.h"
#include <vector>

void Skybox::Init(ID3D11Device* pDevice) {
    worldMatrix = DirectX::XMMatrixIdentity();;
    size = 1.0f;
    GenerateSphere(pDevice);
}

void Skybox::GenerateSphere(ID3D11Device* pDevice) {
    UINT LatLines = 40, LongLines = 40;
    UINT numVertices = ((LatLines - 2) * LongLines) + 2;
    UINT numIndices = (((LatLines - 3) * (LongLines) * 2) + (LongLines * 2)) * 3;

    float phi = 0.0f;
    float theta = 0.0f;

    std::vector<SimpleVertex> vertices(numVertices);

    XMVECTOR currVertPos = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

    vertices[0].pos.x = 0.0f;
    vertices[0].pos.y = 0.0f;
    vertices[0].pos.z = 1.0f;

    for (UINT i = 0; i < LatLines - 2; i++) {
        theta = (i + 1) * (XM_PI / (LatLines - 1));
        XMMATRIX Rotationx = XMMatrixRotationX(theta);
        for (UINT j = 0; j < LongLines; j++) {
            phi = j * (XM_2PI / LongLines);
            XMMATRIX Rotationy = XMMatrixRotationZ(phi);
            currVertPos = XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), (Rotationx * Rotationy));
            currVertPos = XMVector3Normalize(currVertPos);
            vertices[i * (__int64)LongLines + j + 1].pos.x = XMVectorGetX(currVertPos);
            vertices[i * (__int64)LongLines + j + 1].pos.y = XMVectorGetY(currVertPos);
            vertices[i * (__int64)LongLines + j + 1].pos.z = XMVectorGetZ(currVertPos);
        }
    }

    vertices[(__int64)numVertices - 1].pos.x = 0.0f;
    vertices[(__int64)numVertices - 1].pos.y = 0.0f;
    vertices[(__int64)numVertices - 1].pos.z = -1.0f;

    std::vector<UINT> indices(numIndices);

    UINT k = 0;
    for (UINT i = 0; i < LongLines - 1; i++) {
        indices[k] = 0;
        indices[(__int64)k + 2] = i + 1;
        indices[(__int64)k + 1] = i + 2;

        k += 3;
    }
    indices[k] = 0;
    indices[(__int64)k + 2] = LongLines;
    indices[(__int64)k + 1] = 1;

    k += 3;

    for (UINT i = 0; i < LatLines - 3; i++) {
        for (UINT j = 0; j < LongLines - 1; j++) {
            indices[k] = i * LongLines + j + 1;
            indices[(__int64)k + 1] = i * LongLines + j + 2;
            indices[(__int64)k + 2] = (i + 1) * LongLines + j + 1;

            indices[(__int64)k + 3] = (i + 1) * LongLines + j + 1;
            indices[(__int64)k + 4] = i * LongLines + j + 2;
            indices[(__int64)k + 5] = (i + 1) * LongLines + j + 2;

            k += 6;
        }

        indices[k] = (i * LongLines) + LongLines;
        indices[(__int64)k + 1] = (i * LongLines) + 1;
        indices[(__int64)k + 2] = ((i + 1) * LongLines) + LongLines;

        indices[(__int64)k + 3] = ((i + 1) * LongLines) + LongLines;
        indices[(__int64)k + 4] = (i * LongLines) + 1;
        indices[(__int64)k + 5] = ((i + 1) * LongLines) + 1;

        k += 6;
    }

    for (UINT i = 0; i < LongLines - 1; i++) {
        indices[k] = numVertices - 1;
        indices[(__int64)k + 2] = (numVertices - 1) - (i + 1);
        indices[(__int64)k + 1] = (numVertices - 1) - (i + 2);

        k += 3;
    }

    indices[k] = numVertices - 1;
    indices[(__int64)k + 2] = (numVertices - 1) - LongLines;
    indices[(__int64)k + 1] = numVertices - 2;

    /*HRESULT result = pGeometryManager_.loadGeometry(&skyboxVertices[0], sizeof(SimpleVertex) * numVertices,
        &skyboxIndices[0], sizeof(UINT) * numIndices, "skybox");
    if (SUCCEEDED(result)) {
        result = pGeometryManager_.loadGeometry(&sphereVertices[0], sizeof(Vertex) * numVertices,
            &sphereIndices[0], sizeof(UINT) * numIndices, "sphere");
    }*/
    //HRESULT result = pGeometryManager_.loadGeometry(&sphereVertices[0], sizeof(Vertex) * numVertices,
    //    &sphereIndices[0], sizeof(UINT) * numIndices, "sphere");

    ID3D11Buffer* vertexBuffer = nullptr;
    ID3D11Buffer* indexBuffer = nullptr;

    UINT verticesBytes = sizeof(SimpleVertex) * numVertices;
    UINT indicesBytes = sizeof(UINT) * numIndices;

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = verticesBytes;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = vertices.data();
    data.SysMemPitch = verticesBytes;
    data.SysMemSlicePitch = 0;


    HRESULT result = pDevice->CreateBuffer(&desc, &data, &vertexBuffer);

    if (SUCCEEDED(result)) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = indicesBytes;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = indices.data();
        data.SysMemPitch = indicesBytes;
        data.SysMemSlicePitch = 0;

        result = pDevice->CreateBuffer(&desc, &data, &indexBuffer);
    }

    /*if (SUCCEEDED(result)) {
        objects_.emplace(key, std::make_shared<Geometry>(vertexBuffer, indexBuffer, indicesBytes / sizeof(UINT)));
    }*/

    geometry = std::make_unique<Geometry>(vertexBuffer, indexBuffer, numIndices);

    //return result;
}