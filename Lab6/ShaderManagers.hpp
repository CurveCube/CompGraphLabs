#pragma once

#include "framework.h"
#include "Utilities.h"
#include "D3DInclude.h"
#include <map>
#include <vector>
#include <string>
#include <memory>


template<typename ST>
class ShaderManagerBase {
public:
    bool CheckShader(const std::wstring& name, const std::vector<std::string>& macros) {
        return objects_.find(generateKey(name, macros)) != objects_.end();
    };

    virtual HRESULT LoadShader(const std::wstring& name, const std::vector<std::string>& macros) = 0;

    HRESULT GetShader(const std::wstring& name, const std::vector<std::string>& macros, std::shared_ptr<ST>& object) {
        auto result = objects_.find(generateKey(name, macros));
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

    std::wstring generateKey(const std::wstring& name, const std::vector<std::string>& macros) {
        std::wstring key = name + L"_";
        for (auto& m : macros) {
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
class Shader {
public:
    Shader(T* shader, ID3D10Blob* buffer) :
        shader_(shader, utilities::DXPtrDeleter<T*>),
        shaderBuffer_(buffer, utilities::DXPtrDeleter<ID3D10Blob*>) {}

    std::shared_ptr<T> GetShader() {
        return shader_;
    }

    std::shared_ptr<ID3D10Blob> GetShaderBuffer() {
        return shaderBuffer_;
    }

    void Release() {
        shader_.reset();
        shaderBuffer_.reset();
    };

    ~Shader() = default;
private:
    std::shared_ptr<T> shader_;
    std::shared_ptr<ID3D10Blob> shaderBuffer_;
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
            objects_.emplace(generateKey(name, macros),
                std::shared_ptr<Shader<ID3D11VertexShader>>(new Shader<ID3D11VertexShader>(vertexShader, vertexShaderBuffer),
                    utilities::DXPtrDeleter<Shader<ID3D11VertexShader>*>));
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
            objects_.emplace(generateKey(name, macros),
                std::shared_ptr<Shader<ID3D11PixelShader>>(new Shader<ID3D11PixelShader>(pixelShader, pixelShaderBuffer),
                    utilities::DXPtrDeleter<Shader<ID3D11PixelShader>*>));
        }
        return result;
    };

    ~PSManager() = default;
};