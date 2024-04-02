#pragma once

#include "Device.hpp"
#include "D3DInclude.hpp"
#include <map>
#include <vector>
#include <string>
#include <algorithm>


namespace {
    std::wstring GenerateKey(const std::wstring& name, const std::vector<std::string>& macros) {
        std::wstring key = name + L"_";
        std::vector<std::string> tmp = macros;
        std::sort(tmp.begin(), tmp.end());
        for (auto& m : tmp) {
            key += std::wstring(m.begin(), m.end()) + L"_";
        }
        key.pop_back();
        return key;
    };
}; // anonymous namespace


template<typename ST>
class ShaderManagerBase {
public:
    bool CheckShader(const std::wstring& name, const std::vector<std::string>& macros) const {
        return objects_.find(GenerateKey(name, macros)) != objects_.end();
    };

    HRESULT LoadShader(const std::wstring& name, const std::vector<std::string>& macros) {
        std::shared_ptr<ST> object;
        return LoadShader(object, name, macros);
    };

    HRESULT GetShader(std::shared_ptr<ST>& object, const std::wstring& name, const std::vector<std::string>& macros) const {
        auto result = objects_.find(GenerateKey(name, macros));
        if (result == objects_.end()) {
            return E_FAIL;
        }
        else {
            object = result->second;
            return S_OK;
        }
    };

    void ClearShaders() {
        objects_.clear();
    }

    void Cleanup() {
        device_.reset();
        ClearShaders();
    };

    virtual ~ShaderManagerBase() = default;

protected:
    ShaderManagerBase(const std::shared_ptr<Device>& device) : device_(device) {};

    std::shared_ptr<Device> device_; // provided externally <-
    std::map<std::wstring, std::shared_ptr<ST>> objects_; // shaders are transmitted outward ->
};


class VertexShader {
public:
    VertexShader(ID3D11VertexShader* shader, ID3D10Blob* buffer, ID3D11InputLayout* IL) :
        shader_(shader, utilities::DXPtrDeleter<ID3D11VertexShader*>),
        shaderBuffer_(buffer, utilities::DXPtrDeleter<ID3D10Blob*>),
        IL_(IL, utilities::DXPtrDeleter<ID3D11InputLayout*>) {}

    std::shared_ptr<ID3D11VertexShader> GetShader() const {
        return shader_;
    };

    std::shared_ptr<ID3D10Blob> GetShaderBuffer() const {
        return shaderBuffer_;
    };

    std::shared_ptr<ID3D11InputLayout> GetInputLayout() const {
        return IL_;
    };

    ~VertexShader() = default;

private:
    std::shared_ptr<ID3D11VertexShader> shader_; // transmitted outward ->
    std::shared_ptr<ID3D10Blob> shaderBuffer_; // transmitted outward ->
    std::shared_ptr<ID3D11InputLayout> IL_; // transmitted outward ->
};


class VSManager : public ShaderManagerBase<VertexShader> {
public:
    VSManager(const std::shared_ptr<Device>& devicePtr) : ShaderManagerBase(devicePtr) {};

    HRESULT LoadShader(std::shared_ptr<VertexShader>& object, const std::wstring& name,
        const std::vector<std::string>& macros = {}, const std::vector<D3D11_INPUT_ELEMENT_DESC>& ILDesc = {}) {
        if (SUCCEEDED(GetShader(object, name, macros))) {
            return S_OK;
        }

        std::vector<D3D_SHADER_MACRO> d3dmacros;
        for (auto& m : macros) {
            d3dmacros.push_back({ m.c_str() });
        }
        d3dmacros.push_back({ nullptr, nullptr });

        D3DInclude includeObj;
        ID3D11VertexShader* vertexShader = nullptr;
        ID3D10Blob* vertexShaderBuffer = nullptr;
        ID3D11InputLayout* inputLayout = nullptr;
        int flags = 0;
#ifdef _DEBUG
        flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        ID3DBlob* pErrMsg = nullptr;
        HRESULT result = D3DCompileFromFile(name.c_str(), d3dmacros.data(), &includeObj, "main", "vs_5_0", flags, 0, &vertexShaderBuffer, &pErrMsg);
        if (FAILED(result) && pErrMsg != nullptr) {
            OutputDebugStringA((const char*)pErrMsg->GetBufferPointer());
            SAFE_RELEASE(pErrMsg);
        }
        if (SUCCEEDED(result)) {
            result = device_->GetDevice()->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(),
                nullptr, &vertexShader);
        }
        if (SUCCEEDED(result) && ILDesc.size() > 0) {
            result = device_->GetDevice()->CreateInputLayout(ILDesc.data(), ILDesc.size(), vertexShaderBuffer->GetBufferPointer(),
                vertexShaderBuffer->GetBufferSize(), &inputLayout);
        }
        if (SUCCEEDED(result)) {
            object = std::make_shared<VertexShader>(vertexShader, vertexShaderBuffer, inputLayout);
            objects_.emplace(GenerateKey(name, macros), object);
        }
        return result;
    };

    ~VSManager() = default;
};


class PixelShader {
public:
    PixelShader(ID3D11PixelShader* shader, ID3D10Blob* buffer) :
        shader_(shader, utilities::DXPtrDeleter<ID3D11PixelShader*>),
        shaderBuffer_(buffer, utilities::DXPtrDeleter<ID3D10Blob*>) {}

    std::shared_ptr<ID3D11PixelShader> GetShader() const {
        return shader_;
    };

    std::shared_ptr<ID3D10Blob> GetShaderBuffer() const {
        return shaderBuffer_;
    };

    ~PixelShader() = default;

private:
    std::shared_ptr<ID3D11PixelShader> shader_; // transmitted outward ->
    std::shared_ptr<ID3D10Blob> shaderBuffer_; // transmitted outward ->
};


class PSManager : public ShaderManagerBase<PixelShader> {
public:
    PSManager(const std::shared_ptr<Device>& devicePtr) : ShaderManagerBase(devicePtr) {};

    HRESULT LoadShader(std::shared_ptr<PixelShader>& object, const std::wstring& name,
        const std::vector<std::string>& macros = {}) {
        if (SUCCEEDED(GetShader(object, name, macros))) {
            return S_OK;
        }

        std::vector<D3D_SHADER_MACRO> d3dmacros;
        for (auto& m : macros) {
            d3dmacros.push_back({ m.c_str() });
        }
        d3dmacros.push_back({ nullptr, nullptr });

        D3DInclude includeObj;
        ID3D11PixelShader* pixelShader = nullptr;
        ID3D10Blob* pixelShaderBuffer = nullptr;
        int flags = 0;
#ifdef _DEBUG
        flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        ID3DBlob* pErrMsg = nullptr;
        HRESULT result = D3DCompileFromFile(name.c_str(), d3dmacros.data(), &includeObj, "main", "ps_5_0", flags, 0, &pixelShaderBuffer, &pErrMsg);
        if (FAILED(result) && pErrMsg != nullptr) {
            OutputDebugStringA((const char*)pErrMsg->GetBufferPointer());
            SAFE_RELEASE(pErrMsg);
        }
        if (SUCCEEDED(result)) {
            result = device_->GetDevice()->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(),
                nullptr, &pixelShader);
        }
        if (SUCCEEDED(result)) {
            object = std::make_shared<PixelShader>(pixelShader, pixelShaderBuffer);
            objects_.emplace(GenerateKey(name, macros), object);
        }
        return result;
    };

    ~PSManager() = default;
};