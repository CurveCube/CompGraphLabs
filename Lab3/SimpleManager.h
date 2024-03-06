#pragma once

#include "framework.h"
#include "Utilities.h"
#include <map>
#include <string>
#include <memory>


// Основа для простых менеджеров.
template<typename ST>
class SimpleManagerBase {
public:
    bool check(const std::string& key) {
        return objects_.find(key) != objects_.end();
    };

    HRESULT get(const std::string& key, std::shared_ptr<ST>& object) {
        auto result = objects_.find(key);
        if (result == objects_.end())
            return E_FAIL;
        else
        {
            object = result->second;
            return S_OK;
        }
    };

    void erase(const std::string& key) {
        objects_.erase(key);
    };

    void setDevice(const std::shared_ptr<ID3D11Device>& devicePtr) {
        device_ = devicePtr;
    };

    virtual void Cleanup() {
        device_.reset();
        objects_.clear();
    };

protected:
    SimpleManagerBase(const std::shared_ptr<ID3D11Device>& devicePtr) : device_(devicePtr) {};

    void load(const std::string& key, ST* ptr) {
        objects_.emplace(key, std::shared_ptr<ST>(ptr, utilities::DXPtrDeleter<ST*>));
    };

    virtual ~SimpleManagerBase() = default;

    std::shared_ptr<ID3D11Device> device_;
    std::map<std::string, std::shared_ptr<ST>> objects_;
};


// Простой менеджер для входных слоев.
class SimpleILManager : public SimpleManagerBase<ID3D11InputLayout> {
public:
    SimpleILManager(const std::shared_ptr<ID3D11Device>& devicePtr) : SimpleManagerBase(devicePtr) {};

    ~SimpleILManager() = default;

private:
    HRESULT loadIL(const std::string& key, const D3D11_INPUT_ELEMENT_DESC* desc, UINT numElements, ID3D10Blob* VSBuffer);

    friend class SimpleVSManager; // Создание новых InputLayout доступно только из менеджера вершинных шейдеров.
};


// Простой менеджер для вершинных шейдеров.
class SimpleVSManager : public SimpleManagerBase<ID3D11VertexShader> {
public:
    SimpleVSManager(const std::shared_ptr<ID3D11Device>& devicePtr) : SimpleManagerBase(devicePtr) {};

    // Если передан указатель на менеджер входных слоев и указатель на дескриптор, то будет создан входной слой.
    HRESULT loadVS(LPCWSTR filePath, const D3D_SHADER_MACRO* macros, const std::string& key,
        SimpleILManager* ILManager = nullptr, const D3D11_INPUT_ELEMENT_DESC* desc = nullptr, UINT numElements = 0);

    ~SimpleVSManager() = default;
};


// Простой менеджер для пиксельных шейдеров.
class SimplePSManager : public SimpleManagerBase<ID3D11PixelShader> {
public:
    SimplePSManager(const std::shared_ptr<ID3D11Device>& devicePtr) : SimpleManagerBase(devicePtr) {};

    HRESULT loadPS(LPCWSTR filePath, const D3D_SHADER_MACRO* macros, const std::string& key);

    ~SimplePSManager() = default;
};


// Простой менеджер для самплеров.
class SimpleSamplerManager : public SimpleManagerBase<ID3D11SamplerState> {
public:
    SimpleSamplerManager(const std::shared_ptr<ID3D11Device>& devicePtr) : SimpleManagerBase(devicePtr) {};

    HRESULT loadSampler(D3D11_FILTER filter, const std::string& key);

    ~SimpleSamplerManager() = default;
};


// Описание геометрии.
struct Geometry {
public:
    Geometry(ID3D11Buffer* vertexBuffer, ID3D11Buffer* indexBuffer, UINT numIndices) :
        vertexBuffer_(vertexBuffer), indexBuffer_(indexBuffer), numIndices_(numIndices) {};

    Geometry(Geometry&) = delete;
    Geometry& operator=(Geometry&) = delete;

    ID3D11Buffer* getVertexBuffer() {
        return vertexBuffer_;
    };

    ID3D11Buffer* getIndexBuffer() {
        return indexBuffer_;
    };

    UINT getNumIndices() {
        return numIndices_;
    };

    ~Geometry() {
        if (vertexBuffer_ != nullptr)
            vertexBuffer_->Release();
        if (indexBuffer_ != nullptr)
            indexBuffer_->Release();
    };

private:
    ID3D11Buffer* vertexBuffer_;
    ID3D11Buffer* indexBuffer_;
    UINT numIndices_;
};


// Простой менеджер геометрии.
class SimpleGeometryManager : public SimpleManagerBase<Geometry> {
public:
    SimpleGeometryManager(const std::shared_ptr<ID3D11Device>& devicePtr) : SimpleManagerBase(devicePtr) {};

    HRESULT loadGeometry(const void* vertices, UINT verticesBytes, const UINT* indices, UINT indicesBytes, const std::string& key);

    ~SimpleGeometryManager() = default;
};


// Текстура и views на нее.
struct SimpleTexture {
public:
    SimpleTexture(ID3D11Resource* texture, ID3D11ShaderResourceView* SRV) :
        texture_(texture), SRV_(SRV) {};

    SimpleTexture(SimpleTexture&) = delete;
    SimpleTexture& operator=(SimpleTexture&) = delete;

    ID3D11Resource* getResource() {
        return texture_;
    };

    ID3D11ShaderResourceView* getSRV() {
        return SRV_;
    };

    ~SimpleTexture() {
        if (texture_ != nullptr)
            texture_->Release();
        if (SRV_ != nullptr)
            SRV_->Release();
    };

private:
    ID3D11Resource* texture_;
    ID3D11ShaderResourceView* SRV_;
};


// Простой менеджер для текстур.
class SimpleTextureManager : public SimpleManagerBase<SimpleTexture> {
public:
    SimpleTextureManager(const std::shared_ptr<ID3D11Device>& devicePtr,
        const std::shared_ptr<ID3D11DeviceContext>& deviceContextPtr) :
        SimpleManagerBase(devicePtr), deviceContext_(deviceContextPtr) {};

    HRESULT loadTexture(LPCWSTR filePath, const std::string& key, const std::string& annotationText = "");
    HRESULT loadCubeMapTexture(LPCWSTR filePath, const std::string& key, const std::string& annotationText = "");

    void setDeviceContext(const std::shared_ptr<ID3D11DeviceContext>& deviceContextPtr) {
        deviceContext_ = deviceContextPtr;
    };

    void Cleanup() override {
        device_.reset();
        deviceContext_.reset();
        objects_.clear();
    };

    ~SimpleTextureManager() = default;

private:
    std::shared_ptr<ID3D11DeviceContext> deviceContext_;
};