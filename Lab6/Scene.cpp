#include "Scene.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tinygltf/tiny_gltf.h"
#undef TINYGLTF_IMPLEMENTATION
#undef STB_IMAGE_WRITE_IMPLEMENTATION

SceneManager::SceneManager() {
    viewport_.TopLeftX = 0;
    viewport_.TopLeftY = 0;
    viewport_.Width = shadowMapSize;
    viewport_.Height = shadowMapSize;
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    renderTargetViewport_.TopLeftX = 0;
    renderTargetViewport_.TopLeftY = 0;
    renderTargetViewport_.Width = 1.0f;
    renderTargetViewport_.Height = 1.0f;
    renderTargetViewport_.MinDepth = 0.0f;
    renderTargetViewport_.MaxDepth = 1.0f;
}

HRESULT SceneManager::Init(const std::shared_ptr<Device>& device, const std::shared_ptr<ManagerStorage>& managerStorage,
    const std::shared_ptr<Camera>& camera, const std::shared_ptr<DirectionalLight>& directionalLight, int width, int height,
    const std::shared_ptr<ID3DUserDefinedAnnotation>& annotation) {
    if (!managerStorage->IsInit() || !device->IsInit()) {
        return E_FAIL;
    }
    if (isInit_) {
        Cleanup();
    }

    device_ = device;
    managerStorage_ = managerStorage;
    camera_ = camera;
    directionalLight_ = directionalLight;
    annotation_ = annotation;

    width_ = width;
    height_ = height;

    HRESULT result = S_OK;
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetVSManager()->LoadShader(mappingVS_, L"shaders/mappingVS.hlsl");
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetStateManager()->CreateSamplerState(samplerAvg_, D3D11_FILTER_MIN_MAG_MIP_LINEAR,
            D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetStateManager()->CreateSamplerState(generalSampler_, D3D11_FILTER_ANISOTROPIC,
            D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetStateManager()->CreateRasterizerState(generalRasterizerState_);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetStateManager()->CreateBlendState(generalBlendState_, D3D11_BLEND_ONE, D3D11_BLEND_ONE);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetStateManager()->CreateDepthStencilState(generalDepthStencilState_, D3D11_COMPARISON_ALWAYS, D3D11_DEPTH_WRITE_MASK_ZERO);
    }
    if (SUCCEEDED(result)) {
        result = CreateTexture(normals_, width_, height_);
    }
    if (SUCCEEDED(result)) {
        result = CreateTexture(features_, width_, height_);
    }
    if (SUCCEEDED(result)) {
        result = CreateTexture(color_, width_, height_);
    }
    if (SUCCEEDED(result)) {
        result = CreateDepthStencilView(width_, height_, depth_);
    }
    if (SUCCEEDED(result)) {
        result = CreateDepthStencilView(width_, height_, depthCopy_);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(ambientLightPS_, L"shaders/ambientLightPS.hlsl");
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(ambientLightPSSSAO_, L"shaders/ambientLightPS.hlsl", { "WITH_SSAO" });
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(ambientLightPSSSAOMask_, L"shaders/ambientLightPS.hlsl", { "SSAO_MASK" });
    }
    if (SUCCEEDED(result)) {
        result = CreateAuxiliaryForDeferredRender();
    }
    if (SUCCEEDED(result)) {
        result = CreateAuxiliaryForShadowMaps();
    }
    if (SUCCEEDED(result)) {
        result = CreateAuxiliaryForTransparent();
    }
    if (SUCCEEDED(result)) {
        result = CreateBuffers();
    }
    isInit_ = SUCCEEDED(result);

    return result;
}

HRESULT SceneManager::GenerateSphere() {
    UINT latLines = 40, longLines = 40;
    UINT numVertices = ((latLines - 2) * longLines) + 2;
    sphereNumIndices_ = (((latLines - 3) * (longLines) * 2) + (longLines * 2)) * 3;

    float phi = 0.0f;
    float theta = 0.0f;

    std::vector<SphereVertex> vertices(numVertices);

    XMVECTOR currVertPos = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

    vertices[0].pos.x = 0.0f;
    vertices[0].pos.y = 0.0f;
    vertices[0].pos.z = 1.0f;

    for (UINT i = 0; i < latLines - 2; i++) {
        theta = (i + 1) * (XM_PI / (latLines - 1));
        XMMATRIX rotationX = XMMatrixRotationX(theta);
        for (UINT j = 0; j < longLines; j++) {
            phi = j * (XM_2PI / longLines);
            XMMATRIX rotationY = XMMatrixRotationZ(phi);
            currVertPos = XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), (rotationX * rotationY));
            currVertPos = XMVector3Normalize(currVertPos);
            vertices[i * (__int64)longLines + j + 1].pos.x = XMVectorGetX(currVertPos);
            vertices[i * (__int64)longLines + j + 1].pos.y = XMVectorGetY(currVertPos);
            vertices[i * (__int64)longLines + j + 1].pos.z = XMVectorGetZ(currVertPos);
        }
    }

    vertices[(__int64)numVertices - 1].pos.x = 0.0f;
    vertices[(__int64)numVertices - 1].pos.y = 0.0f;
    vertices[(__int64)numVertices - 1].pos.z = -1.0f;

    std::vector<UINT> indices(sphereNumIndices_);

    UINT k = 0;
    for (UINT i = 0; i < longLines - 1; i++) {
        indices[k] = i + 1;
        indices[(__int64)k + 2] = 0;
        indices[(__int64)k + 1] = i + 2;
        k += 3;
    }

    indices[k] = longLines;
    indices[(__int64)k + 2] = 0;
    indices[(__int64)k + 1] = 1;
    k += 3;

    for (UINT i = 0; i < latLines - 3; i++) {
        for (UINT j = 0; j < longLines - 1; j++) {
            indices[(__int64)k + 2] = i * longLines + j + 1;
            indices[(__int64)k + 1] = i * longLines + j + 2;
            indices[k] = (i + 1) * longLines + j + 1;

            indices[(__int64)k + 5] = (i + 1) * longLines + j + 1;
            indices[(__int64)k + 4] = i * longLines + j + 2;
            indices[(__int64)k + 3] = (i + 1) * longLines + j + 2;

            k += 6;
        }

        indices[(__int64)k + 2] = (i * longLines) + longLines;
        indices[(__int64)k + 1] = (i * longLines) + 1;
        indices[k] = ((i + 1) * longLines) + longLines;

        indices[(__int64)k + 5] = ((i + 1) * longLines) + longLines;
        indices[(__int64)k + 4] = (i * longLines) + 1;
        indices[(__int64)k + 3] = ((i + 1) * longLines) + 1;

        k += 6;
    }

    for (UINT i = 0; i < longLines - 1; i++) {
        indices[(__int64)k + 2] = numVertices - 1;
        indices[k] = (numVertices - 1) - (i + 1);
        indices[(__int64)k + 1] = (numVertices - 1) - (i + 2);
        k += 3;
    }

    indices[(__int64)k + 2] = numVertices - 1;
    indices[k] = (numVertices - 1) - longLines;
    indices[(__int64)k + 1] = numVertices - 2;

    UINT verticesBytes = sizeof(SphereVertex) * numVertices;
    UINT indicesBytes = sizeof(UINT) * sphereNumIndices_;

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
    result = device_->GetDevice()->CreateBuffer(&desc, &data, &sphereVertexBuffer_);

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

        result = device_->GetDevice()->CreateBuffer(&desc, &data, &sphereIndexBuffer_);
    }

    return result;
}

HRESULT SceneManager::CreateAuxiliaryForDeferredRender() {
    HRESULT result = S_OK;
    if (SUCCEEDED(result)) {
        result = GenerateSphere();
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetStateManager()->CreateRasterizerState(pointLightRasterizerState_, D3D11_FILL_SOLID, D3D11_CULL_BACK); // the inner side of the sphere is preserved,
                                                                                                                                           // just the indices were taken for left coordinate system
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetStateManager()->CreateDepthStencilState(pointLightDepthStencilState_, D3D11_COMPARISON_LESS_EQUAL, D3D11_DEPTH_WRITE_MASK_ZERO);
    }
    if (SUCCEEDED(result)) {
        result = CreateTexture(emissive_, width_, height_);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetVSManager()->LoadShader(pointLightVS_, L"shaders/VS.hlsl", { "HAS_UV_OUT", "INSTANCING"}, sphereVertexDesc_);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(pointLightPS_, L"shaders/pointLightPS.hlsl", { "DEFAULT" });
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(pointLightPSFresnel_, L"shaders/pointLightPS.hlsl", { "FRESNEL" });
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(pointLightPSNDF_, L"shaders/pointLightPS.hlsl", { "ND" });
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(pointLightPSGeometry_, L"shaders/pointLightPS.hlsl", { "GEOMETRY" });
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(directionalLightPS_, L"shaders/directionalEmissiveLightPS.hlsl");
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(directionalLightPSShadowSplits_, L"shaders/directionalEmissiveLightPS.hlsl", { "SHADOW_SPLITS" });
    }

    return result;
}

HRESULT SceneManager::CreateDepthStencilView(int width, int height, RawPtrDepthBuffer& buffer) {
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Format = DXGI_FORMAT_R32_TYPELESS;
    desc.ArraySize = 1;
    desc.MipLevels = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.Height = height;
    desc.Width = width;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    HRESULT result = device_->GetDevice()->CreateTexture2D(&desc, nullptr, &buffer.texture);
    if (SUCCEEDED(result)) {
        D3D11_DEPTH_STENCIL_VIEW_DESC desc = {};
        desc.Format = DXGI_FORMAT_D32_FLOAT;
        desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipSlice = 0;
        desc.Flags = 0;
        result = device_->GetDevice()->CreateDepthStencilView(buffer.texture, &desc, &buffer.DSV);
    }
    if (SUCCEEDED(result)) {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
        desc.Format = DXGI_FORMAT_R32_FLOAT;
        desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipLevels = 1;
        desc.Texture2D.MostDetailedMip = 0;
        result = device_->GetDevice()->CreateShaderResourceView(buffer.texture, &desc, &buffer.SRV);
    }
    return result;
}

HRESULT SceneManager::CreateAuxiliaryForShadowMaps() {
    HRESULT result = S_OK;
    for (UINT i = 0; i < CSM_SPLIT_COUNT && SUCCEEDED(result); ++i) {
        RawPtrDepthBuffer depthBuffer;
        result = CreateDepthStencilView(shadowMapSize, shadowMapSize, depthBuffer);
        if (SUCCEEDED(result)) {
            shadowSplits_.push_back(depthBuffer);
        }
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetStateManager()->CreateDepthStencilState(shadowDepthStencilState_, D3D11_COMPARISON_LESS);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetStateManager()->CreateSamplerState(samplerPCF_, D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP,
            D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_COMPARISON_LESS);
    }
    return result;
}

HRESULT SceneManager::CreateAuxiliaryForTransparent() {
    n_ = 0;
    int minSide = min(width_, height_);
    while (minSide >>= 1) {
        n_++;
    }

    HRESULT result = managerStorage_->GetPSManager()->LoadShader(downsamplePS_, L"shaders/downsampleWithMaxPS.hlsl");
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetStateManager()->CreateDepthStencilState(transparentDepthStencilState_);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetStateManager()->CreateSamplerState(samplerMax_, D3D11_FILTER_MAXIMUM_ANISOTROPIC);
    }
    if (SUCCEEDED(result)) {
        D3D11_TEXTURE2D_DESC textureDesc = {};
        textureDesc.Width = 1;
        textureDesc.Height = 1;
        textureDesc.MipLevels = 1;
        textureDesc.ArraySize = 1;
        textureDesc.Format = DXGI_FORMAT_R32_FLOAT;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.Usage = D3D11_USAGE_STAGING;
        textureDesc.BindFlags = 0;
        textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        textureDesc.MiscFlags = 0;
        result = device_->GetDevice()->CreateTexture2D(&textureDesc, nullptr, &readMaxTexture_);
    }
    if (SUCCEEDED(result)) {
        result = CreateDepthStencilView(width_, height_, transparentDepth_);
    }
    for (int i = 0; i < n_ + 1 && SUCCEEDED(result); ++i) {
        RawPtrTexture texture;
        result = CreateTexture(texture, i);
        if (SUCCEEDED(result)) {
            scaledFrames_.push_back(texture);
        }
    }

    return result;
}

HRESULT SceneManager::CreateTexture(RawPtrTexture& texture, int width, int height, DXGI_FORMAT format) {
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = format;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;
    HRESULT result = device_->GetDevice()->CreateTexture2D(&textureDesc, nullptr, &texture.texture);
    if (SUCCEEDED(result)) {
        D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
        renderTargetViewDesc.Format = format;
        renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        renderTargetViewDesc.Texture2D.MipSlice = 0;

        result = device_->GetDevice()->CreateRenderTargetView(texture.texture, &renderTargetViewDesc, &texture.RTV);
    }
    if (SUCCEEDED(result)) {
        D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
        shaderResourceViewDesc.Format = format;
        shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
        shaderResourceViewDesc.Texture2D.MipLevels = 1;

        result = device_->GetDevice()->CreateShaderResourceView(texture.texture, &shaderResourceViewDesc, &texture.SRV);
    }

    return result;
}

HRESULT SceneManager::CreateTexture(RawPtrTexture& texture, int i) {
    return CreateTexture(texture, pow(2, i), pow(2, i), DXGI_FORMAT_R32_FLOAT);
}

HRESULT SceneManager::CreateBuffers() {
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(WorldMatrixBuffer);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;
    HRESULT result = device_->GetDevice()->CreateBuffer(&desc, nullptr, &worldMatrixBuffer_);
    if (SUCCEEDED(result)) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(InstancingWorldMatrixBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;
        result = device_->GetDevice()->CreateBuffer(&desc, nullptr, &instancingWorldMatrixBuffer_);
    }
    if (SUCCEEDED(result)) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(ViewMatrixBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;
        result = device_->GetDevice()->CreateBuffer(&desc, nullptr, &viewMatrixBuffer_);
    }
    if (SUCCEEDED(result)) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(ShadowMapAlphaCutoffBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;
        result = device_->GetDevice()->CreateBuffer(&desc, nullptr, &shadowMapAlphaCutoffBuffer_);
    }
    if (SUCCEEDED(result)) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(LightBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;
        result = device_->GetDevice()->CreateBuffer(&desc, nullptr, &lightBuffer_);
    }
    if (SUCCEEDED(result)) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(DirectionalLightBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;
        result = device_->GetDevice()->CreateBuffer(&desc, nullptr, &directionalLightBuffer_);
    }
    if (SUCCEEDED(result)) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(MaterialParamsBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;
        result = device_->GetDevice()->CreateBuffer(&desc, nullptr, &materialParamsBuffer_);
    }
    if (SUCCEEDED(result)) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(ForwardRenderViewMatrixBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;
        result = device_->GetDevice()->CreateBuffer(&desc, nullptr, &forwardRenderViewMatrixBuffer_);
    }
    if (SUCCEEDED(result)) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(MatricesBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;
        result = device_->GetDevice()->CreateBuffer(&desc, nullptr, &matricesBuffer_);
    }
    if (SUCCEEDED(result)) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(SSAOParamsBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;
        result = device_->GetDevice()->CreateBuffer(&desc, nullptr, &SSAOParamsBuffer_);
    }
    if (SUCCEEDED(result)) {
        for (int i = 0; i < MAX_SSAO_SAMPLE_COUNT; ++i) {
            XMFLOAT4 s = XMFLOAT4(
                -1.0f + (rand() / (RAND_MAX / 2.0f)), // [-1; 1]
                -1.0f + (rand() / (RAND_MAX / 2.0f)), // [-1; 1]
                rand() / ((float)RAND_MAX), // [0; 1]
                0.0f
            );
            float len = sqrt(s.x * s.x + s.y * s.y + s.z * s.z);
            float scale = rand() / ((float)RAND_MAX) / len; // [0; 1]
            s = XMFLOAT4(s.x * scale, s.y * scale, s.z * scale, 0.0f);

            scale = i / ((float)MAX_SSAO_SAMPLE_COUNT);
            scale = 0.1 + scale * scale * 0.9;
            s = XMFLOAT4(s.x * scale, s.y * scale, s.z * scale, 0.0f);

            SSAOSamples_[i] = s;
        }
        for (int i = 0; i < NOISE_BUFFER_SIZE / NOISE_BUFFER_SPLIT_SIZE; ++i) {
            for (int j = 0; j < NOISE_BUFFER_SPLIT_SIZE; ++j) {
                XMFLOAT4 s = XMFLOAT4(
                    -1.0f + (rand() / (RAND_MAX / 2.0f)), // [-1; 1]
                    -1.0f + (rand() / (RAND_MAX / 2.0f)), // [-1; 1]
                    0.0f,
                    0.0f
                );
                float len = sqrt(s.x * s.x + s.y * s.y);
                s = XMFLOAT4(s.x / len, s.y / len, 0.0f, 0.0f);
                SSAONoise_[j * NOISE_BUFFER_SPLIT_SIZE + i] = s;
            }
        }
    }
    return result;
}

HRESULT SceneManager::LoadScene(const std::string& name, UINT& index, UINT& count, const XMMATRIX& transformation) {
    tinygltf::TinyGLTF context;
    tinygltf::Model model;
    std::string error;
    std::string warning;
    if (!context.LoadASCIIFromFile(&model, &error, &warning, name)) {
        if (!error.empty()) {
            OutputDebugStringA(error.c_str());
        }
        if (!warning.empty()) {
            OutputDebugStringA(warning.c_str());
        }
        return E_FAIL;
    }

    index = scenes_.size();

    SceneArrays arrays;
    for (auto& gs : model.scenes) {
        Scene s;
        s.arraysId = sceneArrays_.size();
        s.transformation = transformation;
        s.rootNodes = gs.nodes;
        scenes_.push_back(s);
    }

    HRESULT result = CreateBufferAccessors(model, arrays);
    if (SUCCEEDED(result)) {
        result = CreateSamplers(model, arrays);
    }
    if (SUCCEEDED(result)) {
        result = CreateTextures(model, arrays, name);
    }
    if (SUCCEEDED(result)) {
        result = CreateMaterials(model, arrays);
    }
    if (SUCCEEDED(result)) {
        result = CreateMeshes(model, arrays);
    }
    if (SUCCEEDED(result)) {
        result = CreateNodes(model, arrays);
    }
    if (FAILED(result)) {
        Cleanup();
    }
    else {
        sceneArrays_.push_back(arrays);
    }

    count = scenes_.size() - index;

    return result;
}

HRESULT SceneManager::CreateBufferAccessors(const tinygltf::Model& model, SceneArrays& arrays) {
    HRESULT result = S_OK;
    for (auto& ga : model.accessors) {
        BufferAccessor accessor;
        accessor.count = ga.count;
        accessor.byteOffset = ga.byteOffset;
        accessor.format = GetFormat(ga, accessor.byteStride);

        const tinygltf::BufferView& gbv = model.bufferViews[ga.bufferView];
        if (gbv.byteStride != 0) {
            accessor.byteStride = gbv.byteStride;
        }

        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = gbv.byteLength;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = (gbv.target == TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER) ? D3D11_BIND_INDEX_BUFFER : D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        unsigned char* bufferPart = new unsigned char[gbv.byteLength];
        if (!bufferPart) {
            return E_FAIL;
        }
        memcpy(bufferPart, model.buffers[gbv.buffer].data.data() + gbv.byteOffset, gbv.byteLength);

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = bufferPart;
        data.SysMemPitch = gbv.byteLength;
        data.SysMemSlicePitch = 0;

        ID3D11Buffer* buffer = nullptr;
        result = device_->GetDevice()->CreateBuffer(&desc, &data, &buffer);
        delete[] bufferPart;
        if (FAILED(result)) {
            break;
        }
        accessor.buffer = std::shared_ptr<ID3D11Buffer>(buffer, utilities::DXPtrDeleter<ID3D11Buffer*>);
        arrays.accessors.push_back(accessor);
    }
    return result;
}

DXGI_FORMAT SceneManager::GetFormat(const tinygltf::Accessor& accessor, UINT& size) {
    DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    switch (accessor.type) {
    case TINYGLTF_TYPE_SCALAR:
        format = GetFormatScalar(accessor, size);
        break;
    case TINYGLTF_TYPE_VEC2:
        format = GetFormatVec2(accessor, size);
        break;
    case TINYGLTF_TYPE_VEC3:
        format = GetFormatVec3(accessor, size);
        break;
    case TINYGLTF_TYPE_VEC4:
        format = GetFormatVec4(accessor, size);
        break;
    default:
        break;
    }
    return format;
}

DXGI_FORMAT SceneManager::GetFormatScalar(const tinygltf::Accessor& accessor, UINT& size) {
    DXGI_FORMAT format = DXGI_FORMAT_R32_FLOAT;
    switch (accessor.componentType) {
    case TINYGLTF_COMPONENT_TYPE_BYTE:
        if (accessor.normalized) {
            format = DXGI_FORMAT_R8_SNORM;
        }
        else {
            format = DXGI_FORMAT_R8_SINT;
        }
        size = 1;
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        if (accessor.normalized) {
            format = DXGI_FORMAT_R8_UNORM;
        }
        else {
            format = DXGI_FORMAT_R8_UINT;
        }
        size = 1;
        break;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
        if (accessor.normalized) {
            format = DXGI_FORMAT_R16_SNORM;
        }
        else {
            format = DXGI_FORMAT_R16_SINT;
        }
        size = 2;
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        if (accessor.normalized) {
            format = DXGI_FORMAT_R16_UNORM;
        }
        else {
            format = DXGI_FORMAT_R16_UINT;
        }
        size = 2;
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        format = DXGI_FORMAT_R32_UINT;
        size = 4;
        break;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        format = DXGI_FORMAT_R32_FLOAT;
        size = 4;
        break;
    default:
        break;
    }
    return format;
}

DXGI_FORMAT SceneManager::GetFormatVec2(const tinygltf::Accessor& accessor, UINT& size) {
    DXGI_FORMAT format = DXGI_FORMAT_R32G32_FLOAT;
    switch (accessor.componentType) {
    case TINYGLTF_COMPONENT_TYPE_BYTE:
        if (accessor.normalized) {
            format = DXGI_FORMAT_R8G8_SNORM;
        }
        else {
            format = DXGI_FORMAT_R8G8_SINT;
        }
        size = 2;
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        if (accessor.normalized) {
            format = DXGI_FORMAT_R8G8_UNORM;
        }
        else {
            format = DXGI_FORMAT_R8G8_UINT;
        }
        size = 2;
        break;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
        if (accessor.normalized) {
            format = DXGI_FORMAT_R16G16_SNORM;
        }
        else {
            format = DXGI_FORMAT_R16G16_SINT;
        }
        size = 4;
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        if (accessor.normalized) {
            format = DXGI_FORMAT_R16G16_UNORM;
        }
        else {
            format = DXGI_FORMAT_R16G16_UINT;
        }
        size = 4;
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        format = DXGI_FORMAT_R32G32_UINT;
        size = 8;
        break;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        format = DXGI_FORMAT_R32G32_FLOAT;
        size = 8;
        break;
    default:
        break;
    }
    return format;
}

DXGI_FORMAT SceneManager::GetFormatVec3(const tinygltf::Accessor& accessor, UINT& size) {
    DXGI_FORMAT format = DXGI_FORMAT_R32G32B32_FLOAT;
    switch (accessor.componentType) {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        format = DXGI_FORMAT_R32G32B32_UINT;
        size = 12;
        break;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        format = DXGI_FORMAT_R32G32B32_FLOAT;
        size = 12;
        break;
    default:
        break;
    }
    return format;
}

DXGI_FORMAT SceneManager::GetFormatVec4(const tinygltf::Accessor& accessor, UINT& size) {
    DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    switch (accessor.componentType) {
    case TINYGLTF_COMPONENT_TYPE_BYTE:
        if (accessor.normalized) {
            format = DXGI_FORMAT_R8G8B8A8_SNORM;
        }
        else {
            format = DXGI_FORMAT_R8G8B8A8_SINT;
        }
        size = 4;
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        if (accessor.normalized) {
            format = DXGI_FORMAT_R8G8B8A8_UNORM;
        }
        else {
            format = DXGI_FORMAT_R8G8B8A8_UINT;
        }
        size = 4;
        break;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
        if (accessor.normalized) {
            format = DXGI_FORMAT_R16G16B16A16_SNORM;
        }
        else {
            format = DXGI_FORMAT_R16G16B16A16_SINT;
        }
        size = 8;
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        if (accessor.normalized) {
            format = DXGI_FORMAT_R16G16B16A16_UNORM;
        }
        else {
            format = DXGI_FORMAT_R16G16B16A16_UINT;
        }
        size = 8;
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        format = DXGI_FORMAT_R32G32B32A32_UINT;
        size = 16;
        break;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        size = 16;
        break;
    default:
        break;
    }
    return format;
}

HRESULT SceneManager::CreateSamplers(const tinygltf::Model& model, SceneArrays& arrays) {
    HRESULT result = S_OK;
    for (auto& gs : model.samplers) {
        D3D11_FILTER filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        D3D11_TEXTURE_ADDRESS_MODE modeU, modeV;
        switch (gs.minFilter) {
        case TINYGLTF_TEXTURE_FILTER_NEAREST:
            if (gs.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST) {
                filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
            }
            else {
                filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            }
            break;
        case TINYGLTF_TEXTURE_FILTER_LINEAR:
            if (gs.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST) {
                filter = D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
            }
            else {
                filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
            }
            break;
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
            if (gs.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST) {
                filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
            }
            else {
                filter = D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
            }
            break;
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
            if (gs.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST) {
                filter = D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
            }
            else {
                filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
            }
            break;
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
            if (gs.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST) {
                filter = D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
            }
            else {
                filter = D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
            }
            break;
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
            if (gs.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST) {
                filter = D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
            }
            else {
                filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            }
            break;
        default:
            break;
        }
        modeU = GetSamplerMode(gs.wrapS);
        modeV = GetSamplerMode(gs.wrapT);
        std::shared_ptr<ID3D11SamplerState> sampler;
        result = managerStorage_->GetStateManager()->CreateSamplerState(sampler, filter, modeU, modeV);
        if (FAILED(result)) {
            break;
        }
        arrays.samplers.push_back(sampler);
    }
    return result;
}

D3D11_TEXTURE_ADDRESS_MODE SceneManager::GetSamplerMode(int m) {
    D3D11_TEXTURE_ADDRESS_MODE mode = D3D11_TEXTURE_ADDRESS_WRAP;
    switch (m) {
    case TINYGLTF_TEXTURE_WRAP_REPEAT:
        return D3D11_TEXTURE_ADDRESS_WRAP;
    case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
        return D3D11_TEXTURE_ADDRESS_CLAMP;
    case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
        return D3D11_TEXTURE_ADDRESS_MIRROR;
    default:
        break;
    }
    return mode;
}

HRESULT SceneManager::CreateTextures(const tinygltf::Model& model, SceneArrays& arrays, const std::string& gltfFileName) {
    HRESULT result = S_OK;
    auto pos = gltfFileName.rfind('/');
    std::string imagesFolder = gltfFileName.substr(0, pos + 1);
    for (auto& gi : model.images) {
        std::string imagePath = imagesFolder + gi.uri;
        std::shared_ptr<Texture> texture;
        result = managerStorage_->GetTextureManager()->LoadTexture(texture, imagePath);
        if (FAILED(result)) {
            break;
        }
        arrays.textures.push_back(texture);
    }
    return result;
}

HRESULT SceneManager::CreateMaterials(const tinygltf::Model& model, SceneArrays& arrays) {
    HRESULT result = S_OK;
    for (auto& gm : model.materials) {
        Material material;
        if (gm.alphaMode == "BLEND") {
            material.mode = AlphaMode::BLEND_MODE;
            result = managerStorage_->GetStateManager()->CreateBlendState(material.blendState);
            if (SUCCEEDED(result)) {
                result = managerStorage_->GetStateManager()->CreateDepthStencilState(material.depthStencilState,
                    D3D11_COMPARISON_GREATER_EQUAL, D3D11_DEPTH_WRITE_MASK_ZERO);
            }
        }
        else if (gm.alphaMode == "MASK") {
            material.mode = AlphaMode::ALPHA_CUTOFF_MODE;
            material.alphaCutoff = gm.alphaCutoff;
            result = managerStorage_->GetStateManager()->CreateDepthStencilState(material.depthStencilState);
        }
        else {
            material.mode = AlphaMode::OPAQUE_MODE;
            result = managerStorage_->GetStateManager()->CreateDepthStencilState(material.depthStencilState);
        }

        if (FAILED(result)) {
            break;
        }

        if (!gm.doubleSided) {
            material.cullMode = D3D11_CULL_BACK;
        }
        result = managerStorage_->GetStateManager()->CreateRasterizerState(material.rasterizerState, D3D11_FILL_SOLID, material.cullMode);
        if (FAILED(result)) {
            break;
        }

        material.baseColorFactor = XMFLOAT4(
            gm.pbrMetallicRoughness.baseColorFactor[0],
            gm.pbrMetallicRoughness.baseColorFactor[1],
            gm.pbrMetallicRoughness.baseColorFactor[2],
            gm.pbrMetallicRoughness.baseColorFactor[3]
        );
        material.metallicFactor = gm.pbrMetallicRoughness.metallicFactor;
        material.roughnessFactor = gm.pbrMetallicRoughness.roughnessFactor;
        material.emissiveFactor = XMFLOAT3(
            gm.emissiveFactor[0],
            gm.emissiveFactor[1],
            gm.emissiveFactor[2]
        );
        material.occlusionStrength = gm.occlusionTexture.strength;
        material.normalScale = gm.normalTexture.scale;

        int index = gm.pbrMetallicRoughness.baseColorTexture.index;
        if (index >= 0) {
            material.baseColorTA = { gm.pbrMetallicRoughness.baseColorTexture.texCoord, model.textures[index].source, model.textures[index].sampler, true };
        }

        index = gm.pbrMetallicRoughness.metallicRoughnessTexture.index;
        if (index >= 0) {
            material.roughMetallicTA = { gm.pbrMetallicRoughness.metallicRoughnessTexture.texCoord, model.textures[index].source, model.textures[index].sampler, false };
        }

        index = gm.normalTexture.index;
        if (index >= 0) {
            material.normalTA = { gm.normalTexture.texCoord, model.textures[index].source, model.textures[index].sampler, false };
        }

        index = gm.emissiveTexture.index;
        if (index >= 0) {
            material.emissiveTA = { gm.emissiveTexture.texCoord, model.textures[index].source, model.textures[index].sampler, true };
        }

        index = gm.occlusionTexture.index;
        if (index >= 0) {
            material.occlusionTA = { gm.occlusionTexture.texCoord, model.textures[index].source, model.textures[index].sampler, false };
        }

        arrays.materials.push_back(material);
    }
    return result;
}

HRESULT SceneManager::CreateMeshes(const tinygltf::Model& model, SceneArrays& arrays) {
    HRESULT result = S_OK;
    for (auto& gm : model.meshes) {
        Mesh mesh;
        for (auto& gp : gm.primitives) {
            Primitive primitive;
            switch (gp.mode) {
            case TINYGLTF_MODE_POINTS:
                primitive.mode = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
                break;
            case TINYGLTF_MODE_LINE:
                primitive.mode = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
                break;
            case TINYGLTF_MODE_LINE_STRIP:
                primitive.mode = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
                break;
            case TINYGLTF_MODE_TRIANGLES:
                primitive.mode = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
                break;
            case TINYGLTF_MODE_TRIANGLE_STRIP:
                primitive.mode = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
                break;
            default:
                primitive.mode = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
                break;
            }
            primitive.materialId = gp.material;
            primitive.indicesAccessorId = gp.indices;

            std::vector<std::string> defines;
            std::vector<D3D11_INPUT_ELEMENT_DESC> inputElementDesc;
            ParseAttributes(arrays, gp, primitive.attributes, defines, inputElementDesc);

            result = CreateShaders(primitive, arrays, defines, inputElementDesc);
            if (FAILED(result)) {
                break;
            }

            switch (arrays.materials[primitive.materialId].mode) {
            case AlphaMode::BLEND_MODE:
                mesh.transparentPrimitives.push_back(primitive);
                break;
            case AlphaMode::ALPHA_CUTOFF_MODE:
                mesh.primitivesWithAlphaCutoff.push_back(primitive);
                break;
            default:
                mesh.opaquePrimitives.push_back(primitive);
                break;
            }
        }
        if (FAILED(result)) {
            break;
        }
        arrays.meshes.push_back(mesh);
    }
    return result;
}

void SceneManager::ParseAttributes(const SceneArrays& arrays, const tinygltf::Primitive& primitive,
    std::vector<Attribute>& attributes, std::vector<std::string>& baseDefines, std::vector<D3D11_INPUT_ELEMENT_DESC>& desc) {
    std::vector<Attribute> tmp;
    baseDefines.clear();
    baseDefines.push_back("HAS_NORMAL");
    for (int i = 0; i < 9; ++i) {
        tmp.push_back(Attribute());
    }
    for (auto& ga : primitive.attributes) {
        if (ga.first == "TANGENT") {
            baseDefines.push_back("HAS_TANGENT");
            tmp[2].semantic = "TANGENT";
            tmp[2].verticesAccessorId = ga.second;
        }
        else if (ga.first == "TEXCOORD_0") {
            baseDefines.push_back("HAS_TEXCOORD_0");
            tmp[3].semantic = "TEXCOORD_0";
            tmp[3].verticesAccessorId = ga.second;
        }
        else if (ga.first == "TEXCOORD_1") {
            baseDefines.push_back("HAS_TEXCOORD_1");
            tmp[4].semantic = "TEXCOORD_1";
            tmp[4].verticesAccessorId = ga.second;
        }
        else if (ga.first == "TEXCOORD_2") {
            baseDefines.push_back("HAS_TEXCOORD_2");
            tmp[5].semantic = "TEXCOORD_2";
            tmp[5].verticesAccessorId = ga.second;
        }
        else if (ga.first == "TEXCOORD_3") {
            baseDefines.push_back("HAS_TEXCOORD_3");
            tmp[6].semantic = "TEXCOORD_3";
            tmp[6].verticesAccessorId = ga.second;
        }
        else if (ga.first == "TEXCOORD_4") { // by the number of textures of the material
            baseDefines.push_back("HAS_TEXCOORD_4");
            tmp[7].semantic = "TEXCOORD_4";
            tmp[7].verticesAccessorId = ga.second;
        }
        else if (ga.first == "COLOR") {
            baseDefines.push_back("HAS_COLOR");
            tmp[8].semantic = "COLOR";
            tmp[8].verticesAccessorId = ga.second;
        }
        else if (ga.first == "POSITION") {
            tmp[0].semantic = "POSITION";
            tmp[0].verticesAccessorId = ga.second;
        }
        else if (ga.first == "NORMAL")  {
            tmp[1].semantic = "NORMAL";
            tmp[1].verticesAccessorId = ga.second;
        }
        else {
            ; // ignore others attributes
        }
    }
    const Material& material = arrays.materials[primitive.material];
    if (material.baseColorTA.textureId >= 0) {
        baseDefines.push_back("HAS_COLOR_TEXTURE");
    }
    if (material.roughMetallicTA.textureId >= 0) {
        baseDefines.push_back("HAS_MR_TEXTURE");
    }
    if (material.normalTA.textureId >= 0) {
        baseDefines.push_back("HAS_NORMAL_TEXTURE");
    }
    if (material.occlusionTA.textureId >= 0) {
        baseDefines.push_back("HAS_OCCLUSION_TEXTURE");
    }
    if (material.emissiveTA.textureId >= 0) {
        baseDefines.push_back("HAS_EMISSIVE_TEXTURE");
    }
    if (material.mode == AlphaMode::ALPHA_CUTOFF_MODE) {
        baseDefines.push_back("HAS_ALPHA_CUTOFF");
    }
    attributes.clear();
    desc.clear();
    for (const auto& a : tmp) {
        if (a.semantic != "EMPTY") {
            attributes.push_back(a);
            const BufferAccessor& accessor = arrays.accessors[a.verticesAccessorId];
            if (a.semantic == "TANGENT") {
                desc.push_back(D3D11_INPUT_ELEMENT_DESC{ "TANGENT", 0, accessor.format, (unsigned long)desc.size(), 0, D3D11_INPUT_PER_VERTEX_DATA, 0 });
            }
            else if (a.semantic == "TEXCOORD_0") {
                desc.push_back(D3D11_INPUT_ELEMENT_DESC{ "TEXCOORD", 0, accessor.format, (unsigned long)desc.size(), 0, D3D11_INPUT_PER_VERTEX_DATA, 0 });
            }
            else if (a.semantic == "TEXCOORD_1") {
                desc.push_back(D3D11_INPUT_ELEMENT_DESC{ "TEXCOORD", 1, accessor.format, (unsigned long)desc.size(), 0, D3D11_INPUT_PER_VERTEX_DATA, 0 });
            }
            else if (a.semantic == "TEXCOORD_2") {
                desc.push_back(D3D11_INPUT_ELEMENT_DESC{ "TEXCOORD", 2, accessor.format, (unsigned long)desc.size(), 0, D3D11_INPUT_PER_VERTEX_DATA, 0 });
            }
            else if (a.semantic == "TEXCOORD_3") {
                desc.push_back(D3D11_INPUT_ELEMENT_DESC{ "TEXCOORD", 3, accessor.format, (unsigned long)desc.size(), 0, D3D11_INPUT_PER_VERTEX_DATA, 0 });
            }
            else if (a.semantic == "TEXCOORD_4") {
                desc.push_back(D3D11_INPUT_ELEMENT_DESC{ "TEXCOORD", 4, accessor.format, (unsigned long)desc.size(), 0, D3D11_INPUT_PER_VERTEX_DATA, 0 });
            }
            else if (a.semantic == "COLOR") {
                desc.push_back(D3D11_INPUT_ELEMENT_DESC{ "COLOR", 0, accessor.format, (unsigned long)desc.size(), 0, D3D11_INPUT_PER_VERTEX_DATA, 0 });
            }
            else if (a.semantic == "POSITION") {
                desc.push_back(D3D11_INPUT_ELEMENT_DESC{ "POSITION", 0, accessor.format, (unsigned long)desc.size(), 0, D3D11_INPUT_PER_VERTEX_DATA, 0 });
            }
            else { // NORMAL
                desc.push_back(D3D11_INPUT_ELEMENT_DESC{ "NORMAL", 0, accessor.format, (unsigned long)desc.size(), 0, D3D11_INPUT_PER_VERTEX_DATA, 0 });
            }
        }
    }
}

HRESULT SceneManager::CreateShaders(Primitive& primitive, const SceneArrays& arrays,
    const std::vector<std::string>& baseDefines, const std::vector<D3D11_INPUT_ELEMENT_DESC>& desc) {
    std::vector<std::string> defaulMacros = baseDefines;
    defaulMacros.push_back("DEFAULT");

    std::vector<std::string> fresnelMacros = baseDefines;
    fresnelMacros.push_back("FRESNEL");

    std::vector<std::string> ndfMacros = baseDefines;
    ndfMacros.push_back("ND");

    std::vector<std::string> geometryMacros = baseDefines;
    geometryMacros.push_back("GEOMETRY");

    std::vector<std::string> shadowSplitsMacros = defaulMacros;
    shadowSplitsMacros.push_back("SHADOW_SPLITS");

    std::vector<std::string> SSAOMacros = defaulMacros;
    SSAOMacros.push_back("WITH_SSAO");

    std::vector<std::string> SSAOMaskMacros = defaulMacros;
    SSAOMaskMacros.push_back("SSAO_MASK");

    std::vector<std::string> OpaqueSSAOMaskMacros = SSAOMaskMacros;

    if (arrays.materials[primitive.materialId].mode == AlphaMode::BLEND_MODE) {
        defaulMacros.push_back("TRANSPARENT");
        fresnelMacros.push_back("TRANSPARENT");
        ndfMacros.push_back("TRANSPARENT");
        geometryMacros.push_back("TRANSPARENT");
        shadowSplitsMacros.push_back("TRANSPARENT");
        SSAOMacros.push_back("TRANSPARENT");
        SSAOMaskMacros.push_back("TRANSPARENT");
    }

    std::vector<std::string> shadowVSMacros = baseDefines;
    shadowVSMacros.push_back("HAS_COLOR_OUT");
    shadowVSMacros.push_back("HAS_TEXCOORD_OUT");

    std::vector<std::string> VSMacros = shadowVSMacros;
    VSMacros.push_back("HAS_WORLD_POS_OUT");
    VSMacros.push_back("HAS_NORMAL_OUT");
    VSMacros.push_back("HAS_TANGENT_OUT");

    HRESULT result = managerStorage_->GetVSManager()->LoadShader(primitive.VS, L"shaders/VS.hlsl", VSMacros, desc);
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetVSManager()->LoadShader(primitive.shadowVS, L"shaders/VS.hlsl", shadowVSMacros, desc);
    }
    if (SUCCEEDED(result) && arrays.materials[primitive.materialId].mode != AlphaMode::BLEND_MODE) {
        result = managerStorage_->GetPSManager()->LoadShader(primitive.gBufferPS, L"shaders/gBufferPS.hlsl", baseDefines);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(primitive.PSDefault, L"shaders/forwardRenderPS.hlsl", defaulMacros);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(primitive.PSFresnel, L"shaders/forwardRenderPS.hlsl", fresnelMacros);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(primitive.PSNdf, L"shaders/forwardRenderPS.hlsl", ndfMacros);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(primitive.PSGeometry, L"shaders/forwardRenderPS.hlsl", geometryMacros);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(primitive.PSShadowSplits, L"shaders/forwardRenderPS.hlsl", shadowSplitsMacros);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(primitive.PSSSAOMask, L"shaders/forwardRenderPS.hlsl", OpaqueSSAOMaskMacros);
    }
    if (SUCCEEDED(result) && arrays.materials[primitive.materialId].mode == AlphaMode::BLEND_MODE) {
        result = managerStorage_->GetPSManager()->LoadShader(primitive.transparentPSSSAO, L"shaders/forwardRenderPS.hlsl", SSAOMacros);
        if (SUCCEEDED(result)) {
            result = managerStorage_->GetPSManager()->LoadShader(primitive.transparentPSSSAOMask, L"shaders/forwardRenderPS.hlsl", SSAOMaskMacros);
        }
    }
    if (SUCCEEDED(result) && arrays.materials[primitive.materialId].mode != AlphaMode::OPAQUE_MODE) { // opaque without pixel shader for shadow map
        result = managerStorage_->GetPSManager()->LoadShader(primitive.shadowPS, L"shaders/shadowPS.hlsl", baseDefines);
    }

    return result;
}

HRESULT SceneManager::CreateNodes(const tinygltf::Model& model, SceneArrays& arrays) {
    HRESULT result = S_OK;
    for (auto& gn : model.nodes) {
        Node node;
        node.meshId = gn.mesh;
        node.children = gn.children;

        if (gn.matrix.empty()) {
            XMMATRIX transformation = XMMatrixIdentity();
            if (!gn.translation.empty()) {
                transformation = XMMatrixMultiply(XMMatrixTranslation(gn.translation[0], gn.translation[1], gn.translation[2]), transformation);
            }
            if (!gn.rotation.empty()) {
                transformation = XMMatrixMultiply(XMMatrixRotationQuaternion({
                    (float)gn.rotation[0], (float)gn.rotation[1], (float)gn.rotation[2], (float)gn.rotation[3]
                }), transformation);
            }
            if (!gn.scale.empty()) {
                transformation = XMMatrixMultiply(XMMatrixScaling(gn.scale[0], gn.scale[1], gn.scale[2]), transformation);
            }
            node.transformation = transformation;
        }
        else {
            node.transformation = {
                XMVECTOR{(float)gn.matrix[0], (float)gn.matrix[1], (float)gn.matrix[2], (float)gn.matrix[3]},
                XMVECTOR{(float)gn.matrix[4], (float)gn.matrix[5], (float)gn.matrix[6], (float)gn.matrix[7]},
                XMVECTOR{(float)gn.matrix[8], (float)gn.matrix[9], (float)gn.matrix[10], (float)gn.matrix[11]},
                XMVECTOR{(float)gn.matrix[12], (float)gn.matrix[13], (float)gn.matrix[14], (float)gn.matrix[15]}
            };
        }

        arrays.nodes.push_back(node);
    }
    return result;
}

bool SceneManager::CreateShadowMaps(const std::vector<int>& sceneIndices) {
    if (!!annotation_) {
        annotation_->BeginEvent(L"Create_shadow_maps");
    }
    for (UINT i = 0; i < CSM_SPLIT_COUNT; ++i) {
        device_->GetDeviceContext()->ClearDepthStencilView(shadowSplits_[i].DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
        device_->GetDeviceContext()->OMSetRenderTargets(0, nullptr, shadowSplits_[i].DSV);
        device_->GetDeviceContext()->OMSetDepthStencilState(shadowDepthStencilState_.get(), 0);
        viewport_.Width = shadowMapSize;
        viewport_.Height = shadowMapSize;
        device_->GetDeviceContext()->RSSetViewports(1, &viewport_);
        device_->GetDeviceContext()->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

        ViewMatrixBuffer viewMatrix;
        viewMatrix.viewProjectionMatrix = directionalLight_->viewProjectionMatrices[i];
        device_->GetDeviceContext()->UpdateSubresource(viewMatrixBuffer_, 0, nullptr, &viewMatrix, 0, 0);

        for (auto j : sceneIndices) {
            if (j < 0 || j >= scenes_.size()) {
                continue;
            }
            for (auto node : scenes_[j].rootNodes) {
                if (!CreateShadowMapForNode(scenes_[j].arraysId, node, scenes_[j].transformation)) {
                    if (!!annotation_) {
                        annotation_->EndEvent();
                    }
                    return false;
                }
            }
        }
    }
    if (!!annotation_) {
        annotation_->EndEvent();
    }
    return true;
}

bool SceneManager::CreateShadowMapForNode(int arrayId, int nodeId, const XMMATRIX& transformation) {
    const Node& node = sceneArrays_[arrayId].nodes[nodeId];
    XMMATRIX currentTransformation = XMMatrixMultiply(node.transformation, transformation);

    if (node.meshId >= 0) {
        const Mesh& mesh = sceneArrays_[arrayId].meshes[node.meshId];
        for (auto& p : mesh.opaquePrimitives) {
            if (!CreateShadowMapForPrimitive(arrayId, p, AlphaMode::OPAQUE_MODE, currentTransformation)) {
                return false;
            }
        }
        if (!excludeTransparent) {
            for (auto& p : mesh.transparentPrimitives) {
                if (!CreateShadowMapForPrimitive(arrayId, p, AlphaMode::ALPHA_CUTOFF_MODE, currentTransformation)) {
                    return false;
                }
            }
        }
        for (auto& p : mesh.primitivesWithAlphaCutoff) {
            if (!CreateShadowMapForPrimitive(arrayId, p, AlphaMode::ALPHA_CUTOFF_MODE, currentTransformation)) {
                return false;
            }
        }
    }

    for (auto c : node.children) {
        if (!CreateShadowMapForNode(arrayId, c, currentTransformation)) {
            return false;
        }
    }

    return true;
}

bool SceneManager::CreateShadowMapForPrimitive(int arrayId, const Primitive& primitive, AlphaMode mode, const XMMATRIX& transformation) {
    const Material& material = sceneArrays_[arrayId].materials[primitive.materialId];

    WorldMatrixBuffer worldMatrix;
    worldMatrix.worldMatrix = transformation;
    device_->GetDeviceContext()->UpdateSubresource(worldMatrixBuffer_, 0, nullptr, &worldMatrix, 0, 0);

    std::shared_ptr<ID3D11RasterizerState> rasterizerState;
    HRESULT result = managerStorage_->GetStateManager()->CreateRasterizerState(rasterizerState, D3D11_FILL_SOLID, material.cullMode, depthBias, slopeScaleBias);
    if (FAILED(result)) {
        return false;
    }

    if (mode == AlphaMode::ALPHA_CUTOFF_MODE) {
        ShadowMapAlphaCutoffBuffer alphaCutoff;
        alphaCutoff.baseColorFactor = material.baseColorFactor;
        alphaCutoff.alphaCutoffTexCoord = XMFLOAT4(material.alphaCutoff, material.baseColorTA.texCoordId, 0.0f, 0.0f);
        device_->GetDeviceContext()->UpdateSubresource(shadowMapAlphaCutoffBuffer_, 0, nullptr, &alphaCutoff, 0, 0);

        if (material.baseColorTA.textureId >= 0) {
            ID3D11ShaderResourceView* resources[] = { sceneArrays_[arrayId].textures[material.baseColorTA.textureId]->GetSRV(material.baseColorTA.isSRGB).get() };
            device_->GetDeviceContext()->PSSetShaderResources(0, 1, resources);

            ID3D11SamplerState* samplers[] = { sceneArrays_[arrayId].samplers[material.baseColorTA.samplerId].get() };
            device_->GetDeviceContext()->PSSetSamplers(0, 1, samplers);
        }
    }

    device_->GetDeviceContext()->IASetInputLayout(primitive.shadowVS->GetInputLayout().get());
    UINT numBuffers = primitive.attributes.size();
    std::vector<ID3D11Buffer*> vertexBuffers;
    std::vector<UINT> strides;
    std::vector<UINT> offsets;
    for (auto& a : primitive.attributes) {
        const BufferAccessor& accessor = sceneArrays_[arrayId].accessors[a.verticesAccessorId];
        vertexBuffers.push_back(accessor.buffer.get());
        strides.push_back(accessor.byteStride);
        offsets.push_back(accessor.byteOffset);
    }
    device_->GetDeviceContext()->IASetVertexBuffers(0, numBuffers, vertexBuffers.data(), strides.data(), offsets.data());
    device_->GetDeviceContext()->RSSetState(rasterizerState.get());
    device_->GetDeviceContext()->IASetPrimitiveTopology(primitive.mode);
    device_->GetDeviceContext()->VSSetShader(primitive.shadowVS->GetShader().get(), nullptr, 0);
    device_->GetDeviceContext()->VSSetConstantBuffers(0, 1, &worldMatrixBuffer_);
    device_->GetDeviceContext()->VSSetConstantBuffers(1, 1, &viewMatrixBuffer_);

    if (mode == AlphaMode::ALPHA_CUTOFF_MODE) {
        device_->GetDeviceContext()->PSSetShader(primitive.shadowPS->GetShader().get(), nullptr, 0);
        device_->GetDeviceContext()->PSSetConstantBuffers(0, 1, &shadowMapAlphaCutoffBuffer_);
    }
    else {
        device_->GetDeviceContext()->PSSetShader(nullptr, nullptr, 0);
    }

    if (primitive.indicesAccessorId >= 0) {
        const BufferAccessor& accessor = sceneArrays_[arrayId].accessors[primitive.indicesAccessorId];
        device_->GetDeviceContext()->IASetIndexBuffer(accessor.buffer.get(), accessor.format, accessor.byteOffset);
        device_->GetDeviceContext()->DrawIndexed(accessor.count, 0, 0);
    }
    else {
        device_->GetDeviceContext()->Draw(sceneArrays_[arrayId].accessors[primitive.attributes[0].verticesAccessorId].count, 0);
    }
    return true;
}

bool SceneManager::PrepareTransparent(const std::vector<int>& sceneIndices) {
    if (excludeTransparent) {
        return true;
    }
    
    if (!!annotation_) {
        annotation_->BeginEvent(L"Prepare_transparent");
    }

    transparentPrimitives_.clear();

    ViewMatrixBuffer viewMatrix;
    viewMatrix.viewProjectionMatrix = camera_->GetViewProjectionMatrix();
    device_->GetDeviceContext()->UpdateSubresource(viewMatrixBuffer_, 0, nullptr, &viewMatrix, 0, 0);

    for (auto j : sceneIndices) {
        if (j < 0 || j >= scenes_.size()) {
            continue;
        }
        for (auto node : scenes_[j].rootNodes) {
            if (!PrepareTransparentForNode(scenes_[j].arraysId, node, scenes_[j].transformation)) {
                if (!!annotation_) {
                    annotation_->EndEvent();
                }
                return false;
            }
        }
    }
    if (!!annotation_) {
        annotation_->EndEvent();
    }
    return true;
}

bool SceneManager::PrepareTransparentForNode(int arrayId, int nodeId, const XMMATRIX& transformation) {
    const Node& node = sceneArrays_[arrayId].nodes[nodeId];
    XMMATRIX currentTransformation = XMMatrixMultiply(node.transformation, transformation);

    if (node.meshId >= 0) {
        for (auto& p : sceneArrays_[arrayId].meshes[node.meshId].transparentPrimitives) {
            if (!AddPrimitiveToTransparentPrimitives(arrayId, p, currentTransformation)) {
                return false;
            }
        }
    }

    for (auto c : node.children) {
        if (!PrepareTransparentForNode(arrayId, c, currentTransformation)) {
            return false;
        }
    }

    return true;
}

bool SceneManager::AddPrimitiveToTransparentPrimitives(int arrayId, const Primitive& primitive, const XMMATRIX& transformation) {
    TransparentPrimitive transparentPrimitive(primitive);
    transparentPrimitive.transformation = transformation;
    transparentPrimitive.arrayId = arrayId;

    device_->GetDeviceContext()->ClearDepthStencilView(transparentDepth_.DSV, D3D11_CLEAR_DEPTH, 0.0f, 0);
    device_->GetDeviceContext()->OMSetRenderTargets(0, nullptr, transparentDepth_.DSV);
    device_->GetDeviceContext()->OMSetDepthStencilState(transparentDepthStencilState_.get(), 0);
    viewport_.Width = width_;
    viewport_.Height = height_;
    device_->GetDeviceContext()->RSSetViewports(1, &viewport_);
    device_->GetDeviceContext()->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

    WorldMatrixBuffer worldMatrix;
    worldMatrix.worldMatrix = transformation;
    device_->GetDeviceContext()->UpdateSubresource(worldMatrixBuffer_, 0, nullptr, &worldMatrix, 0, 0);

    device_->GetDeviceContext()->IASetInputLayout(primitive.shadowVS->GetInputLayout().get());
    UINT numBuffers = primitive.attributes.size();
    std::vector<ID3D11Buffer*> vertexBuffers;
    std::vector<UINT> strides;
    std::vector<UINT> offsets;
    for (auto& a : primitive.attributes) {
        const BufferAccessor& accessor = sceneArrays_[arrayId].accessors[a.verticesAccessorId];
        vertexBuffers.push_back(accessor.buffer.get());
        strides.push_back(accessor.byteStride);
        offsets.push_back(accessor.byteOffset);
    }
    device_->GetDeviceContext()->IASetVertexBuffers(0, numBuffers, vertexBuffers.data(), strides.data(), offsets.data());
    device_->GetDeviceContext()->RSSetState(sceneArrays_[arrayId].materials[primitive.materialId].rasterizerState.get());
    device_->GetDeviceContext()->IASetPrimitiveTopology(primitive.mode);
    device_->GetDeviceContext()->VSSetShader(primitive.shadowVS->GetShader().get(), nullptr, 0);
    device_->GetDeviceContext()->VSSetConstantBuffers(0, 1, &worldMatrixBuffer_);
    device_->GetDeviceContext()->VSSetConstantBuffers(1, 1, &viewMatrixBuffer_);
    device_->GetDeviceContext()->PSSetShader(nullptr, nullptr, 0);
    if (primitive.indicesAccessorId >= 0) {
        const BufferAccessor& accessor = sceneArrays_[arrayId].accessors[primitive.indicesAccessorId];
        device_->GetDeviceContext()->IASetIndexBuffer(accessor.buffer.get(), accessor.format, accessor.byteOffset);
        device_->GetDeviceContext()->DrawIndexed(accessor.count, 0, 0);
    }
    else {
        device_->GetDeviceContext()->Draw(sceneArrays_[arrayId].accessors[primitive.attributes[0].verticesAccessorId].count, 0);
    }

    static float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    for (int i = n_; i >= 0; --i) {
        ID3D11RenderTargetView* views[] = { scaledFrames_[i].RTV };
        device_->GetDeviceContext()->OMSetRenderTargets(1, views, nullptr);
        device_->GetDeviceContext()->ClearRenderTargetView(scaledFrames_[i].RTV, color);

        ID3D11SamplerState* samplers[] = { samplerMax_.get() };
        device_->GetDeviceContext()->PSSetSamplers(0, 1, samplers);

        viewport_.Width = pow(2, i);
        viewport_.Height = pow(2, i);
        device_->GetDeviceContext()->RSSetViewports(1, &viewport_);

        ID3D11ShaderResourceView* resources[] = { i == n_ ? transparentDepth_.SRV : scaledFrames_[i + 1].SRV };
        device_->GetDeviceContext()->PSSetShaderResources(0, 1, resources);
        device_->GetDeviceContext()->RSSetState(nullptr);
        device_->GetDeviceContext()->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
        device_->GetDeviceContext()->IASetInputLayout(nullptr);
        device_->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        device_->GetDeviceContext()->VSSetShader(mappingVS_->GetShader().get(), nullptr, 0);
        device_->GetDeviceContext()->PSSetShader(downsamplePS_->GetShader().get(), nullptr, 0);
        device_->GetDeviceContext()->Draw(6, 0);
    }

    device_->GetDeviceContext()->CopyResource(readMaxTexture_, scaledFrames_[0].texture);
    D3D11_MAPPED_SUBRESOURCE ResourceDesc = {};
    HRESULT result = device_->GetDeviceContext()->Map(readMaxTexture_, 0, D3D11_MAP_READ, 0, &ResourceDesc);
    if (FAILED(result)) {
        return false;
    }

    if (ResourceDesc.pData) {
        float* pData = reinterpret_cast<float*>(ResourceDesc.pData);
        transparentPrimitive.distance = pData[0];
    }
    device_->GetDeviceContext()->Unmap(readMaxTexture_, 0);

    transparentPrimitives_.push_back(transparentPrimitive);

    return true;
}

bool SceneManager::Render(
    const std::shared_ptr<ID3D11ShaderResourceView>& irradianceMap,
    const std::shared_ptr<ID3D11ShaderResourceView>& prefilteredMap,
    const std::shared_ptr<ID3D11ShaderResourceView>& BRDF,
    const std::shared_ptr<Skybox>& skybox,
    const std::vector<PointLight>& lights,
    const std::vector<int>& sceneIndices
) {
    if (!IsInit() || !renderTarget_) {
        return false;
    }

    if (!!annotation_) {
        annotation_->BeginEvent(L"Preliminary_preparations");
    }

    if (currentMode_ == Mode::DEFAULT || currentMode_ == Mode::SHADOW_SPLITS) {
        if (!CreateShadowMaps(sceneIndices)) {
            if (!!annotation_) {
                annotation_->EndEvent();
            }
            return false;
        }
    }

    if (!PrepareTransparent(sceneIndices)) {
        if (!!annotation_) {
            annotation_->EndEvent();
        }
        return false;
    }

    static const FLOAT backColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    device_->GetDeviceContext()->ClearRenderTargetView(normals_.RTV, backColor);
    device_->GetDeviceContext()->ClearRenderTargetView(features_.RTV, backColor);
    device_->GetDeviceContext()->ClearRenderTargetView(color_.RTV, backColor);
    if (deferredRender) {
        device_->GetDeviceContext()->ClearRenderTargetView(emissive_.RTV, backColor);
    }
    device_->GetDeviceContext()->ClearDepthStencilView(depth_.DSV, D3D11_CLEAR_DEPTH, 0.0f, 0);

    device_->GetDeviceContext()->RSSetViewports(1, &renderTargetViewport_);

    if (deferredRender) {
        ID3D11RenderTargetView* rtv[] = { color_.RTV, features_.RTV, normals_.RTV, emissive_.RTV };
        device_->GetDeviceContext()->OMSetRenderTargets(4, rtv, depth_.DSV);
    }
    else {
        ID3D11RenderTargetView* rtv[] = { renderTarget_.get(), color_.RTV, features_.RTV, normals_.RTV };
        device_->GetDeviceContext()->OMSetRenderTargets(4, rtv, depth_.DSV);
    }

    if (!!annotation_) {
        annotation_->EndEvent();
        annotation_->BeginEvent(L"Draw_scene");
    }

    ViewMatrixBuffer viewBuffer;
    viewBuffer.viewProjectionMatrix = camera_->GetViewProjectionMatrix();
    device_->GetDeviceContext()->UpdateSubresource(viewMatrixBuffer_, 0, nullptr, &viewBuffer, 0, 0);

    XMFLOAT3 cameraPos = camera_->GetPosition();
    MatricesBuffer matricesBuffer;
    matricesBuffer.cameraPos = XMFLOAT4(cameraPos.x, cameraPos.y, cameraPos.z, 1.0f);
    matricesBuffer.projectionMatrix = camera_->GetProjectionMatrix();
    matricesBuffer.viewMatrix = camera_->GetViewMatrix();
    matricesBuffer.invProjectionMatrix = XMMatrixInverse(nullptr, matricesBuffer.projectionMatrix);
    matricesBuffer.invViewMatrix = XMMatrixInverse(nullptr, matricesBuffer.viewMatrix);
    device_->GetDeviceContext()->UpdateSubresource(matricesBuffer_, 0, nullptr, &matricesBuffer, 0, 0);

    if ((!excludeTransparent && !transparentPrimitives_.empty()) || !deferredRender) {
        ForwardRenderViewMatrixBuffer sceneBuffer;
        sceneBuffer.viewProjectionMatrix = camera_->GetViewProjectionMatrix();
        sceneBuffer.lightParams = XMINT4(int(lights.size()), 0, 0, 0);
        sceneBuffer.directionalLight = directionalLight_->GetInfo();
        for (int i = 0; i < lights.size() && i < MAX_LIGHT; ++i) {
            sceneBuffer.lights[i].pos = lights[i].pos;
            sceneBuffer.lights[i].color = lights[i].color;
        }
        device_->GetDeviceContext()->UpdateSubresource(forwardRenderViewMatrixBuffer_, 0, nullptr, &sceneBuffer, 0, 0);
    }

    if ((withSSAO && currentMode_ == Mode::DEFAULT) || currentMode_ == Mode::SSAO_MASK) {
        SSAOParamsBuffer buffer;
        buffer.parameters = XMFLOAT4(MAX_SSAO_SAMPLE_COUNT, SSAORadius, SSAODepthLimit, 0.0f);
        buffer.sizes = XMFLOAT4(width_, height_, 0.0f, 0.0f);
        for (int i = 0; i < MAX_SSAO_SAMPLE_COUNT; ++i) {
            buffer.samples[i] = SSAOSamples_[i];
        }
        for (int i = 0; i < NOISE_BUFFER_SIZE; ++i) {
            buffer.noise[i] = SSAONoise_[i];
        }
        device_->GetDeviceContext()->UpdateSubresource(SSAOParamsBuffer_, 0, nullptr, &buffer, 0, 0);
    }

    for (auto j : sceneIndices) {
        if (j < 0 || j >= scenes_.size()) {
            continue;
        }
        for (auto node : scenes_[j].rootNodes) {
            RenderNode(scenes_[j].arraysId, node, irradianceMap, prefilteredMap, BRDF, scenes_[j].transformation);
        }
    }

    device_->GetDeviceContext()->CopyResource(depthCopy_.texture, depth_.texture);
    ID3D11RenderTargetView* rtv[] = { renderTarget_.get() };
    device_->GetDeviceContext()->OMSetRenderTargets(1, rtv, depth_.DSV);

    if (currentMode_ == Mode::DEFAULT || currentMode_ == Mode::SSAO_MASK) {
        RenderAmbientLight(irradianceMap, prefilteredMap, BRDF);
    }

    if (deferredRender) {
        if (currentMode_ == Mode::DEFAULT || currentMode_ == Mode::SHADOW_SPLITS) {
            RenderDirectionalLight();
        }
        if (currentMode_ != Mode::SHADOW_SPLITS && currentMode_ != Mode::SSAO_MASK) {
            RenderPointLights(lights);
        }
    }

    if (!skybox->Render()) {
        if (!!annotation_) {
            annotation_->EndEvent();
        }
        return false;
    }

    if (!excludeTransparent) {
        RenderTransparent(irradianceMap, prefilteredMap, BRDF);
    }

    if (!!annotation_) {
        annotation_->EndEvent();
    }

    return true;
}

void SceneManager::RenderNode(
    int arrayId,
    int nodeId,
    const std::shared_ptr<ID3D11ShaderResourceView>& irradianceMap,
    const std::shared_ptr<ID3D11ShaderResourceView>& prefilteredMap,
    const std::shared_ptr<ID3D11ShaderResourceView>& BRDF,
    const XMMATRIX& transformation
) {
    const Node& node = sceneArrays_[arrayId].nodes[nodeId];
    XMMATRIX currentTransformation = XMMatrixMultiply(node.transformation, transformation);

    if (node.meshId >= 0) {
        const Mesh& mesh = sceneArrays_[arrayId].meshes[node.meshId];
        for (auto& p : mesh.opaquePrimitives) {
            RenderPrimitive(arrayId, p, irradianceMap, prefilteredMap, BRDF, currentTransformation);
        }
        for (auto& p : mesh.primitivesWithAlphaCutoff) {
            RenderPrimitive(arrayId, p, irradianceMap, prefilteredMap, BRDF, currentTransformation);
        }
    }

    for (auto c : node.children) {
        RenderNode(arrayId, c, irradianceMap, prefilteredMap, BRDF, currentTransformation);
    }
}

void SceneManager::RenderPrimitive(
    int arrayId,
    const Primitive& primitive,
    const std::shared_ptr<ID3D11ShaderResourceView>& irradianceMap,
    const std::shared_ptr<ID3D11ShaderResourceView>& prefilteredMap,
    const std::shared_ptr<ID3D11ShaderResourceView>& BRDF,
    const XMMATRIX& transformation,
    bool transparent
) {
    const Material& material = sceneArrays_[arrayId].materials[primitive.materialId];

    WorldMatrixBuffer worldMatrix;
    worldMatrix.worldMatrix = transformation;
    device_->GetDeviceContext()->UpdateSubresource(worldMatrixBuffer_, 0, nullptr, &worldMatrix, 0, 0);

    MaterialParamsBuffer materialBuffer;
    materialBuffer.baseColorFactor = material.baseColorFactor;
    materialBuffer.baseColorTA = XMINT4(material.baseColorTA.textureId, material.baseColorTA.texCoordId, material.baseColorTA.samplerId, material.baseColorTA.isSRGB);
    materialBuffer.emissiveFactorAlphaCutoff = XMFLOAT4(material.emissiveFactor.x, material.emissiveFactor.y, material.emissiveFactor.z, material.alphaCutoff);
    materialBuffer.emissiveTA = XMINT4(material.emissiveTA.textureId, material.emissiveTA.texCoordId, material.emissiveTA.samplerId, material.emissiveTA.isSRGB);
    materialBuffer.MRONFactors = XMFLOAT4(material.metallicFactor, material.roughnessFactor, material.occlusionStrength, material.normalScale);
    materialBuffer.normalTA = XMINT4(material.normalTA.textureId, material.normalTA.texCoordId, material.normalTA.samplerId, material.normalTA.isSRGB);
    materialBuffer.roughMetallicTA = XMINT4(material.roughMetallicTA.textureId, material.roughMetallicTA.texCoordId, material.roughMetallicTA.samplerId,
        material.roughMetallicTA.isSRGB);
    materialBuffer.occlusionTA = XMINT4(material.occlusionTA.textureId, material.occlusionTA.texCoordId, material.occlusionTA.samplerId, material.occlusionTA.isSRGB);
    device_->GetDeviceContext()->UpdateSubresource(materialParamsBuffer_, 0, nullptr, &materialBuffer, 0, 0);

    if ((transparent || !deferredRender) && (currentMode_ == Mode::DEFAULT || currentMode_ == Mode::SHADOW_SPLITS || currentMode_ == Mode::SSAO_MASK)) {
        std::vector<ID3D11SamplerState*> samplers = { };
        if (transparent) {
            samplers.push_back(samplerAvg_.get());
        }
        samplers.push_back(samplerPCF_.get());
        if (transparent && ((withSSAO && currentMode_ == Mode::DEFAULT) || currentMode_ == Mode::SSAO_MASK)) {
            samplers.push_back(generalSampler_.get());
        }
        device_->GetDeviceContext()->PSSetSamplers(transparent ? 0 : 1, samplers.size(), samplers.data());

        std::vector<ID3D11ShaderResourceView*> resources = { };
        if (transparent) {
            resources.push_back(irradianceMap.get());
            resources.push_back(prefilteredMap.get());
            resources.push_back(BRDF.get());
        }
        for (UINT i = 0; i < CSM_SPLIT_COUNT; ++i) {
            resources.push_back(shadowSplits_[i].SRV);
        }
        if (transparent && ((withSSAO && currentMode_ == Mode::DEFAULT) || currentMode_ == Mode::SSAO_MASK)) {
            resources.push_back(depthCopy_.SRV);
        }
        device_->GetDeviceContext()->PSSetShaderResources(transparent ? 0 : 3, resources.size(), resources.data());
    }
    int m = deferredRender && !transparent ? 0 : 3;
    int k = deferredRender && !transparent ? 0 : CSM_SPLIT_COUNT + 4;
    if (material.baseColorTA.textureId >= 0) {
        std::vector<ID3D11SamplerState*> samplers = { sceneArrays_[arrayId].samplers[material.baseColorTA.samplerId].get() };
        device_->GetDeviceContext()->PSSetSamplers(m++, samplers.size(), samplers.data());

        std::vector<ID3D11ShaderResourceView*> resources = { 
            sceneArrays_[arrayId].textures[material.baseColorTA.textureId]->GetSRV(material.baseColorTA.isSRGB).get()
        };
        device_->GetDeviceContext()->PSSetShaderResources(k++, resources.size(), resources.data());
    }
    else {
        device_->GetDeviceContext()->PSSetShaderResources(k++, 0, nullptr);
    }
    if (material.roughMetallicTA.textureId >= 0) {
        std::vector<ID3D11SamplerState*> samplers = { sceneArrays_[arrayId].samplers[material.roughMetallicTA.samplerId].get() };
        device_->GetDeviceContext()->PSSetSamplers(m++, samplers.size(), samplers.data());

        std::vector<ID3D11ShaderResourceView*> resources = {
            sceneArrays_[arrayId].textures[material.roughMetallicTA.textureId]->GetSRV(material.roughMetallicTA.isSRGB).get()
        };
        device_->GetDeviceContext()->PSSetShaderResources(k++, resources.size(), resources.data());
    }
    else {
        device_->GetDeviceContext()->PSSetShaderResources(k++, 0, nullptr);
    }
    if (material.normalTA.textureId >= 0) {
        std::vector<ID3D11SamplerState*> samplers = { sceneArrays_[arrayId].samplers[material.normalTA.samplerId].get() };
        device_->GetDeviceContext()->PSSetSamplers(m++, samplers.size(), samplers.data());

        std::vector<ID3D11ShaderResourceView*> resources = {
            sceneArrays_[arrayId].textures[material.normalTA.textureId]->GetSRV(material.normalTA.isSRGB).get()
        };
        device_->GetDeviceContext()->PSSetShaderResources(k++, resources.size(), resources.data());
    }
    else {
        device_->GetDeviceContext()->PSSetShaderResources(k++, 0, nullptr);
    }
    if (material.occlusionTA.textureId >= 0) {
        std::vector<ID3D11SamplerState*> samplers = { sceneArrays_[arrayId].samplers[material.occlusionTA.samplerId].get() };
        device_->GetDeviceContext()->PSSetSamplers(m++, samplers.size(), samplers.data());

        std::vector<ID3D11ShaderResourceView*> resources = {
            sceneArrays_[arrayId].textures[material.occlusionTA.textureId]->GetSRV(material.occlusionTA.isSRGB).get()
        };
        device_->GetDeviceContext()->PSSetShaderResources(k++, resources.size(), resources.data());
    }
    else {
        device_->GetDeviceContext()->PSSetShaderResources(k++, 0, nullptr);
    }
    if (material.emissiveTA.textureId >= 0) {
        std::vector<ID3D11SamplerState*> samplers = { sceneArrays_[arrayId].samplers[material.emissiveTA.samplerId].get() };
        device_->GetDeviceContext()->PSSetSamplers(m++, samplers.size(), samplers.data());

        std::vector<ID3D11ShaderResourceView*> resources = {
            sceneArrays_[arrayId].textures[material.emissiveTA.textureId]->GetSRV(material.emissiveTA.isSRGB).get()
        };
        device_->GetDeviceContext()->PSSetShaderResources(k++, resources.size(), resources.data());
    }
    else {
        device_->GetDeviceContext()->PSSetShaderResources(k++, 0, nullptr);
    }

    device_->GetDeviceContext()->OMSetDepthStencilState(material.depthStencilState.get(), 0);
    device_->GetDeviceContext()->RSSetState(material.rasterizerState.get());

    device_->GetDeviceContext()->IASetInputLayout(primitive.VS->GetInputLayout().get());
    UINT numBuffers = primitive.attributes.size();
    std::vector<ID3D11Buffer*> vertexBuffers;
    std::vector<UINT> strides;
    std::vector<UINT> offsets;
    for (auto& a : primitive.attributes) {
        const BufferAccessor& accessor = sceneArrays_[arrayId].accessors[a.verticesAccessorId];
        vertexBuffers.push_back(accessor.buffer.get());
        strides.push_back(accessor.byteStride);
        offsets.push_back(accessor.byteOffset);
    }
    device_->GetDeviceContext()->IASetVertexBuffers(0, numBuffers, vertexBuffers.data(), strides.data(), offsets.data());
    device_->GetDeviceContext()->IASetPrimitiveTopology(primitive.mode);
    device_->GetDeviceContext()->VSSetShader(primitive.VS->GetShader().get(), nullptr, 0);
    device_->GetDeviceContext()->VSSetConstantBuffers(0, 1, &worldMatrixBuffer_);
    device_->GetDeviceContext()->VSSetConstantBuffers(1, 1, &viewMatrixBuffer_);
    if (deferredRender && !transparent) {
        device_->GetDeviceContext()->PSSetShader(primitive.gBufferPS->GetShader().get(), nullptr, 0);
        device_->GetDeviceContext()->PSSetConstantBuffers(0, 1, &materialParamsBuffer_);
    }
    else {
        switch (currentMode_) {
        case Mode::FRESNEL:
            device_->GetDeviceContext()->PSSetShader(primitive.PSFresnel->GetShader().get(), nullptr, 0);
            break;
        case Mode::NDF:
            device_->GetDeviceContext()->PSSetShader(primitive.PSNdf->GetShader().get(), nullptr, 0);
            break;
        case Mode::GEOMETRY:
            device_->GetDeviceContext()->PSSetShader(primitive.PSGeometry->GetShader().get(), nullptr, 0);
            break;
        case Mode::SHADOW_SPLITS:
            device_->GetDeviceContext()->PSSetShader(primitive.PSShadowSplits->GetShader().get(), nullptr, 0);
            break;
        case Mode::SSAO_MASK:
            if (transparent) {
                device_->GetDeviceContext()->PSSetShader(primitive.transparentPSSSAOMask->GetShader().get(), nullptr, 0);
            }
            else {
                device_->GetDeviceContext()->PSSetShader(primitive.PSSSAOMask->GetShader().get(), nullptr, 0);
            }
            break;
        default:
            if (withSSAO && transparent) {
                device_->GetDeviceContext()->PSSetShader(primitive.transparentPSSSAO->GetShader().get(), nullptr, 0);
            }
            else {
                device_->GetDeviceContext()->PSSetShader(primitive.PSDefault->GetShader().get(), nullptr, 0);
            }
            break;
        }
        device_->GetDeviceContext()->PSSetConstantBuffers(0, 1, &materialParamsBuffer_);
        device_->GetDeviceContext()->PSSetConstantBuffers(1, 1, &forwardRenderViewMatrixBuffer_);
        device_->GetDeviceContext()->PSSetConstantBuffers(2, 1, &matricesBuffer_);
        if (transparent && ((withSSAO && currentMode_ == Mode::DEFAULT) || currentMode_ == Mode::SSAO_MASK)) {
            device_->GetDeviceContext()->PSSetConstantBuffers(3, 1, &SSAOParamsBuffer_);
        }
    }
    device_->GetDeviceContext()->OMSetBlendState(material.blendState.get(), nullptr, 0xFFFFFFFF);

    if (primitive.indicesAccessorId >= 0) {
        const BufferAccessor& accessor = sceneArrays_[arrayId].accessors[primitive.indicesAccessorId];
        device_->GetDeviceContext()->IASetIndexBuffer(accessor.buffer.get(), accessor.format, accessor.byteOffset);
        device_->GetDeviceContext()->DrawIndexed(accessor.count, 0, 0);
    }
    else {
        device_->GetDeviceContext()->Draw(sceneArrays_[arrayId].accessors[primitive.attributes[0].verticesAccessorId].count, 0);
    }
}

void SceneManager::RenderTransparent(
    const std::shared_ptr<ID3D11ShaderResourceView>& irradianceMap,
    const std::shared_ptr<ID3D11ShaderResourceView>& prefilteredMap,
    const std::shared_ptr<ID3D11ShaderResourceView>& BRDF
) {
    if (transparentPrimitives_.empty()) {
        return;
    }

    if (!!annotation_) {
        annotation_->BeginEvent(L"Render_transparent");
    }

    std::sort(transparentPrimitives_.begin(), transparentPrimitives_.end());

    for (auto& tp : transparentPrimitives_) {
        RenderPrimitive(tp.arrayId, tp.primitive, irradianceMap, prefilteredMap, BRDF, tp.transformation, true);
    }

    if (!!annotation_) {
        annotation_->EndEvent();
    }
}

void SceneManager::RenderAmbientLight(
    const std::shared_ptr<ID3D11ShaderResourceView>& irradianceMap,
    const std::shared_ptr<ID3D11ShaderResourceView>& prefilteredMap,
    const std::shared_ptr<ID3D11ShaderResourceView>& BRDF
) {
    if (!!annotation_) {
        annotation_->BeginEvent(L"Render_ambient_light");
    }

    std::vector<ID3D11SamplerState*> samplers = { generalSampler_.get() };
    std::vector<ID3D11ShaderResourceView*> resources = { color_.SRV, features_.SRV, normals_.SRV, depthCopy_.SRV };
    if (currentMode_ != Mode::SSAO_MASK) {
        samplers.push_back(samplerAvg_.get());

        resources.push_back(irradianceMap.get());
        resources.push_back(prefilteredMap.get());
        resources.push_back(BRDF.get());
    }
    device_->GetDeviceContext()->PSSetSamplers(0, samplers.size(), samplers.data());
    device_->GetDeviceContext()->PSSetShaderResources(0, resources.size(), resources.data());

    device_->GetDeviceContext()->OMSetDepthStencilState(generalDepthStencilState_.get(), 0);
    device_->GetDeviceContext()->RSSetState(generalRasterizerState_.get());
    device_->GetDeviceContext()->OMSetBlendState(generalBlendState_.get(), nullptr, 0xFFFFFFFF);
    device_->GetDeviceContext()->IASetInputLayout(mappingVS_->GetInputLayout().get());
    device_->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    device_->GetDeviceContext()->PSSetConstantBuffers(0, 1, &matricesBuffer_);
    if (currentMode_ == Mode::SSAO_MASK || withSSAO) {
        device_->GetDeviceContext()->PSSetConstantBuffers(1, 1, &SSAOParamsBuffer_);
    }
    device_->GetDeviceContext()->VSSetShader(mappingVS_->GetShader().get(), nullptr, 0);
    if (currentMode_ == Mode::SSAO_MASK) {
        device_->GetDeviceContext()->PSSetShader(ambientLightPSSSAOMask_->GetShader().get(), nullptr, 0);
    }
    else if (withSSAO) {
        device_->GetDeviceContext()->PSSetShader(ambientLightPSSSAO_->GetShader().get(), nullptr, 0);
    }
    else {
        device_->GetDeviceContext()->PSSetShader(ambientLightPS_->GetShader().get(), nullptr, 0);
    }
    device_->GetDeviceContext()->Draw(6, 0);

    if (!!annotation_) {
        annotation_->EndEvent();
    }
}

void SceneManager::RenderDirectionalLight() {
    if (!!annotation_) {
        annotation_->BeginEvent(L"Render_directional_light");
    }

    auto dirLightInfo = directionalLight_->GetInfo();
    DirectionalLightBuffer lightBuffer;
    lightBuffer.color = dirLightInfo.color;
    lightBuffer.direction = dirLightInfo.direction;
    lightBuffer.viewProjectionMatrix = dirLightInfo.viewProjectionMatrix;
    lightBuffer.splitSizeRatio = dirLightInfo.splitSizeRatio;
    device_->GetDeviceContext()->UpdateSubresource(directionalLightBuffer_, 0, nullptr, &lightBuffer, 0, 0);

    std::vector<ID3D11SamplerState*> samplers = { generalSampler_.get(), samplerPCF_.get() };
    std::vector<ID3D11ShaderResourceView*> resources = { color_.SRV, features_.SRV, normals_.SRV, emissive_.SRV, depthCopy_.SRV };
    for (UINT i = 0; i < CSM_SPLIT_COUNT; ++i) {
        resources.push_back(shadowSplits_[i].SRV);
    }
    device_->GetDeviceContext()->PSSetSamplers(0, samplers.size(), samplers.data());
    device_->GetDeviceContext()->PSSetShaderResources(0, resources.size(), resources.data());

    device_->GetDeviceContext()->OMSetDepthStencilState(generalDepthStencilState_.get(), 0);
    device_->GetDeviceContext()->RSSetState(generalRasterizerState_.get());
    device_->GetDeviceContext()->OMSetBlendState(generalBlendState_.get(), nullptr, 0xFFFFFFFF);
    device_->GetDeviceContext()->IASetInputLayout(mappingVS_->GetInputLayout().get());
    device_->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    device_->GetDeviceContext()->PSSetConstantBuffers(0, 1, &directionalLightBuffer_);
    device_->GetDeviceContext()->PSSetConstantBuffers(1, 1, &matricesBuffer_);
    device_->GetDeviceContext()->VSSetShader(mappingVS_->GetShader().get(), nullptr, 0);
    if (currentMode_ == Mode::SHADOW_SPLITS) {
        device_->GetDeviceContext()->PSSetShader(directionalLightPSShadowSplits_->GetShader().get(), nullptr, 0);
    }
    else {
        device_->GetDeviceContext()->PSSetShader(directionalLightPS_->GetShader().get(), nullptr, 0);
    }
    device_->GetDeviceContext()->Draw(6, 0);

    if (!!annotation_) {
        annotation_->EndEvent();
    }
}

void SceneManager::RenderPointLights(const std::vector<PointLight>& lights) {
    if (lights.empty()) {
        return;
    }

    if (!!annotation_) {
        annotation_->BeginEvent(L"Render_point_lights");
    }

    ID3D11SamplerState* samplers[] = { generalSampler_.get() };
    device_->GetDeviceContext()->PSSetSamplers(0, 1, samplers);

    ID3D11ShaderResourceView* resources[] = { color_.SRV, features_.SRV, normals_.SRV, depthCopy_.SRV };
    device_->GetDeviceContext()->PSSetShaderResources(0, 4, resources);
    device_->GetDeviceContext()->OMSetDepthStencilState(pointLightDepthStencilState_.get(), 0);
    device_->GetDeviceContext()->RSSetState(pointLightRasterizerState_.get());
    device_->GetDeviceContext()->OMSetBlendState(generalBlendState_.get(), nullptr, 0xFFFFFFFF);

    device_->GetDeviceContext()->IASetIndexBuffer(sphereIndexBuffer_, DXGI_FORMAT_R32_UINT, 0);
    ID3D11Buffer* vertexBuffers[] = { sphereVertexBuffer_ };
    UINT strides[] = { sizeof(SphereVertex) };
    UINT offsets[] = { 0 };
    device_->GetDeviceContext()->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    device_->GetDeviceContext()->IASetInputLayout(pointLightVS_->GetInputLayout().get());
    device_->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    device_->GetDeviceContext()->VSSetShader(pointLightVS_->GetShader().get(), nullptr, 0);
    switch (currentMode_) {
    case Mode::DEFAULT:
        device_->GetDeviceContext()->PSSetShader(pointLightPS_->GetShader().get(), nullptr, 0);
        break;
    case Mode::FRESNEL:
        device_->GetDeviceContext()->PSSetShader(pointLightPSFresnel_->GetShader().get(), nullptr, 0);
        break;
    case Mode::NDF:
        device_->GetDeviceContext()->PSSetShader(pointLightPSNDF_->GetShader().get(), nullptr, 0);
        break;
    case Mode::GEOMETRY:
        device_->GetDeviceContext()->PSSetShader(pointLightPSGeometry_->GetShader().get(), nullptr, 0);
        break;
    default:
        break;
    }
    device_->GetDeviceContext()->VSSetConstantBuffers(1, 1, &viewMatrixBuffer_);
    device_->GetDeviceContext()->PSSetConstantBuffers(0, 1, &matricesBuffer_);

    InstancingWorldMatrixBuffer worldMatrix;
    LightBuffer lightBuffer;
    int i = 0;
    for (auto& l : lights) {
        float radius = sqrt(l.color.w / clampRadianceLimit);
        XMMATRIX transformation = XMMatrixIdentity();
        transformation = XMMatrixMultiply(XMMatrixTranslation(l.pos.x, l.pos.y, l.pos.z), transformation);
        transformation = XMMatrixMultiply(XMMatrixScaling(radius, radius, radius), transformation);

        worldMatrix.worldMatrix[i] = transformation;

        lightBuffer.color[i] = l.color;
        lightBuffer.pos[i] = l.pos;
        lightBuffer.parameters[i] = XMFLOAT4(radius, smoothClampRadianceLimit, clampRadianceLimit, 0.0f);

        ++i;
    }
    device_->GetDeviceContext()->UpdateSubresource(instancingWorldMatrixBuffer_, 0, nullptr, &worldMatrix, 0, 0);
    device_->GetDeviceContext()->UpdateSubresource(lightBuffer_, 0, nullptr, &lightBuffer, 0, 0);
    device_->GetDeviceContext()->VSSetConstantBuffers(0, 1, &instancingWorldMatrixBuffer_);
    device_->GetDeviceContext()->PSSetConstantBuffers(1, 1, &lightBuffer_);

    device_->GetDeviceContext()->DrawIndexedInstanced(sphereNumIndices_, (UINT)lights.size(), 0, 0, 0);
    if (!!annotation_) {
        annotation_->EndEvent();
    }
}

bool SceneManager::Resize(int width, int height) {
    if (!IsInit()) {
        return false;
    }

    width_ = width;
    height_ = height;

    n_ = 0;
    int minSide = min(width, height);
    while (minSide >>= 1) {
        ++n_;
    }

    SAFE_RELEASE(depthCopy_.texture);
    SAFE_RELEASE(depthCopy_.DSV);
    SAFE_RELEASE(depthCopy_.SRV);

    SAFE_RELEASE(transparentDepth_.texture);
    SAFE_RELEASE(transparentDepth_.DSV);
    SAFE_RELEASE(transparentDepth_.SRV);

    SAFE_RELEASE(depth_.texture);
    SAFE_RELEASE(depth_.DSV);
    SAFE_RELEASE(depth_.SRV);

    SAFE_RELEASE(normals_.texture);
    SAFE_RELEASE(normals_.SRV);
    SAFE_RELEASE(normals_.RTV);

    SAFE_RELEASE(features_.texture);
    SAFE_RELEASE(features_.SRV);
    SAFE_RELEASE(features_.RTV);

    SAFE_RELEASE(color_.texture);
    SAFE_RELEASE(color_.SRV);
    SAFE_RELEASE(color_.RTV);

    SAFE_RELEASE(emissive_.texture);
    SAFE_RELEASE(emissive_.SRV);
    SAFE_RELEASE(emissive_.RTV);

    for (int i = 0; i < scaledFrames_.size(); ++i) {
        SAFE_RELEASE(scaledFrames_[i].texture);
        SAFE_RELEASE(scaledFrames_[i].SRV);
        SAFE_RELEASE(scaledFrames_[i].RTV);
    }
    scaledFrames_.clear();

    HRESULT result = CreateDepthStencilView(width, height, depth_);
    if (SUCCEEDED(result)) {
        result = CreateDepthStencilView(width, height, depthCopy_);
    }
    if (SUCCEEDED(result)) {
        result = CreateDepthStencilView(width, height, transparentDepth_);
    }
    if (SUCCEEDED(result)) {
        result = CreateTexture(normals_, width, height);
    }
    if (SUCCEEDED(result)) {
        result = CreateTexture(features_, width, height);
    }
    if (SUCCEEDED(result)) {
        result = CreateTexture(color_, width, height);
    }
    if (SUCCEEDED(result)) {
        result = CreateTexture(emissive_, width, height);
    }
    for (int i = 0; i < n_ + 1 && SUCCEEDED(result); ++i) {
        RawPtrTexture texture;
        result = CreateTexture(texture, i);
        if (SUCCEEDED(result)) {
            scaledFrames_.push_back(texture);
        }
    }

    return SUCCEEDED(result);
}

void SceneManager::Cleanup() {
    device_.reset();
    managerStorage_.reset();
    camera_.reset();
    directionalLight_.reset();
    samplerAvg_.reset();
    shadowDepthStencilState_.reset();
    samplerPCF_.reset();
    mappingVS_.reset();
    downsamplePS_.reset();
    transparentDepthStencilState_.reset();
    samplerMax_.reset();
    renderTarget_.reset();
    annotation_.reset();
    pointLightVS_.reset();
    pointLightPS_.reset();
    pointLightPSFresnel_.reset();
    pointLightPSNDF_.reset();
    pointLightPSGeometry_.reset();
    pointLightDepthStencilState_.reset();
    generalDepthStencilState_.reset();
    pointLightRasterizerState_.reset();
    generalRasterizerState_.reset();
    generalSampler_.reset();
    generalBlendState_.reset();
    directionalLightPS_.reset();
    directionalLightPSShadowSplits_.reset();
    ambientLightPS_.reset();
    ambientLightPSSSAO_.reset();
    ambientLightPSSSAOMask_.reset();

    SAFE_RELEASE(normals_.texture);
    SAFE_RELEASE(normals_.SRV);
    SAFE_RELEASE(normals_.RTV);

    SAFE_RELEASE(features_.texture);
    SAFE_RELEASE(features_.SRV);
    SAFE_RELEASE(features_.RTV);

    SAFE_RELEASE(color_.texture);
    SAFE_RELEASE(color_.SRV);
    SAFE_RELEASE(color_.RTV);

    SAFE_RELEASE(emissive_.texture);
    SAFE_RELEASE(emissive_.SRV);
    SAFE_RELEASE(emissive_.RTV);

    SAFE_RELEASE(depthCopy_.texture);
    SAFE_RELEASE(depthCopy_.DSV);
    SAFE_RELEASE(depthCopy_.SRV);

    SAFE_RELEASE(transparentDepth_.texture);
    SAFE_RELEASE(transparentDepth_.DSV);
    SAFE_RELEASE(transparentDepth_.SRV);

    SAFE_RELEASE(depth_.texture);
    SAFE_RELEASE(depth_.DSV);
    SAFE_RELEASE(depth_.SRV);

    SAFE_RELEASE(sphereVertexBuffer_);
    SAFE_RELEASE(sphereIndexBuffer_);

    SAFE_RELEASE(worldMatrixBuffer_);
    SAFE_RELEASE(instancingWorldMatrixBuffer_);
    SAFE_RELEASE(viewMatrixBuffer_);
    SAFE_RELEASE(shadowMapAlphaCutoffBuffer_);
    SAFE_RELEASE(lightBuffer_);
    SAFE_RELEASE(directionalLightBuffer_);
    SAFE_RELEASE(materialParamsBuffer_);
    SAFE_RELEASE(forwardRenderViewMatrixBuffer_);
    SAFE_RELEASE(matricesBuffer_);
    SAFE_RELEASE(SSAOParamsBuffer_);

    SAFE_RELEASE(readMaxTexture_);

    for (int i = 0; i < shadowSplits_.size(); ++i) {
        SAFE_RELEASE(shadowSplits_[i].texture);
        SAFE_RELEASE(shadowSplits_[i].DSV);
        SAFE_RELEASE(shadowSplits_[i].SRV);
    }
    shadowSplits_.clear();

    for (int i = 0; i < scaledFrames_.size(); ++i) {
        SAFE_RELEASE(scaledFrames_[i].texture);
        SAFE_RELEASE(scaledFrames_[i].SRV);
        SAFE_RELEASE(scaledFrames_[i].RTV);
    }
    scaledFrames_.clear();

    scenes_.clear();
    sceneArrays_.clear();
    transparentPrimitives_.clear();

    isInit_ = false;
}