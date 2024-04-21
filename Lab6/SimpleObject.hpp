#pragma once

#include "ManagerStorage.hpp"
#include "Camera.hpp"
#include "Light.hpp"


class SimpleObject {
    struct WorldMatrixBuffer {
        XMMATRIX worldMatrix;
        XMFLOAT3 color;
        float roughness;
        float metalness;
    };

    struct ViewMatrixBuffer {
        XMMATRIX viewProjectionMatrix;
        XMFLOAT4 cameraPos;
        XMINT4 lightParams;
        SpotLight lights[MAX_LIGHT];
    };

    struct Vertex {
        XMFLOAT3 pos;
        XMFLOAT3 norm;
    };

public:
    enum Mode {
        DEFAULT,
        FRESNEL,
        NDF,
        GEOMETRY
    };

    XMFLOAT3 color_;
    float roughness_ = 0.0f;
    float metalness_ = 1.0f;

    SimpleObject() : worldMatrix_(XMMatrixIdentity()), color_(XMFLOAT3(1.0f, 0.71f, 0.29f)) {};

    HRESULT Init(const std::shared_ptr<Device>& device, const std::shared_ptr<ManagerStorage>& managerStorage,
        const std::shared_ptr<Camera>& camera) {
        if (!managerStorage->IsInit() || !device->IsInit()) {
            return E_FAIL;
        }

        device_ = device;
        managerStorage_ = managerStorage;
        camera_ = camera;

        HRESULT result = GenerateSphere();
        if (SUCCEEDED(result)) {
            result = LoadShaders();
        }
        if (SUCCEEDED(result)) {
            D3D11_BUFFER_DESC desc = {};
            desc.ByteWidth = sizeof(WorldMatrixBuffer);
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            desc.CPUAccessFlags = 0;
            desc.MiscFlags = 0;
            desc.StructureByteStride = 0;
            result = device_->GetDevice()->CreateBuffer(&desc, nullptr, &worldMatrixBuffer_);
        }
        if (SUCCEEDED(result)) {
            D3D11_BUFFER_DESC desc = {};
            desc.ByteWidth = sizeof(ViewMatrixBuffer);
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            desc.MiscFlags = 0;
            desc.StructureByteStride = 0;

            result = device_->GetDevice()->CreateBuffer(&desc, nullptr, &viewMatrixBuffer_);
        }
        if (SUCCEEDED(result)) {
            result = managerStorage_->GetStateManager()->CreateDepthStencilState(dsState_);
        }
        if (SUCCEEDED(result)) {
            result = managerStorage_->GetStateManager()->CreateRasterizerState(rasterizerState_);
        }
        if (SUCCEEDED(result)) {
            result = managerStorage_->GetStateManager()->CreateSamplerState(samplerAvg_, D3D11_FILTER_MIN_MAG_MIP_LINEAR,
                D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP);
        }

        return result;
    };

    bool Render(const std::shared_ptr<ID3D11ShaderResourceView>& irradianceMap, const std::shared_ptr<ID3D11ShaderResourceView>& prefilteredMap,
        const std::shared_ptr<ID3D11ShaderResourceView>& BRDF, const std::vector<SpotLight>& lights) const {
        if (!IsInit()) {
            return false;
        }

        WorldMatrixBuffer worldMatrixBuffer;
        worldMatrixBuffer.worldMatrix = worldMatrix_;
        worldMatrixBuffer.color = color_;
        worldMatrixBuffer.roughness = roughness_;
        worldMatrixBuffer.metalness = metalness_;
        device_->GetDeviceContext()->UpdateSubresource(worldMatrixBuffer_, 0, nullptr, &worldMatrixBuffer, 0, 0);

        XMFLOAT3 cameraPos = camera_->GetPosition();
        D3D11_MAPPED_SUBRESOURCE subresource;
        HRESULT result = device_->GetDeviceContext()->Map(viewMatrixBuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &subresource);
        if (FAILED(result)) {
            return false;
        }

        ViewMatrixBuffer& sceneBuffer = *reinterpret_cast<ViewMatrixBuffer*>(subresource.pData);
        sceneBuffer.viewProjectionMatrix = camera_->GetViewProjectionMatrix();
        sceneBuffer.cameraPos = XMFLOAT4(cameraPos.x, cameraPos.y, cameraPos.z, 1.0f);
        sceneBuffer.lightParams = XMINT4(int(lights.size()), 0, 0, 0);
        for (int i = 0; i < lights.size(); i++) {
            sceneBuffer.lights[i].pos = lights[i].pos;
            sceneBuffer.lights[i].color = lights[i].color;
        }
        device_->GetDeviceContext()->Unmap(viewMatrixBuffer_, 0);

        ID3D11SamplerState* samplers[] = { samplerAvg_.get() };
        device_->GetDeviceContext()->PSSetSamplers(0, 1, samplers);

        ID3D11ShaderResourceView* resources[] = { irradianceMap.get(), prefilteredMap.get(), BRDF.get() };
        device_->GetDeviceContext()->PSSetShaderResources(0, 3, resources);
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
        device_->GetDeviceContext()->PSSetConstantBuffers(0, 1, &worldMatrixBuffer_);
        device_->GetDeviceContext()->PSSetConstantBuffers(1, 1, &viewMatrixBuffer_);
        switch (currentMode_) {
        case DEFAULT:
            device_->GetDeviceContext()->PSSetShader(PS_->GetShader().get(), nullptr, 0);
            break;
        case FRESNEL:
            device_->GetDeviceContext()->PSSetShader(PSFresnel_->GetShader().get(), nullptr, 0);
            break;
        case NDF:
            device_->GetDeviceContext()->PSSetShader(PSNDF_->GetShader().get(), nullptr, 0);
            break;
        case GEOMETRY:
            device_->GetDeviceContext()->PSSetShader(PSGeometry_->GetShader().get(), nullptr, 0);
            break;
        default:
            break;
        }
        device_->GetDeviceContext()->DrawIndexed(numIndices_, 0, 0);

        return true;
    };

    void SetMode(Mode mode) {
        currentMode_ = mode;
    };

    void Cleanup() {
        device_.reset();
        managerStorage_.reset();
        camera_.reset();
        VS_.reset();
        PS_.reset();
        PSFresnel_.reset();
        PSNDF_.reset();
        PSGeometry_.reset();
        dsState_.reset();
        samplerAvg_.reset();
        rasterizerState_.reset();

        SAFE_RELEASE(vertexBuffer_);
        SAFE_RELEASE(indexBuffer_);
        SAFE_RELEASE(worldMatrixBuffer_);
        SAFE_RELEASE(viewMatrixBuffer_);
    };

    bool IsInit() const {
        return !!samplerAvg_;
    };

    ~SimpleObject() {
        Cleanup();
    };

private:
    HRESULT GenerateSphere() {
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
        vertices[0].norm.x = 0.0f;
        vertices[0].norm.y = 0.0f;
        vertices[0].norm.z = 1.0f;

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
                vertices[i * (__int64)LongLines + j + 1].norm.x = XMVectorGetX(currVertPos);
                vertices[i * (__int64)LongLines + j + 1].norm.y = XMVectorGetY(currVertPos);
                vertices[i * (__int64)LongLines + j + 1].norm.z = XMVectorGetZ(currVertPos);
            }
        }

        vertices[(__int64)numVertices - 1].pos.x = 0.0f;
        vertices[(__int64)numVertices - 1].pos.y = 0.0f;
        vertices[(__int64)numVertices - 1].pos.z = -1.0f;
        vertices[(__int64)numVertices - 1].norm.x = 0.0f;
        vertices[(__int64)numVertices - 1].norm.y = 0.0f;
        vertices[(__int64)numVertices - 1].norm.z = -1.0f;

        std::vector<UINT> indices(numIndices_);

        UINT k = 0;
        for (UINT i = 0; i < LongLines - 1; i++) {
            indices[k] = i + 1;
            indices[(__int64)k + 2] = 0;
            indices[(__int64)k + 1] = i + 2;
            k += 3;
        }

        indices[k] = LongLines;
        indices[(__int64)k + 2] = 0;
        indices[(__int64)k + 1] = 1;
        k += 3;

        for (UINT i = 0; i < LatLines - 3; i++) {
            for (UINT j = 0; j < LongLines - 1; j++) {
                indices[(__int64)k + 2] = i * LongLines + j + 1;
                indices[(__int64)k + 1] = i * LongLines + j + 2;
                indices[k] = (i + 1) * LongLines + j + 1;

                indices[(__int64)k + 5] = (i + 1) * LongLines + j + 1;
                indices[(__int64)k + 4] = i * LongLines + j + 2;
                indices[(__int64)k + 3] = (i + 1) * LongLines + j + 2;

                k += 6;
            }

            indices[(__int64)k + 2] = (i * LongLines) + LongLines;
            indices[(__int64)k + 1] = (i * LongLines) + 1;
            indices[k] = ((i + 1) * LongLines) + LongLines;

            indices[(__int64)k + 5] = ((i + 1) * LongLines) + LongLines;
            indices[(__int64)k + 4] = (i * LongLines) + 1;
            indices[(__int64)k + 3] = ((i + 1) * LongLines) + 1;

            k += 6;
        }

        for (UINT i = 0; i < LongLines - 1; i++) {
            indices[(__int64)k + 2] = numVertices - 1;
            indices[k] = (numVertices - 1) - (i + 1);
            indices[(__int64)k + 1] = (numVertices - 1) - (i + 2);
            k += 3;
        }

        indices[(__int64)k + 2] = numVertices - 1;
        indices[k] = (numVertices - 1) - LongLines;
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
    };

    HRESULT LoadShaders() {
        HRESULT result = managerStorage_->GetVSManager()->LoadShader(VS_, L"shaders/simpleVS.hlsl", {}, VertexDesc);
        if (SUCCEEDED(result)) {
            std::vector<std::string> shaderMacros = { "DEFAULT" };
            result = managerStorage_->GetPSManager()->LoadShader(PS_, L"shaders/simplePS.hlsl", shaderMacros);
        }
        if (SUCCEEDED(result)) {
            std::vector<std::string> shaderMacros = { "FRESNEL" };
            result = managerStorage_->GetPSManager()->LoadShader(PSFresnel_, L"shaders/simplePS.hlsl", shaderMacros);
        }
        if (SUCCEEDED(result)) {
            std::vector<std::string> shaderMacros = { "ND" };
            result = managerStorage_->GetPSManager()->LoadShader(PSNDF_, L"shaders/simplePS.hlsl", shaderMacros);
        }
        if (SUCCEEDED(result)) {
            std::vector<std::string> shaderMacros = { "GEOMETRY" };
            result = managerStorage_->GetPSManager()->LoadShader(PSGeometry_, L"shaders/simplePS.hlsl", shaderMacros);
        }
        return result;
    };

    std::vector<D3D11_INPUT_ELEMENT_DESC> VertexDesc = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    std::shared_ptr<Device> device_; // provided externally <-
    std::shared_ptr<ManagerStorage> managerStorage_; // provided externally <-
    std::shared_ptr<Camera> camera_; // provided externally <-

    ID3D11Buffer* vertexBuffer_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* indexBuffer_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* worldMatrixBuffer_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* viewMatrixBuffer_ = nullptr; // always remains only inside the class #

    std::shared_ptr<VertexShader> VS_; // provided externally <-
    std::shared_ptr<PixelShader> PS_; // provided externally <-
    std::shared_ptr<PixelShader> PSFresnel_; // provided externally <-
    std::shared_ptr<PixelShader> PSNDF_; // provided externally <-
    std::shared_ptr<PixelShader> PSGeometry_; // provided externally <-
    std::shared_ptr<ID3D11DepthStencilState> dsState_; // provided externally <-
    std::shared_ptr<ID3D11SamplerState> samplerAvg_; // provided externally <-
    std::shared_ptr<ID3D11RasterizerState> rasterizerState_; // provided externally <-

    UINT numIndices_ = 0;
    XMMATRIX worldMatrix_;
    Mode currentMode_ = DEFAULT;
};