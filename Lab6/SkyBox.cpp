#include "Skybox.h"
#include "Managers.hpp"
#include <vector>

const D3D11_INPUT_ELEMENT_DESC Skybox::SimpleVertexDesc[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

HRESULT Skybox::Init(const std::wstring& textureName) {
    HRESULT result = S_OK;
    result = GenerateSphere();

    std::shared_ptr<Shader<ID3D11PixelShader>> PS;
    std::shared_ptr<Shader<ID3D11VertexShader>> VS;
    ID3D11InputLayout* inputLayout;

    if (!ManagerStorage::GetInstance().IsInited())
        return E_FAIL;

    if (SUCCEEDED(result)) {
        auto PSShaderManager = ManagerStorage::GetInstance().GetPSManager();
        result = PSShaderManager->GetShader(L"CubeMapPS.hlsl", {}, PS);
    }

    if (SUCCEEDED(result)) {
        auto VSShaderManager = ManagerStorage::GetInstance().GetVSManager();
        result = VSShaderManager->GetShader(L"CubeMapVS.hlsl", {}, VS);
    }

    if (SUCCEEDED(result)) {
        HRESULT result = pDevice_->CreateInputLayout(SimpleVertexDesc,
            sizeof(SimpleVertexDesc) / sizeof(SimpleVertexDesc[0]),
            VS->shaderBuffer_->GetBufferPointer(),
            VS->shaderBuffer_->GetBufferSize(),
            &inputLayout);
    }

    if (SUCCEEDED(result)) {
        PS_ = PS->shader_;
        VS_ = VS->shader_;
        IL_ = std::make_unique<ID3D11InputLayout>(inputLayout);
    }

    if (SUCCEEDED(result)) {
        auto textureManager = ManagerStorage::GetInstance().GetTextureManager();
        result = textureManager->GetTexture(textureName, texture_);
    }

    if (SUCCEEDED(result)) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(SkyboxWorldMatrixBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        SkyboxWorldMatrixBuffer worldMatrixBuffer;
        worldMatrixBuffer.worldMatrix = DirectX::XMMatrixIdentity();
        worldMatrixBuffer.size = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = &worldMatrixBuffer;
        data.SysMemPitch = sizeof(worldMatrixBuffer);
        data.SysMemSlicePitch = 0;

        result = pDevice_->CreateBuffer(&desc, &data, &pWorldMatrixBuffer_);
    }

    return result;
}

void Skybox::UpdateBuffer()
{
    SkyboxWorldMatrixBuffer skyboxWorldMatrixBuffer;
    skyboxWorldMatrixBuffer.worldMatrix = worldMatrix_;
    skyboxWorldMatrixBuffer.size = XMFLOAT4(size_, 0.0f, 0.0f, 0.0f);
    pDeviceContext_->UpdateSubresource(pWorldMatrixBuffer_, 0, nullptr, &skyboxWorldMatrixBuffer, 0, 0);
}

void Skybox::SetSize(float size)
{
    size_ = size;
}

float Skybox::GetSize()
{
    return size_;
}

void Skybox::Render(ID3D11Buffer* pViewMatrixBuffer)
{
    ID3D11ShaderResourceView* resources[] = { texture_->SRV_.get() };
    pDeviceContext_->PSSetShaderResources(0, 1, resources);

    pDeviceContext_->IASetIndexBuffer(geometry_->getIndexBuffer(), DXGI_FORMAT_R32_UINT, 0);
    ID3D11Buffer* vertexBuffers[] = { geometry_->getVertexBuffer() };
    UINT strides[] = { sizeof(SimpleVertex) };
    UINT offsets[] = { 0 };
    pDeviceContext_->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    pDeviceContext_->IASetInputLayout(IL_.get());
    pDeviceContext_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pDeviceContext_->VSSetShader(VS_.get(), nullptr, 0);
    pDeviceContext_->VSSetConstantBuffers(0, 1, &pWorldMatrixBuffer_);
    pDeviceContext_->VSSetConstantBuffers(1, 1, &pViewMatrixBuffer);
    pDeviceContext_->PSSetShader(PS_.get(), nullptr, 0);

    pDeviceContext_->DrawIndexed(geometry_->getNumIndices(), 0, 0);
}



HRESULT Skybox::GenerateSphere() {
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

    HRESULT result = S_OK;
    result = pDevice_->CreateBuffer(&desc, &data, &vertexBuffer);

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

        result = pDevice_->CreateBuffer(&desc, &data, &indexBuffer);
    }

    geometry_ = std::make_unique<Geometry>(vertexBuffer, indexBuffer, numIndices);

    return result;
}