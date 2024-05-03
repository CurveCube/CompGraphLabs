#include "Scene.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tinygltf/tiny_gltf.h"
#undef TINYGLTF_IMPLEMENTATION
#undef STB_IMAGE_WRITE_IMPLEMENTATION

SceneManager::SceneManager() {
    for (UINT i = 0; i < CSM_SPLIT_COUNT; ++i) {
        shadowBuffers_[i] = nullptr;
        shadowDSViews_[i] = nullptr;
        shadowSRViews_[i] = nullptr;
    }

    shadowMapViewport_.TopLeftX = 0;
    shadowMapViewport_.TopLeftY = 0;
    shadowMapViewport_.Width = shadowMapSize;
    shadowMapViewport_.Height = shadowMapSize;
    shadowMapViewport_.MinDepth = 0.0f;
    shadowMapViewport_.MaxDepth = 1.0f;

    transparentViewport_.TopLeftX = 0;
    transparentViewport_.TopLeftY = 0;
    transparentViewport_.Width = shadowMapSize;
    transparentViewport_.Height = shadowMapSize;
    transparentViewport_.MinDepth = 0.0f;
    transparentViewport_.MaxDepth = 1.0f;

    downsampleViewport_.TopLeftX = 0;
    downsampleViewport_.TopLeftY = 0;
    downsampleViewport_.Width = 1.0f;
    downsampleViewport_.Height = 1.0f;
    downsampleViewport_.MinDepth = 0.0f;
    downsampleViewport_.MaxDepth = 1.0f;

    depthPrepathViewport_.TopLeftX = 0;
    depthPrepathViewport_.TopLeftY = 0;
    depthPrepathViewport_.Width = 1.0f;
    depthPrepathViewport_.Height = 1.0f;
    depthPrepathViewport_.MinDepth = 0.0f;
    depthPrepathViewport_.MaxDepth = 1.0f;
}

HRESULT SceneManager::Init(const std::shared_ptr<Device>& device, const std::shared_ptr<ManagerStorage>& managerStorage,
    const std::shared_ptr<Camera>& camera, const std::shared_ptr<DirectionalLight>& directionalLight, int width, int height) {
    if (!managerStorage->IsInit() || !device->IsInit()) {
        return E_FAIL;
    }

    device_ = device;
    managerStorage_ = managerStorage;
    camera_ = camera;
    directionalLight_ = directionalLight;

    transparentViewport_.Width = width;
    transparentViewport_.Height = height;
    width_ = width;
    height_ = height;

    HRESULT result = S_OK;
    for (UINT i = 0; i < CSM_SPLIT_COUNT && SUCCEEDED(result); ++i) {
        result = CreateDepthStencilView(shadowMapSize, shadowMapSize, &shadowBuffers_[i], &shadowDSViews_[i], &shadowSRViews_[i]);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetStateManager()->CreateDepthStencilState(shadowDepthStencilState_, D3D11_COMPARISON_LESS);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetStateManager()->CreateDepthStencilState(depthPrepathDepthStencilState_, D3D11_COMPARISON_GREATER);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetStateManager()->CreateSamplerState(samplerPCF_, D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP,
            D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_COMPARISON_LESS);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetStateManager()->CreateSamplerState(samplerAvg_, D3D11_FILTER_MIN_MAG_MIP_LINEAR,
            D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP);
    }
    if (SUCCEEDED(result)) {
        result = CreateAuxiliaryForTransparent(width, height);
    }
    if (SUCCEEDED(result)) {
        result = CreateDepthStencilView(width, height, &depthBuffer_, &depthDSView_, &depthSRView_);
    }
    if (SUCCEEDED(result)) {
        result = CreateBuffers();
    }

    return result;
}

HRESULT SceneManager::CreateDepthStencilView(int width, int height, ID3D11Texture2D** buffer, ID3D11DepthStencilView** DSV, ID3D11ShaderResourceView** SRV) {
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
    HRESULT result = device_->GetDevice()->CreateTexture2D(&desc, nullptr, buffer);
    if (SUCCEEDED(result)) {
        D3D11_DEPTH_STENCIL_VIEW_DESC desc = {};
        desc.Format = DXGI_FORMAT_D32_FLOAT;
        desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipSlice = 0;
        desc.Flags = 0;
        result = device_->GetDevice()->CreateDepthStencilView(*buffer, &desc, DSV);
    }
    if (SUCCEEDED(result)) {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
        desc.Format = DXGI_FORMAT_R32_FLOAT;
        desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipLevels = 1;
        desc.Texture2D.MostDetailedMip = 0;
        result = device_->GetDevice()->CreateShaderResourceView(*buffer, &desc, SRV);
    }
    return result;
}

HRESULT SceneManager::CreateAuxiliaryForTransparent(int width, int height) {
    n = 0;
    int minSide = min(width, height);
    while (minSide >>= 1) {
        n++;
    }

    HRESULT result = managerStorage_->GetVSManager()->LoadShader(mappingVS_, L"shaders/mappingVS.hlsl");
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(downsamplePS_, L"shaders/downsampleOnlyMaxPS.hlsl");
    }
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
        result = CreateDepthStencilView(width, height, &transparentDepthBuffer_, &transparentDSView_, &transparentSRView_);
    }
    for (int i = 0; i < n + 1 && SUCCEEDED(result); ++i) {
        RawPtrTexture texture;
        result = CreateTexture(texture, i);
        if (SUCCEEDED(result)) {
            scaledFrames_.push_back(texture);
        }
    }

    return result;
}

HRESULT SceneManager::CreateTexture(RawPtrTexture& texture, int i) {
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = pow(2, i);
    textureDesc.Height = pow(2, i);
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R32_FLOAT;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;
    HRESULT result = device_->GetDevice()->CreateTexture2D(&textureDesc, nullptr, &texture.texture);
    if (SUCCEEDED(result)) {
        D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
        renderTargetViewDesc.Format = DXGI_FORMAT_R32_FLOAT;
        renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        renderTargetViewDesc.Texture2D.MipSlice = 0;

        result = device_->GetDevice()->CreateRenderTargetView(texture.texture, &renderTargetViewDesc, &texture.RTV);
    }
    if (SUCCEEDED(result)) {
        D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
        shaderResourceViewDesc.Format = DXGI_FORMAT_R32_FLOAT;
        shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
        shaderResourceViewDesc.Texture2D.MipLevels = 1;

        result = device_->GetDevice()->CreateShaderResourceView(texture.texture, &shaderResourceViewDesc, &texture.SRV);
    }

    return result;
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
        desc.ByteWidth = sizeof(ShadowMapViewMatrixBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;
        result = device_->GetDevice()->CreateBuffer(&desc, nullptr, &shadowMapViewMatrixBuffer_);
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

    HRESULT result = managerStorage_->GetVSManager()->LoadShader(primitive.VS, L"shaders/VS.hlsl", baseDefines, desc);
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetVSManager()->LoadShader(primitive.shadowVS, L"shaders/shadowVS.hlsl", baseDefines, desc);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(primitive.PSDefault, L"shaders/PS.hlsl", defaulMacros);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(primitive.PSFresnel, L"shaders/PS.hlsl", fresnelMacros);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(primitive.PSNdf, L"shaders/PS.hlsl", ndfMacros);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(primitive.PSGeometry, L"shaders/PS.hlsl", geometryMacros);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(primitive.PSShadowSplits, L"shaders/PS.hlsl", shadowSplitsMacros);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(primitive.PSSSAO, L"shaders/PS.hlsl", SSAOMacros);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(primitive.PSSSAOMask, L"shaders/PS.hlsl", SSAOMaskMacros);
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
    if (!IsInit()) {
        return false;
    }

    for (UINT i = 0; i < CSM_SPLIT_COUNT; ++i) {
        device_->GetDeviceContext()->ClearDepthStencilView(shadowDSViews_[i], D3D11_CLEAR_DEPTH, 1.0f, 0);
        device_->GetDeviceContext()->OMSetRenderTargets(0, nullptr, shadowDSViews_[i]);
        device_->GetDeviceContext()->OMSetDepthStencilState(shadowDepthStencilState_.get(), 0);
        device_->GetDeviceContext()->RSSetViewports(1, &shadowMapViewport_);
        device_->GetDeviceContext()->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

        ShadowMapViewMatrixBuffer viewMatrix;
        viewMatrix.viewProjectionMatrix = directionalLight_->viewProjectionMatrices[i];
        device_->GetDeviceContext()->UpdateSubresource(shadowMapViewMatrixBuffer_, 0, nullptr, &viewMatrix, 0, 0);

        for (auto j : sceneIndices) {
            if (j < 0 || j >= scenes_.size()) {
                continue;
            }
            for (auto node : scenes_[j].rootNodes) {
                if (!CreateShadowMapForNode(scenes_[j].arraysId, node, scenes_[j].transformation)) {
                    return false;
                }
            }
        }
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
        if (!excludeTransparent_) {
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
    HRESULT result = managerStorage_->GetStateManager()->CreateRasterizerState(rasterizerState, D3D11_FILL_SOLID, material.cullMode, depthBias_, slopeScaleBias_);
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
    device_->GetDeviceContext()->VSSetConstantBuffers(1, 1, &shadowMapViewMatrixBuffer_);

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
    if (!IsInit()) {
        return false;
    }
    if (excludeTransparent_) {
        return true;
    }

    transparentPrimitives_.clear();

    ShadowMapViewMatrixBuffer viewMatrix;
    viewMatrix.viewProjectionMatrix = camera_->GetViewProjectionMatrix();
    device_->GetDeviceContext()->UpdateSubresource(shadowMapViewMatrixBuffer_, 0, nullptr, &viewMatrix, 0, 0);

    for (auto j : sceneIndices) {
        if (j < 0 || j >= scenes_.size()) {
            continue;
        }
        for (auto node : scenes_[j].rootNodes) {
            if (!PrepareTransparentForNode(scenes_[j].arraysId, node, scenes_[j].transformation)) {
                return false;
            }
        }
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

    device_->GetDeviceContext()->ClearDepthStencilView(transparentDSView_, D3D11_CLEAR_DEPTH, 0.0f, 0);
    device_->GetDeviceContext()->OMSetRenderTargets(0, nullptr, transparentDSView_);
    device_->GetDeviceContext()->OMSetDepthStencilState(transparentDepthStencilState_.get(), 0);
    device_->GetDeviceContext()->RSSetViewports(1, &transparentViewport_);
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
    device_->GetDeviceContext()->VSSetConstantBuffers(1, 1, &shadowMapViewMatrixBuffer_);
    device_->GetDeviceContext()->PSSetShader(nullptr, nullptr, 0);
    if (primitive.indicesAccessorId >= 0) {
        const BufferAccessor& accessor = sceneArrays_[arrayId].accessors[primitive.indicesAccessorId];
        device_->GetDeviceContext()->IASetIndexBuffer(accessor.buffer.get(), accessor.format, accessor.byteOffset);
        device_->GetDeviceContext()->DrawIndexed(accessor.count, 0, 0);
    }
    else {
        device_->GetDeviceContext()->Draw(sceneArrays_[arrayId].accessors[primitive.attributes[0].verticesAccessorId].count, 0);
    }

    static float color[4] = { 0.25f, 0.25f, 0.25f, 1.0f };
    for (int i = n; i >= 0; --i) {
        ID3D11RenderTargetView* views[] = { scaledFrames_[i].RTV };
        device_->GetDeviceContext()->OMSetRenderTargets(1, views, nullptr);
        device_->GetDeviceContext()->ClearRenderTargetView(scaledFrames_[i].RTV, color);

        ID3D11SamplerState* samplers[] = { samplerMax_.get() };
        device_->GetDeviceContext()->PSSetSamplers(0, 1, samplers);

        downsampleViewport_.Width = pow(2, i);
        downsampleViewport_.Height = pow(2, i);
        device_->GetDeviceContext()->RSSetViewports(1, &downsampleViewport_);

        ID3D11ShaderResourceView* resources[] = { i == n ? transparentSRView_ : scaledFrames_[i + 1].SRV };
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
    const std::vector<SpotLight>& lights,
    const std::vector<int>& sceneIndices
) {
    if (!IsInit()) {
        return false;
    }

    XMFLOAT3 cameraPos = camera_->GetPosition();
    ViewMatrixBuffer sceneBuffer;
    sceneBuffer.viewProjectionMatrix = camera_->GetViewProjectionMatrix();
    sceneBuffer.cameraPos = XMFLOAT4(cameraPos.x, cameraPos.y, cameraPos.z, 1.0f);
    sceneBuffer.lightParams = XMINT4(int(lights.size()), 0, 0, 0);
    sceneBuffer.directionalLight = directionalLight_->GetInfo();
    for (int i = 0; i < lights.size() && i < MAX_LIGHT; ++i) {
        sceneBuffer.lights[i].pos = lights[i].pos;
        sceneBuffer.lights[i].color = lights[i].color;
    }
    device_->GetDeviceContext()->UpdateSubresource(viewMatrixBuffer_, 0, nullptr, &sceneBuffer, 0, 0);

    if (withSSAO_) {
        SSAOParamsBuffer buffer;
        buffer.parameters = XMFLOAT4(MAX_SSAO_SAMPLE_COUNT, SSAORadius_, SSAODepthLimit_, 0.0f);
        buffer.sizes = XMFLOAT4(width_, height_, 0.0f, 0.0f);
        buffer.projectionMatrix = camera_->GetProjectionMatrix();
        buffer.viewMatrix = camera_->GetViewMatrix();
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

    if (!skybox->Render()) {
        return false;
    }

    if (!excludeTransparent_) {
        RenderTransparent(irradianceMap, prefilteredMap, BRDF);
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
    const XMMATRIX& transformation
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

    if (currentMode_ == DEFAULT || currentMode_ == SHADOW_SPLITS || currentMode_ == SSAO_MASK) {
        std::vector<ID3D11SamplerState*> samplers = { };
        samplers.push_back(samplerAvg_.get());
        samplers.push_back(samplerPCF_.get());
        device_->GetDeviceContext()->PSSetSamplers(0, samplers.size(), samplers.data());

        std::vector<ID3D11ShaderResourceView*> resources = { };
        resources.push_back(irradianceMap.get());
        resources.push_back(prefilteredMap.get());
        resources.push_back(BRDF.get());
        for (UINT i = 0; i < CSM_SPLIT_COUNT; ++i) {
            resources.push_back(shadowSRViews_[i]);
        }
        if (withSSAO_ && (currentMode_ == DEFAULT || currentMode_ == SSAO_MASK)) {
            resources.push_back(depthSRView_);
        }
        device_->GetDeviceContext()->PSSetShaderResources(0, resources.size(), resources.data());
    }
    int m = 2;
    int k = CSM_SPLIT_COUNT + 4;
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
    switch (currentMode_) {
    case FRESNEL:
        device_->GetDeviceContext()->PSSetShader(primitive.PSFresnel->GetShader().get(), nullptr, 0);
        break;
    case NDF:
        device_->GetDeviceContext()->PSSetShader(primitive.PSNdf->GetShader().get(), nullptr, 0);
        break;
    case GEOMETRY:
        device_->GetDeviceContext()->PSSetShader(primitive.PSGeometry->GetShader().get(), nullptr, 0);
        break;
    case SHADOW_SPLITS:
        device_->GetDeviceContext()->PSSetShader(primitive.PSShadowSplits->GetShader().get(), nullptr, 0);
        break;
    case SSAO_MASK:
        device_->GetDeviceContext()->PSSetShader(primitive.PSSSAOMask->GetShader().get(), nullptr, 0);
        break;
    default:
        if (withSSAO_) {
            device_->GetDeviceContext()->PSSetShader(primitive.PSSSAO->GetShader().get(), nullptr, 0);
        }
        else {
            device_->GetDeviceContext()->PSSetShader(primitive.PSDefault->GetShader().get(), nullptr, 0);
        }
        break;
    }
    device_->GetDeviceContext()->PSSetConstantBuffers(0, 1, &materialParamsBuffer_);
    device_->GetDeviceContext()->PSSetConstantBuffers(1, 1, &viewMatrixBuffer_);
    if (withSSAO_) {
        device_->GetDeviceContext()->PSSetConstantBuffers(2, 1, &SSAOParamsBuffer_);
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

    std::sort(transparentPrimitives_.begin(), transparentPrimitives_.end());

    for (auto& tp : transparentPrimitives_) {
        RenderPrimitive(tp.arrayId, tp.primitive, irradianceMap, prefilteredMap, BRDF, tp.transformation);
    }
}

bool SceneManager::MakeDepthPrepath(const std::vector<int>& sceneIndices) {
    if (!IsInit()) {
        return false;
    }

    device_->GetDeviceContext()->ClearDepthStencilView(depthDSView_, D3D11_CLEAR_DEPTH, 0.0f, 0);
    device_->GetDeviceContext()->OMSetRenderTargets(0, nullptr, depthDSView_);
    device_->GetDeviceContext()->OMSetDepthStencilState(depthPrepathDepthStencilState_.get(), 0);
    device_->GetDeviceContext()->RSSetViewports(1, &depthPrepathViewport_);
    device_->GetDeviceContext()->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

    ShadowMapViewMatrixBuffer viewMatrix;
    viewMatrix.viewProjectionMatrix = camera_->GetViewProjectionMatrix();
    device_->GetDeviceContext()->UpdateSubresource(shadowMapViewMatrixBuffer_, 0, nullptr, &viewMatrix, 0, 0);

    for (auto j : sceneIndices) {
        if (j < 0 || j >= scenes_.size()) {
            continue;
        }
        for (auto node : scenes_[j].rootNodes) {
            MakeDepthPrepathForNode(scenes_[j].arraysId, node, scenes_[j].transformation);
        }
    }
    return true;
}

void SceneManager::MakeDepthPrepathForNode(int arrayId, int nodeId, const XMMATRIX& transformation) {
    const Node& node = sceneArrays_[arrayId].nodes[nodeId];
    XMMATRIX currentTransformation = XMMatrixMultiply(node.transformation, transformation);

    if (node.meshId >= 0) {
        const Mesh& mesh = sceneArrays_[arrayId].meshes[node.meshId];
        for (auto& p : mesh.opaquePrimitives) {
            MakeDepthPrepathForPrimitive(arrayId, p, AlphaMode::OPAQUE_MODE, currentTransformation);
        }
        if (!excludeTransparent_) {
            for (auto& p : mesh.transparentPrimitives) {
                MakeDepthPrepathForPrimitive(arrayId, p, AlphaMode::ALPHA_CUTOFF_MODE, currentTransformation);
            }
        }
        for (auto& p : mesh.primitivesWithAlphaCutoff) {
            MakeDepthPrepathForPrimitive(arrayId, p, AlphaMode::ALPHA_CUTOFF_MODE, currentTransformation);
        }
    }

    for (auto c : node.children) {
        MakeDepthPrepathForNode(arrayId, c, currentTransformation);
    }
}

void SceneManager::MakeDepthPrepathForPrimitive(int arrayId, const Primitive& primitive, AlphaMode mode, const XMMATRIX& transformation) {
    const Material& material = sceneArrays_[arrayId].materials[primitive.materialId];

    WorldMatrixBuffer worldMatrix;
    worldMatrix.worldMatrix = transformation;
    device_->GetDeviceContext()->UpdateSubresource(worldMatrixBuffer_, 0, nullptr, &worldMatrix, 0, 0);

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
    device_->GetDeviceContext()->RSSetState(material.rasterizerState.get());
    device_->GetDeviceContext()->IASetPrimitiveTopology(primitive.mode);
    device_->GetDeviceContext()->VSSetShader(primitive.shadowVS->GetShader().get(), nullptr, 0);
    device_->GetDeviceContext()->VSSetConstantBuffers(0, 1, &worldMatrixBuffer_);
    device_->GetDeviceContext()->VSSetConstantBuffers(1, 1, &shadowMapViewMatrixBuffer_);

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
}

void SceneManager::SetMode(Mode mode) {
    currentMode_ = mode;
}

bool SceneManager::Resize(int width, int height) {
    if (!IsInit()) {
        return false;
    }
    
    width_ = width;
    height_ = height;

    n = 0;
    int minSide = min(width, height);
    while (minSide >>= 1) {
        n++;
    }

    SAFE_RELEASE(transparentDepthBuffer_);
    SAFE_RELEASE(transparentDSView_);
    SAFE_RELEASE(transparentSRView_);
    SAFE_RELEASE(depthBuffer_);
    SAFE_RELEASE(depthDSView_);
    SAFE_RELEASE(depthSRView_);

    for (int i = 0; i < scaledFrames_.size(); ++i) {
        SAFE_RELEASE(scaledFrames_[i].texture);
        SAFE_RELEASE(scaledFrames_[i].SRV);
        SAFE_RELEASE(scaledFrames_[i].RTV);
    }
    scaledFrames_.clear();

    transparentViewport_.Width = width;
    transparentViewport_.Height = height;

    depthPrepathViewport_.Width = width;
    depthPrepathViewport_.Height = height;

    HRESULT result = CreateDepthStencilView(width, height, &transparentDepthBuffer_, &transparentDSView_, &transparentSRView_);
    if (SUCCEEDED(result)) {
        result = CreateDepthStencilView(width, height, &depthBuffer_, &depthDSView_, &depthSRView_);
    }
    for (int i = 0; i < n + 1 && SUCCEEDED(result); ++i) {
        RawPtrTexture texture;
        result = CreateTexture(texture, i);
        if (SUCCEEDED(result)) {
            scaledFrames_.push_back(texture);
        }
    }

    return SUCCEEDED(result);
}

bool SceneManager::IsInit() const {
    return !!SSAOParamsBuffer_;
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
    depthPrepathDepthStencilState_.reset();

    SAFE_RELEASE(worldMatrixBuffer_);
    SAFE_RELEASE(viewMatrixBuffer_);
    SAFE_RELEASE(materialParamsBuffer_);
    SAFE_RELEASE(shadowMapViewMatrixBuffer_);
    SAFE_RELEASE(shadowMapAlphaCutoffBuffer_);
    SAFE_RELEASE(SSAOParamsBuffer_);

    for (UINT i = 0; i < CSM_SPLIT_COUNT; ++i) {
        SAFE_RELEASE(shadowBuffers_[i]);
        SAFE_RELEASE(shadowDSViews_[i]);
        SAFE_RELEASE(shadowSRViews_[i]);
    }

    SAFE_RELEASE(readMaxTexture_);
    SAFE_RELEASE(transparentDepthBuffer_);
    SAFE_RELEASE(transparentDSView_);
    SAFE_RELEASE(transparentSRView_);
    SAFE_RELEASE(depthBuffer_);
    SAFE_RELEASE(depthDSView_);
    SAFE_RELEASE(depthSRView_);

    for (int i = 0; i < scaledFrames_.size(); ++i) {
        SAFE_RELEASE(scaledFrames_[i].texture);
        SAFE_RELEASE(scaledFrames_[i].SRV);
        SAFE_RELEASE(scaledFrames_[i].RTV);
    }
    scaledFrames_.clear();

    scenes_.clear();
    sceneArrays_.clear();
    transparentPrimitives_.clear();
}