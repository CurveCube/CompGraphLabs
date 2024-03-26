#include "SimpleManager.h"
#include "D3DInclude.h"
#define STB_IMAGE_IMPLEMENTATION
#include "tinygltf/stb_image.h"


HRESULT SimpleILManager::loadIL(const std::string& key, const D3D11_INPUT_ELEMENT_DESC* desc, UINT numElements, ID3D10Blob* VSBuffer) {
    if (check(key))
        return E_FAIL; // Не допускаем перезаписи значения для ключа

    ID3D11InputLayout* inputLayout;
    HRESULT result = device_->CreateInputLayout(desc, numElements, VSBuffer->GetBufferPointer(), VSBuffer->GetBufferSize(), &inputLayout);
    if (SUCCEEDED(result)) {
        load(key, inputLayout);
    }
    return result;
}


HRESULT SimpleVSManager::loadVS(LPCWSTR filePath, const D3D_SHADER_MACRO* macros, const std::string& key,
                                SimpleILManager* ILManager, const D3D11_INPUT_ELEMENT_DESC* desc, UINT numElements) {
    if (check(key))
        return E_FAIL; // Не допускаем перезаписи значения для ключа

    D3DInclude includeObj;
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D10Blob* vertexShaderBuffer = nullptr;
    int flags = 0;
#ifdef _DEBUG // Включение/отключение отладочной информации для шейдеров в зависимости от конфигурации сборки
    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    ID3DBlob* pErrMsg = nullptr;
    HRESULT result = D3DCompileFromFile(filePath, macros, &includeObj, "main", "vs_5_0", flags, 0, &vertexShaderBuffer, &pErrMsg);
    if (!SUCCEEDED(result) && pErrMsg != nullptr)
    {
        OutputDebugStringA((const char*)pErrMsg->GetBufferPointer());
        SAFE_RELEASE(pErrMsg);
    }
    if (SUCCEEDED(result)) {
        result = device_->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), nullptr, &vertexShader);
    }
    if (SUCCEEDED(result) && ILManager != nullptr && desc != nullptr) {
        result = ILManager->loadIL(key, desc, numElements, vertexShaderBuffer);
    }
    if (!SUCCEEDED(result)) { // Для освобождения ресурсов, если не удалось создать InputLayout
        SAFE_RELEASE(vertexShader);
    }
    if (SUCCEEDED(result)) {
        load(key, vertexShader);
    }
    SAFE_RELEASE(vertexShaderBuffer);
    return result;
}


HRESULT SimplePSManager::loadPS(LPCWSTR filePath, const D3D_SHADER_MACRO* macros, const std::string& key) {
    if (check(key))
        return E_FAIL; // Не допускаем перезаписи значения для ключа

    D3DInclude includeObj;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D10Blob* pixelShaderBuffer = nullptr;
    int flags = 0;
#ifdef _DEBUG // Включение/отключение отладочной информации для шейдеров в зависимости от конфигурации сборки
    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    ID3DBlob* pErrMsg = nullptr;
    HRESULT result = D3DCompileFromFile(filePath, macros, &includeObj, "main", "ps_5_0", flags, 0, &pixelShaderBuffer, &pErrMsg);
    if (!SUCCEEDED(result) && pErrMsg != nullptr)
    {
        OutputDebugStringA((const char*)pErrMsg->GetBufferPointer());
        SAFE_RELEASE(pErrMsg);
    }
    if (SUCCEEDED(result)) {
        result = device_->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), nullptr, &pixelShader);
    }
    if (SUCCEEDED(result)) {
        load(key, pixelShader);
    }
    SAFE_RELEASE(pixelShaderBuffer);
    return result;
}


HRESULT SimpleSamplerManager::loadSampler(D3D11_FILTER filter, const std::string& key, D3D11_TEXTURE_ADDRESS_MODE mode) {
    if (check(key))
        return E_FAIL; // Не допускаем перезаписи значения для ключа

    D3D11_SAMPLER_DESC desc = {};
    desc.Filter = filter;
    desc.AddressU = mode;
    desc.AddressV = mode;
    desc.AddressW = mode;
    desc.MinLOD = -FLT_MAX;
    desc.MaxLOD = FLT_MAX;
    desc.MipLODBias = 0.0f;
    desc.MaxAnisotropy = 16;
    desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 1.0f;

    ID3D11SamplerState* sampler = nullptr;
    HRESULT result = device_->CreateSamplerState(&desc, &sampler);
    if (SUCCEEDED(result)) {
        load(key, sampler);
    }
    return result;
}


HRESULT SimpleGeometryManager::loadGeometry(const void* vertices, UINT verticesBytes, const UINT* indices,
                                            UINT indicesBytes, const std::string& key) {
    if (check(key))
        return E_FAIL; // Не допускаем перезаписи значения для ключа

    ID3D11Buffer* vertexBuffer = nullptr;
    ID3D11Buffer* indexBuffer = nullptr;

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = verticesBytes;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = vertices;
    data.SysMemPitch = verticesBytes;
    data.SysMemSlicePitch = 0;

    HRESULT result = device_->CreateBuffer(&desc, &data, &vertexBuffer);

    if (SUCCEEDED(result)) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = indicesBytes;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = indices;
        data.SysMemPitch = indicesBytes;
        data.SysMemSlicePitch = 0;

        result = device_->CreateBuffer(&desc, &data, &indexBuffer);
    }

    if (SUCCEEDED(result)) {
        objects_.emplace(key, std::make_shared<Geometry>(vertexBuffer, indexBuffer, indicesBytes / sizeof(UINT)));
    }

    return result;
};


HRESULT SimpleTextureManager::loadTexture(LPCWSTR filePath, const std::string& key, const std::string& annotationText) {
    if (check(key))
        return E_FAIL; // Не допускаем перезаписи значения для ключа

    ID3D11Resource* texture;
    ID3D11ShaderResourceView* SRV;
    HRESULT result = CreateDDSTextureFromFile(device_.get(), deviceContext_.get(), filePath, &texture, &SRV);
    if (SUCCEEDED(result) && annotationText != "") {
        result = texture->SetPrivateData(WKPDID_D3DDebugObjectName, annotationText.size(), annotationText.c_str());
    }
    if (SUCCEEDED(result)) {
        objects_.emplace(key, std::make_shared<SimpleTexture>(texture, SRV));
    }
    return result;
}


HRESULT SimpleTextureManager::loadTexture(ID3D11Resource* texture, ID3D11ShaderResourceView* SRV, const std::string& key) {
    if (check(key))
        return E_FAIL; // Не допускаем перезаписи значения для ключа
    
    objects_.emplace(key, std::make_shared<SimpleTexture>(texture, SRV));

    return S_OK;
}


HRESULT SimpleTextureManager::loadCubeMapTexture(LPCWSTR filePath, const std::string& key, const std::string& annotationText) {
    if (check(key))
        return E_FAIL; // Не допускаем перезаписи значения для ключа

    ID3D11Resource* texture;
    ID3D11ShaderResourceView* SRV;
    HRESULT result = CreateDDSTextureFromFileEx(device_.get(), deviceContext_.get(), filePath,
        0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_TEXTURECUBE,
        DDS_LOADER_DEFAULT, &texture, &SRV);
    if (SUCCEEDED(result) && annotationText != "") {
        result = texture->SetPrivateData(WKPDID_D3DDebugObjectName, annotationText.size(), annotationText.c_str());
    }
    if (SUCCEEDED(result)) {
        objects_.emplace(key, std::make_shared<SimpleTexture>(texture, SRV));
    }
    return result;
}


HRESULT SimpleTextureManager::loadHDRTexture(const char* filePath, const std::string& key, const std::string& annotationText)
{
    if (check(key))
        return E_FAIL; // Не допускаем перезаписи значения для ключа

    ID3D11ShaderResourceView* SRV;
    bool b = stbi_is_hdr(filePath);
    int width, height, nrComponents;
    float* data = stbi_loadf(filePath, &width, &height, &nrComponents, 4);
    if (!data)
        return E_FAIL;

    HRESULT result = S_OK;

    D3D11_TEXTURE2D_DESC textureDesc = {};

    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA initData;
    initData.pSysMem = data;
    initData.SysMemPitch = width * sizeof(float) * 4;
    initData.SysMemSlicePitch = width * height * sizeof(float) * 4;

    ID3D11Texture2D* texture;
    result = device_.get()->CreateTexture2D(&textureDesc, &initData, &texture);

    if (SUCCEEDED(result))
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC descSRV = {};
        descSRV.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        descSRV.Texture2D.MipLevels = 1;
        descSRV.Texture2D.MostDetailedMip = 0;
        result = device_.get()->CreateShaderResourceView(texture, &descSRV, &SRV);
    }

    if (SUCCEEDED(result) && annotationText != "") {
        result = texture->SetPrivateData(WKPDID_D3DDebugObjectName, annotationText.size(), annotationText.c_str());
    }
    if (SUCCEEDED(result)) {
        objects_.emplace(key, std::make_shared<SimpleTexture>(texture, SRV));
    }

    stbi_image_free(data);

    return S_OK;
}