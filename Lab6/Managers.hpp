#pragma once

#include "framework.h"
#include "Utilities.h"
#include "D3DInclude.h"
#include <map>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <locale>
#include <codecvt>

#define STB_IMAGE_IMPLEMENTATION
#include "tinygltf/stb_image.h"


template<typename ST>
class ShaderManagerBase {
public:
    bool CheckShader(const std::wstring& name, const std::vector<std::string>& macros) {
        return objects_.find(GenerateKey(name, macros)) != objects_.end();
    };

    virtual HRESULT LoadShader(const std::wstring& name, const std::vector<std::string>& macros) = 0;

    HRESULT GetShader(const std::wstring& name, const std::vector<std::string>& macros, std::shared_ptr<ST>& object) {
        auto result = objects_.find(GenerateKey(name, macros));
        if (result == objects_.end())
            return E_FAIL;
        else {
            object = result->second;
            return S_OK;
        }
    };

    void ClearShaders() {
        objects_.clear();
    }

    virtual void Cleanup() {
        device_.reset();
        ClearShaders();
    };

protected:
    ShaderManagerBase(const std::shared_ptr<ID3D11Device>& devicePtr, const std::wstring& folder) : device_(devicePtr), folder_(folder) {};

    std::wstring GenerateKey(const std::wstring& name, const std::vector<std::string>& macros) {
        std::wstring key = name + L"_";
        std::vector<std::string> tmp = macros;
        std::sort(tmp.begin(), tmp.end());
        for (auto& m : tmp) {
            key += std::wstring(m.begin(), m.end()) + L"_";
        }
        key.pop_back();
        return key;
    }

    virtual ~ShaderManagerBase() = default;

    std::shared_ptr<ID3D11Device> device_;
    std::map<std::wstring, std::shared_ptr<ST>> objects_;
    std::wstring folder_;
};


template<typename T>
struct Shader {
    std::shared_ptr<T> shader_;
    std::shared_ptr<ID3D10Blob> shaderBuffer_;

    Shader(T* shader = nullptr, ID3D10Blob* buffer = nullptr) :
        shader_(shader, utilities::DXPtrDeleter<T*>),
        shaderBuffer_(buffer, utilities::DXPtrDeleter<ID3D10Blob*>) {}

    void SetWithDeleter(T* shader = nullptr, ID3D10Blob* buffer = nullptr) {
        shader_ = std::shared_ptr<T>(shader, utilities::DXPtrDeleter<T*>);
        shaderBuffer_ = std::shared_ptr<ID3D10Blob>(buffer, utilities::DXPtrDeleter<ID3D10Blob*>);
    }

    void Cleanup() {
        shader_.reset();
        shaderBuffer_.reset();
    };

    ~Shader() = default;
};


class VSManager : public ShaderManagerBase<Shader<ID3D11VertexShader>> {
public:
    VSManager(const std::shared_ptr<ID3D11Device>& devicePtr, const std::wstring& folder) : ShaderManagerBase(devicePtr, folder) {};

    HRESULT LoadShader(const std::wstring& name, const std::vector<std::string>& macros) override {
        if (CheckShader(name, macros)) {
            return S_OK;
        }

        std::vector<D3D_SHADER_MACRO> d3dmacros;
        for (auto& m : macros) {
            d3dmacros.push_back({ m.c_str() });
        }
        d3dmacros.push_back({ nullptr, nullptr });

        D3DInclude includeObj;
        std::wstring filePath = folder_ + L"/" + name;
        ID3D11VertexShader* vertexShader = nullptr;
        ID3D10Blob* vertexShaderBuffer = nullptr;
        int flags = 0;
#ifdef _DEBUG // Включение/отключение отладочной информации для шейдеров в зависимости от конфигурации сборки
        flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        ID3DBlob* pErrMsg = nullptr;
        HRESULT result = D3DCompileFromFile(filePath.c_str(), d3dmacros.data(), &includeObj, "main", "vs_5_0", flags, 0, &vertexShaderBuffer, &pErrMsg);
        if (!SUCCEEDED(result) && pErrMsg != nullptr)
        {
            OutputDebugStringA((const char*)pErrMsg->GetBufferPointer());
            SAFE_RELEASE(pErrMsg);
        }
        if (SUCCEEDED(result)) {
            result = device_->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), nullptr, &vertexShader);
        }
        if (SUCCEEDED(result)) {
            objects_.emplace(GenerateKey(name, macros),
                std::make_shared<Shader<ID3D11VertexShader>>(vertexShader, vertexShaderBuffer));
        }
        return result;
    };

    ~VSManager() = default;
};


class PSManager : public ShaderManagerBase<Shader<ID3D11PixelShader>> {
public:
    PSManager(const std::shared_ptr<ID3D11Device>& devicePtr, const std::wstring& folder) : ShaderManagerBase(devicePtr, folder) {};

    HRESULT LoadShader(const std::wstring& name, const std::vector<std::string>& macros) override {
        if (CheckShader(name, macros)) {
            return S_OK;
        }

        std::vector<D3D_SHADER_MACRO> d3dmacros;
        for (auto& m : macros) {
            d3dmacros.push_back({ m.c_str() });
        }
        d3dmacros.push_back({ nullptr, nullptr });

        D3DInclude includeObj;
        std::wstring filePath = folder_ + L"/" + name;
        ID3D11PixelShader* pixelShader = nullptr;
        ID3D10Blob* pixelShaderBuffer = nullptr;
        int flags = 0;
#ifdef _DEBUG // Включение/отключение отладочной информации для шейдеров в зависимости от конфигурации сборки
        flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        ID3DBlob* pErrMsg = nullptr;
        HRESULT result = D3DCompileFromFile(filePath.c_str(), d3dmacros.data(), &includeObj, "main", "ps_5_0", flags, 0, &pixelShaderBuffer, &pErrMsg);
        if (!SUCCEEDED(result) && pErrMsg != nullptr)
        {
            OutputDebugStringA((const char*)pErrMsg->GetBufferPointer());
            SAFE_RELEASE(pErrMsg);
        }
        if (SUCCEEDED(result)) {
            result = device_->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), nullptr, &pixelShader);
        }
        if (SUCCEEDED(result)) {
            objects_.emplace(GenerateKey(name, macros),
                std::make_shared<Shader<ID3D11PixelShader>>(pixelShader, pixelShaderBuffer));
        }
        return result;
    };

    ~PSManager() = default;
};


struct Texture {
    std::shared_ptr<ID3D11Resource> texture_;
    std::shared_ptr<ID3D11ShaderResourceView> SRV_;

    Texture(ID3D11Resource* texture = nullptr, ID3D11ShaderResourceView* SRV = nullptr) :
        texture_(texture, utilities::DXPtrDeleter<ID3D11Resource*>),
        SRV_(SRV, utilities::DXPtrDeleter<ID3D11ShaderResourceView*>) {};

    void SetTextureWithDeleter(ID3D11Resource* texture = nullptr) {
        texture_ = std::shared_ptr<ID3D11Resource>(texture, utilities::DXPtrDeleter<ID3D11Resource*>);
    }

    void SetSRVWithDeleter(ID3D11ShaderResourceView* SRV = nullptr) {
        SRV_ = std::shared_ptr<ID3D11ShaderResourceView>(SRV, utilities::DXPtrDeleter<ID3D11ShaderResourceView*>);
    }

    virtual void Cleanup() {
        texture_.reset();
        SRV_.reset();
    };

    virtual ~Texture() = default;
};


class TextureWithRTV : public Texture {
    std::shared_ptr<ID3D11RenderTargetView> RTV_;

    TextureWithRTV(ID3D11Resource* texture = nullptr, ID3D11ShaderResourceView* SRV = nullptr, ID3D11RenderTargetView* RTV = nullptr) :
        Texture(texture, SRV), RTV_(RTV, utilities::DXPtrDeleter<ID3D11RenderTargetView*>) {};

    void SetRTVWithDeleter(ID3D11RenderTargetView* RTV = nullptr) {
        RTV_ = std::shared_ptr<ID3D11RenderTargetView>(RTV, utilities::DXPtrDeleter<ID3D11RenderTargetView*>);
    }

    void Cleanup() {
        Texture::Cleanup();
        RTV_.reset();
    };

    ~TextureWithRTV() = default;
};


class TextureManager {
public:
    TextureManager(const std::shared_ptr<ID3D11Device>& devicePtr,
        const std::shared_ptr<ID3D11DeviceContext>& deviceContextPtr, const std::wstring& folder) :
        device_(devicePtr), deviceContext_(deviceContextPtr), folder_(folder) {};

    bool CheckTexture(const std::wstring& name) {
        return textures_.find(name) != textures_.end();
    };

    HRESULT GetTexture(const std::wstring& name, std::shared_ptr<Texture>& texture) {
        auto result = textures_.find(name);
        if (result == textures_.end())
            return E_FAIL;
        else {
            texture = result->second;
            return S_OK;
        }
    };

    HRESULT LoadTexture(const std::wstring& name, const std::string& annotationText = "") {
        if (CheckTexture(name))
            return S_OK;

        ID3D11Resource* texture;
        ID3D11ShaderResourceView* SRV;
        HRESULT result = CreateDDSTextureFromFile(device_.get(), deviceContext_.get(), (folder_ + L"/" + name).c_str(), &texture, &SRV);
        if (SUCCEEDED(result) && annotationText != "") {
            result = texture->SetPrivateData(WKPDID_D3DDebugObjectName, annotationText.size(), annotationText.c_str());
        }
        if (SUCCEEDED(result)) {
            textures_.emplace(name, std::make_shared<Texture>(texture, SRV));
        }
        return result;
    };

    HRESULT LoadTexture(ID3D11Resource* texture, ID3D11ShaderResourceView* SRV, const std::wstring& name) {
        if (CheckTexture(name))
            return E_FAIL; // Не допускаем перезаписи значения для ключа

        textures_.emplace(name, std::make_shared<Texture>(texture, SRV));

        return S_OK;
    };

    HRESULT LoadCubeMapTexture(const std::wstring& name, const std::string& annotationText = "") {
        if (CheckTexture(name))
            return S_OK;

        ID3D11Resource* texture;
        ID3D11ShaderResourceView* SRV;
        HRESULT result = CreateDDSTextureFromFileEx(device_.get(), deviceContext_.get(), (folder_ + L"/" + name).c_str(),
            0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_TEXTURECUBE,
            DDS_LOADER_DEFAULT, &texture, &SRV);
        if (SUCCEEDED(result) && annotationText != "") {
            result = texture->SetPrivateData(WKPDID_D3DDebugObjectName, annotationText.size(), annotationText.c_str());
        }
        if (SUCCEEDED(result)) {
            textures_.emplace(name, std::make_shared<Texture>(texture, SRV));
        }
        return result;
    };

    HRESULT LoadHDRTexture(const std::wstring& name, const std::string& annotationText = "") {
        if (CheckTexture(name))
            return S_OK;

        int width, height, nrComponents;
        std::wstring_convert<std::wstring> converter;
        std::string filePath = converter.to_bytes(folder_ + L"/" + name);
        float* data = stbi_loadf(filePath.c_str(), &width, &height, &nrComponents, 4);
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
        ID3D11ShaderResourceView* SRV;
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
            textures_.emplace(name, std::make_shared<Texture>(texture, SRV));
        }

        stbi_image_free(data);

        return result;
    };

    void ClearTextures() {
        textures_.clear();
    };

    void Cleanup() {
        device_.reset();
        deviceContext_.reset();
        ClearTextures();
    };

    ~TextureManager() = default;

private:
    std::shared_ptr<ID3D11Device> device_;
    std::shared_ptr<ID3D11DeviceContext> deviceContext_;
    std::map<std::wstring, std::shared_ptr<Texture>> textures_;
    std::wstring folder_;
};


class ManagerStorage {
public:
    static ManagerStorage& GetInstance() {
        static ManagerStorage instance;
        return instance;
    };

    void Init(const std::shared_ptr<ID3D11Device>& devicePtr,
        const std::shared_ptr<ID3D11DeviceContext>& deviceContextPtr,
        const std::wstring& VSFolder,
        const std::wstring& PSFolder,
        const std::wstring& textureFolder) {
        VSManager_ = std::make_shared<VSManager>(devicePtr, VSFolder);
        PSManager_ = std::make_shared<PSManager>(devicePtr, PSFolder);
        textureManager_ = std::make_shared<TextureManager>(devicePtr, deviceContextPtr, textureFolder);
        isInit = true;
    };

    bool IsInited() {
        return isInit;
    };

    std::shared_ptr<VSManager> GetVSManager() {
        return VSManager_;
    };

    std::shared_ptr<PSManager> GetPSManager() {
        return PSManager_;
    };

    std::shared_ptr<TextureManager> GetTextureManager() {
        return textureManager_;
    };

    void Cleanup() {
        VSManager_->Cleanup();
        PSManager_->Cleanup();
        textureManager_->Cleanup();
        isInit = false;
    };

private:
    ManagerStorage() = default;

    ~ManagerStorage() = default;

    std::shared_ptr<VSManager> VSManager_;
    std::shared_ptr<PSManager> PSManager_;
    std::shared_ptr<TextureManager> textureManager_;
    bool isInit = false;
};