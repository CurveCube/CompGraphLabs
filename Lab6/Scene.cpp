#include "Scene.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tinygltf/tiny_gltf.h"
#undef TINYGLTF_IMPLEMENTATION
#undef STB_IMAGE_WRITE_IMPLEMENTATION

const float SceneManager::slopeScaleBias_ = 2 * sqrt(2);

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
}

HRESULT SceneManager::Init(const std::shared_ptr<Device>& device, const std::shared_ptr<ManagerStorage>& managerStorage,
    const std::shared_ptr<Camera>& camera) {
    if (!managerStorage->IsInit() || !device->IsInit()) {
        return E_FAIL;
    }

    device_ = device;
    managerStorage_ = managerStorage;
    camera_ = camera;

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(WorldMatrixBuffer);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;
    HRESULT result = device_->GetDevice()->CreateBuffer(&desc, nullptr, &worldMatrixBuffer_);

    if (SUCCEEDED(result)) {
        result = CreateDepthStencilViews();
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
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(MaterialParamsBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;
        result = device_->GetDevice()->CreateBuffer(&desc, nullptr, &materialParamsBuffer_);
    }

    return result;
}

HRESULT SceneManager::CreateDepthStencilViews() {
    HRESULT result = S_OK;
    for (UINT i = 0; i < CSM_SPLIT_COUNT && SUCCEEDED(result); ++i) {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Format = DXGI_FORMAT_R32_TYPELESS;
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.Height = shadowMapSize;
        desc.Width = shadowMapSize;
        desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        result = device_->GetDevice()->CreateTexture2D(&desc, nullptr, &shadowBuffers_[i]);
        if (SUCCEEDED(result)) {
            D3D11_DEPTH_STENCIL_VIEW_DESC desc = {};
            desc.Format = DXGI_FORMAT_D32_FLOAT;
            desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            desc.Texture2D.MipSlice = 0;
            desc.Flags = 0;
            result = device_->GetDevice()->CreateDepthStencilView(shadowBuffers_[i], &desc, &shadowDSViews_[i]);
        }
        if (SUCCEEDED(result)) {
            D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
            desc.Format = DXGI_FORMAT_R32_FLOAT;
            desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            desc.Texture2D.MipLevels = 1;
            desc.Texture2D.MostDetailedMip = 0;
            result = device_->GetDevice()->CreateShaderResourceView(shadowBuffers_[i], &desc, &shadowSRViews_[i]);
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
    sceneArrays_.push_back(arrays);
    for (auto& gs : model.scenes) {
        Scene s;
        s.arraysId = sceneArrays_.size() - 1;
        s.transformation = transformation;
        s.rootNodes = gs.nodes;
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

    count = scenes_.size() - index;

    return result;
}

HRESULT SceneManager::CreateBufferAccessors(const tinygltf::Model& model, SceneArrays& arrays) {
    HRESULT result = S_OK;
    for (auto& ga : model.accessors) {
        BufferAccessor accessor;
        accessor.count = ga.count;
        accessor.byteOffset = ga.byteOffset;
        accessor.format = GetFormat(ga);

        const tinygltf::BufferView& gbv = model.bufferViews[ga.bufferView];
        accessor.byteStride = gbv.byteStride;

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

        result = device_->GetDevice()->CreateBuffer(&desc, &data, &accessor.buffer);
        delete[] bufferPart;
        if (FAILED(result)) {
            break;
        }
        arrays.accessors.push_back(accessor);
        accessor.reset();
    }
    return result;
}

DXGI_FORMAT SceneManager::GetFormat(const tinygltf::Accessor& accessor) {
    DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    switch (accessor.type) {
    case TINYGLTF_TYPE_SCALAR:
        format = GetFormatScalar(accessor);
        break;
    case TINYGLTF_TYPE_VEC2:
        format = GetFormatVec2(accessor);
        break;
    case TINYGLTF_TYPE_VEC3:
        format = GetFormatVec3(accessor);
        break;
    case TINYGLTF_TYPE_VEC4:
        format = GetFormatVec4(accessor);
        break;
    default:
        break;
    }
    return format;
}

DXGI_FORMAT SceneManager::GetFormatScalar(const tinygltf::Accessor& accessor) {
    DXGI_FORMAT format = DXGI_FORMAT_R32_FLOAT;
    switch (accessor.componentType) {
    case TINYGLTF_COMPONENT_TYPE_BYTE:
        if (accessor.normalized) {
            format = DXGI_FORMAT_R8_SNORM;
        }
        else {
            format = DXGI_FORMAT_R8_SINT;
        }
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        if (accessor.normalized) {
            format = DXGI_FORMAT_R8_UNORM;
        }
        else {
            format = DXGI_FORMAT_R8_UINT;
        }
        break;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
        if (accessor.normalized) {
            format = DXGI_FORMAT_R16_SNORM;
        }
        else {
            format = DXGI_FORMAT_R16_SINT;
        }
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        if (accessor.normalized) {
            format = DXGI_FORMAT_R16_UNORM;
        }
        else {
            format = DXGI_FORMAT_R16_UINT;
        }
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        format = DXGI_FORMAT_R32_UINT;
        break;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        format = DXGI_FORMAT_R32_FLOAT;
        break;
    default:
        break;
    }
    return format;
}

DXGI_FORMAT SceneManager::GetFormatVec2(const tinygltf::Accessor& accessor) {
    DXGI_FORMAT format = DXGI_FORMAT_R32G32_FLOAT;
    switch (accessor.componentType) {
    case TINYGLTF_COMPONENT_TYPE_BYTE:
        if (accessor.normalized) {
            format = DXGI_FORMAT_R8G8_SNORM;
        }
        else {
            format = DXGI_FORMAT_R8G8_SINT;
        }
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        if (accessor.normalized) {
            format = DXGI_FORMAT_R8G8_UNORM;
        }
        else {
            format = DXGI_FORMAT_R8G8_UINT;
        }
        break;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
        if (accessor.normalized) {
            format = DXGI_FORMAT_R16G16_SNORM;
        }
        else {
            format = DXGI_FORMAT_R16G16_SINT;
        }
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        if (accessor.normalized) {
            format = DXGI_FORMAT_R16G16_UNORM;
        }
        else {
            format = DXGI_FORMAT_R16G16_UINT;
        }
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        format = DXGI_FORMAT_R32G32_UINT;
        break;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        format = DXGI_FORMAT_R32G32_FLOAT;
        break;
    default:
        break;
    }
    return format;
}

DXGI_FORMAT SceneManager::GetFormatVec3(const tinygltf::Accessor& accessor) {
    DXGI_FORMAT format = DXGI_FORMAT_R32G32B32_FLOAT;
    switch (accessor.componentType) {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        format = DXGI_FORMAT_R32G32B32_UINT;
        break;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        format = DXGI_FORMAT_R32G32B32_FLOAT;
        break;
    default:
        break;
    }
    return format;
}

DXGI_FORMAT SceneManager::GetFormatVec4(const tinygltf::Accessor& accessor) {
    DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    switch (accessor.componentType) {
    case TINYGLTF_COMPONENT_TYPE_BYTE:
        if (accessor.normalized) {
            format = DXGI_FORMAT_R8G8B8A8_SNORM;
        }
        else {
            format = DXGI_FORMAT_R8G8B8A8_SINT;
        }
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        if (accessor.normalized) {
            format = DXGI_FORMAT_R8G8B8A8_UNORM;
        }
        else {
            format = DXGI_FORMAT_R8G8B8A8_UINT;
        }
        break;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
        if (accessor.normalized) {
            format = DXGI_FORMAT_R16G16B16A16_SNORM;
        }
        else {
            format = DXGI_FORMAT_R16G16B16A16_SINT;
        }
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        if (accessor.normalized) {
            format = DXGI_FORMAT_R16G16B16A16_UNORM;
        }
        else {
            format = DXGI_FORMAT_R16G16B16A16_UINT;
        }
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        format = DXGI_FORMAT_R32G32B32A32_UINT;
        break;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        format = DXGI_FORMAT_R32G32B32A32_FLOAT;
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
            material.mode = BLEND_MODE;
            result = managerStorage_->GetStateManager()->CreateBlendState(material.blendState);
            if (SUCCEEDED(result)) {
                result = managerStorage_->GetStateManager()->CreateDepthStencilState(material.depthStencilState,
                    D3D11_COMPARISON_GREATER_EQUAL, D3D11_DEPTH_WRITE_MASK_ZERO);
            }
        }
        else if (gm.alphaMode == "MASK") {
            material.mode = ALPHA_CUTOFF_MODE;
            material.alphaCutoff = gm.alphaCutoff;
            result = managerStorage_->GetStateManager()->CreateDepthStencilState(material.depthStencilState);
        }
        else {
            material.mode = OPAQUE_MODE;
            result = managerStorage_->GetStateManager()->CreateDepthStencilState(material.depthStencilState);
        }

        if (FAILED(result)) {
            break;
        }

        D3D11_CULL_MODE cullMode = D3D11_CULL_NONE;
        if (!gm.doubleSided) {
            cullMode = D3D11_CULL_BACK;
        }
        result = managerStorage_->GetStateManager()->CreateRasterizerState(material.rasterizerState, D3D11_FILL_SOLID, cullMode, depthBias_, slopeScaleBias_);

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
        material.baseColorTA = { gm.pbrMetallicRoughness.baseColorTexture.texCoord, model.textures[index].source, model.textures[index].sampler, true };

        index = gm.pbrMetallicRoughness.metallicRoughnessTexture.index;
        material.roughMetallicTA = { gm.pbrMetallicRoughness.metallicRoughnessTexture.texCoord, model.textures[index].source, model.textures[index].sampler, false };

        index = gm.normalTexture.index;
        material.normalTA = { gm.normalTexture.texCoord, model.textures[index].source, model.textures[index].sampler, false };

        index = gm.emissiveTexture.index;
        material.emissiveTA = { gm.emissiveTexture.texCoord, model.textures[index].source, model.textures[index].sampler, true };

        index = gm.occlusionTexture.index;
        material.occlusionTA = { gm.occlusionTexture.texCoord, model.textures[index].source, model.textures[index].sampler, false };

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
            case BLEND_MODE:
                mesh.transparentPrimitives.push_back(primitive);
                break;
            case ALPHA_CUTOFF_MODE:
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
    attributes.clear();
    for (auto& ga : primitive.attributes) {
        Attribute attribute = { ga.first, ga.second };
        if (ga.first == "TANGENT") {
            baseDefines.push_back("HAS_TANGENT");
        }
        else if (ga.first == "TEXCOORD_0") {
            baseDefines.push_back("HAS_TEXCOORD_0");
        }
        else if (ga.first == "TEXCOORD_1") {
            baseDefines.push_back("HAS_TEXCOORD_1");
        }
        else if (ga.first == "TEXCOORD_2") {
            baseDefines.push_back("HAS_TEXCOORD_2");
        }
        else if (ga.first == "TEXCOORD_3") {
            baseDefines.push_back("HAS_TEXCOORD_3");
        }
        else if (ga.first == "TEXCOORD_4") { // by the number of textures of the material
            baseDefines.push_back("HAS_TEXCOORD_4");
        }
        else if (ga.first == "COLOR") {
            baseDefines.push_back("HAS_COLOR");
        }
        else {
            ; // ignore others attributes (POSITION and NORMAL always implied)
        }
        attributes.push_back(attribute);
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
    if (material.mode == ALPHA_CUTOFF_MODE) {
        baseDefines.push_back("HAS_ALPHA_CUTOFF");
    }
    std::sort(attributes.begin(), attributes.end());
    desc.clear();
    for (const auto& a : attributes) {
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

HRESULT SceneManager::CreateShaders(Primitive& primitive, const SceneArrays& arrays,
    const std::vector<std::string>& baseDefines, const std::vector<D3D11_INPUT_ELEMENT_DESC>& desc) {
    std::vector<std::string> defaulMacros = baseDefines;
    defaulMacros.push_back("DEFAULT");

    std::vector<std::string> fresnelMacros = baseDefines;
    defaulMacros.push_back("FRESNEL");

    std::vector<std::string> ndfMacros = baseDefines;
    defaulMacros.push_back("ND");

    std::vector<std::string> geometryMacros = baseDefines;
    defaulMacros.push_back("GEOMETRY"); // обработать прозрачность

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
    if (SUCCEEDED(result) && arrays.materials[primitive.materialId].mode != OPAQUE_MODE) { // opaque without pixel shader for shadow map
        result = managerStorage_->GetPSManager()->LoadShader(primitive.shadowPS, L"shaders/shadowPS.hlsl", baseDefines);
    }

    return result;
}

HRESULT SceneManager::CreateNodes(const tinygltf::Model& model, SceneArrays& arrays) {
    HRESULT result = S_OK;
    for (auto& gn : model.nodes) {
        if (gn.mesh < 0) {
            continue;
        }

        Node node;
        node.meshId = gn.mesh;
        node.children = gn.children;

        if (gn.matrix.empty()) {
            XMMATRIX transformation = XMMatrixIdentity();
            if (!gn.translation.empty()) {
                transformation = XMMatrixTranslation(gn.translation[0], gn.translation[1], gn.translation[2]);
            }
            if (!gn.rotation.empty()) {
                transformation = XMMatrixMultiply(transformation, XMMatrixRotationQuaternion({
                    (float)gn.rotation[0], (float)gn.rotation[1], (float)gn.rotation[2], (float)gn.rotation[3]
                }));
            }
            if (!gn.scale.empty()) {
                transformation = XMMatrixMultiply(transformation, XMMatrixScaling(gn.scale[0], gn.scale[1], gn.scale[2]));
            }
            node.transformation = transformation;
        }
        else {
            node.transformation = {
                XMVECTOR{(float)gn.matrix[0], (float)gn.matrix[4], (float)gn.matrix[8], (float)gn.matrix[12]},
                XMVECTOR{(float)gn.matrix[1], (float)gn.matrix[5], (float)gn.matrix[9], (float)gn.matrix[13]},
                XMVECTOR{(float)gn.matrix[2], (float)gn.matrix[6], (float)gn.matrix[10], (float)gn.matrix[14]},
                XMVECTOR{(float)gn.matrix[3], (float)gn.matrix[7], (float)gn.matrix[11], (float)gn.matrix[15]}
            };
        }

        arrays.nodes.push_back(node);
    }
    return result;
}

bool SceneManager::CreateShadowMaps(const DirectionalLight& dirLight, const std::vector<int>& sceneIndices) {
    return true;
}

bool SceneManager::Render(
    const std::shared_ptr<ID3D11ShaderResourceView>& irradianceMap,
    const std::shared_ptr<ID3D11ShaderResourceView>& prefilteredMap,
    const std::shared_ptr<ID3D11ShaderResourceView>& BRDF,
    const std::shared_ptr<Skybox>& skybox,
    const std::vector<SpotLight>& lights,
    const DirectionalLight& dirLight,
    const std::vector<int>& sceneIndices
) {
    return true;
}

bool SceneManager::RenderTransparent(
    const std::shared_ptr<ID3D11ShaderResourceView>& irradianceMap,
    const std::shared_ptr<ID3D11ShaderResourceView>& prefilteredMap,
    const std::shared_ptr<ID3D11ShaderResourceView>& BRDF,
    const std::vector<SpotLight>& lights,
    const DirectionalLight& dirLight,
    const std::vector<int>& sceneIndices
) {
    return true;
}

void SceneManager::SetMode(Mode mode) {
    currentMode_ = mode;
}

bool SceneManager::IsInit() const {
    return !!materialParamsBuffer_;
}

void SceneManager::Cleanup() {
    device_.reset();
    managerStorage_.reset();
    camera_.reset();

    SAFE_RELEASE(worldMatrixBuffer_);
    SAFE_RELEASE(viewMatrixBuffer_);
    SAFE_RELEASE(materialParamsBuffer_);

    for (UINT i = 0; i < CSM_SPLIT_COUNT; ++i) {
        SAFE_RELEASE(shadowBuffers_[i]);
        SAFE_RELEASE(shadowDSViews_[i]);
        SAFE_RELEASE(shadowSRViews_[i]);
    }

    scenes_.clear();
    sceneArrays_.clear();
    transparentPrimitives_.clear();
}