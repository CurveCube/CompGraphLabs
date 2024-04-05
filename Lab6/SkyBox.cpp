#include "Skybox.h"

const std::vector<D3D11_INPUT_ELEMENT_DESC> Skybox::VertexDesc = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

HRESULT Skybox::Init(const std::shared_ptr<Device>& device, const std::shared_ptr<ManagerStorage>& managerStorage,
    const std::shared_ptr<Camera>& camera, const std::shared_ptr<ID3D11ShaderResourceView>& cubemapTexture) {
    if (!managerStorage->IsInit() || !device->IsInit()) {
        return E_FAIL;
    }

    device_ = device;
    managerStorage_ = managerStorage;
    camera_ = camera;

    SetCubeMapTexture(cubemapTexture);
    HRESULT result = GenerateSphere();
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetVSManager()->LoadShader(VS_, L"shaders/cubemapVS.hlsl", {}, VertexDesc);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(PS_, L"shaders/cubemapPS.hlsl");
    }
    if (SUCCEEDED(result)) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(WorldMatrixBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        WorldMatrixBuffer worldMatrixBuffer;
        worldMatrixBuffer.worldMatrix = worldMatrix_;
        worldMatrixBuffer.size = XMFLOAT4(size_, 0.0f, 0.0f, 0.0f);

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = &worldMatrixBuffer;
        data.SysMemPitch = sizeof(worldMatrixBuffer);
        data.SysMemSlicePitch = 0;

        result = device_->GetDevice()->CreateBuffer(&desc, &data, &worldMatrixBuffer_);
    }
    if (SUCCEEDED(result)) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(ViewMatrixBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        ViewMatrixBuffer viewMatrixBuffer;
        XMFLOAT3 pos = camera_->GetPosition();
        viewMatrixBuffer.viewProjectionMatrix = camera_->GetViewProjectionMatrix();
        viewMatrixBuffer.cameraPos = XMFLOAT4(pos.x, pos.y, pos.z, 1.0f);

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = &viewMatrixBuffer;
        data.SysMemPitch = sizeof(viewMatrixBuffer);
        data.SysMemSlicePitch = 0;

        result = device_->GetDevice()->CreateBuffer(&desc, &data, &viewMatrixBuffer_);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetStateManager()->CreateDepthStencilState(dsState_, D3D11_COMPARISON_GREATER_EQUAL, D3D11_DEPTH_WRITE_MASK_ZERO);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetStateManager()->CreateSamplerState(sampler_, D3D11_FILTER_ANISOTROPIC);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetStateManager()->CreateRasterizerState(rasterizerState_);
    }

    return result;
}

bool Skybox::Resize(UINT width, UINT height) {
    if (!IsInit()) {
        return false;
    }

    float n = camera_->GetNearPlane();
    float fov = camera_->GetFov();
    float halfW = tanf(fov / 2) * n;
    float halfH = width / float(height) * halfW;
    size_ = sqrtf(n * n + halfH * halfH + halfW * halfW) * 1.1f;

    return true;
}

bool Skybox::Render() {
    if (!IsInit()) {
        return false;
    }

    WorldMatrixBuffer worldMatrixBuffer;
    worldMatrixBuffer.worldMatrix = worldMatrix_;
    worldMatrixBuffer.size = XMFLOAT4(size_, 0.0f, 0.0f, 0.0f);
    device_->GetDeviceContext()->UpdateSubresource(worldMatrixBuffer_, 0, nullptr, &worldMatrixBuffer, 0, 0);

    ViewMatrixBuffer viewMatrixBuffer;
    XMFLOAT3 pos = camera_->GetPosition();
    viewMatrixBuffer.viewProjectionMatrix = camera_->GetViewProjectionMatrix();
    viewMatrixBuffer.cameraPos = XMFLOAT4(pos.x, pos.y, pos.z, 1.0f);
    device_->GetDeviceContext()->UpdateSubresource(viewMatrixBuffer_, 0, nullptr, &viewMatrixBuffer, 0, 0);

    ID3D11SamplerState* samplers[] = { sampler_.get() };
    device_->GetDeviceContext()->PSSetSamplers(0, 1, samplers);

    ID3D11ShaderResourceView* resources[] = { cubemapTexture_.get() };
    device_->GetDeviceContext()->PSSetShaderResources(0, 1, resources);
    device_->GetDeviceContext()->OMSetDepthStencilState(dsState_.get(), 0);
    device_->GetDeviceContext()->RSSetState(rasterizerState_.get());

    device_->GetDeviceContext()->IASetIndexBuffer(indexBuffer_, DXGI_FORMAT_R32_UINT, 0);
    ID3D11Buffer* vertexBuffers[] = { vertexBuffer_ };
    UINT strides[] = { sizeof(Vertex) };
    UINT offsets[] = { 0 };
    device_->GetDeviceContext()->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    device_->GetDeviceContext()->IASetInputLayout(VS_->GetInputLayout().get());
    device_->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    device_->GetDeviceContext()->VSSetShader(VS_->GetShader().get(), nullptr, 0);
    device_->GetDeviceContext()->VSSetConstantBuffers(0, 1, &worldMatrixBuffer_);
    device_->GetDeviceContext()->VSSetConstantBuffers(1, 1, &viewMatrixBuffer_);
    device_->GetDeviceContext()->PSSetShader(PS_->GetShader().get(), nullptr, 0);
    device_->GetDeviceContext()->DrawIndexed(numIndices_, 0, 0);

    return true;
}

HRESULT Skybox::GenerateSphere() {
    UINT LatLines = 40, LongLines = 40;
    UINT numVertices = ((LatLines - 2) * LongLines) + 2;
    numIndices_ = (((LatLines - 3) * (LongLines) * 2) + (LongLines * 2)) * 3;

    float phi = 0.0f;
    float theta = 0.0f;

    std::vector<Vertex> vertices(numVertices);

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

    std::vector<UINT> indices(numIndices_);

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

    UINT verticesBytes = sizeof(Vertex) * numVertices;
    UINT indicesBytes = sizeof(UINT) * numIndices_;

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
    result = device_->GetDevice()->CreateBuffer(&desc, &data, &vertexBuffer_);

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

        result = device_->GetDevice()->CreateBuffer(&desc, &data, &indexBuffer_);
    }

    return result;
}

void Skybox::Cleanup() {
    device_.reset();
    managerStorage_.reset();
    camera_.reset();
    VS_.reset();
    PS_.reset();
    cubemapTexture_.reset();
    dsState_.reset();
    sampler_.reset();
    rasterizerState_.reset();

    SAFE_RELEASE(vertexBuffer_);
    SAFE_RELEASE(indexBuffer_);
    SAFE_RELEASE(worldMatrixBuffer_);
    SAFE_RELEASE(viewMatrixBuffer_);
};