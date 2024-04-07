#pragma once

#include "Device.hpp"
#include <map>
#include <vector>
#include <string>


class Texture {
public:
    Texture(ID3D11Texture2D* texture, ID3D11ShaderResourceView* SRV, ID3D11ShaderResourceView* SRVSRGB) :
        texture_(texture, utilities::DXPtrDeleter<ID3D11Texture2D*>), SRV_(SRV, utilities::DXPtrDeleter<ID3D11ShaderResourceView*>),
        SRVSRGB_(SRVSRGB, utilities::DXPtrDeleter<ID3D11ShaderResourceView*>) {};

    std::shared_ptr<ID3D11Texture2D> GetTexture() const {
        return texture_;
    };

    std::shared_ptr<ID3D11ShaderResourceView> GetSRV(bool isSRGB = false) const {
        if (isSRGB) {
            return SRVSRGB_;
        }
        else {
            return SRV_;
        }
    };

    HRESULT SetAnnotationText(const std::string& annotationText = "") {
        return texture_->SetPrivateData(WKPDID_D3DDebugObjectName, annotationText.size(), annotationText.c_str());
    };

    bool IsHDR() const {
        D3D11_TEXTURE2D_DESC texDesc;
        texture_->GetDesc(&texDesc);
        return texDesc.Format == DXGI_FORMAT_R32G32B32A32_FLOAT;
    };

    ~Texture() = default;

private:
    std::shared_ptr<ID3D11Texture2D> texture_; // transmitted outward ->
    std::shared_ptr<ID3D11ShaderResourceView> SRV_; // transmitted outward ->
    std::shared_ptr<ID3D11ShaderResourceView> SRVSRGB_; // transmitted outward ->
};


class TextureManager {
public:
    TextureManager(const std::shared_ptr<Device>& device) : device_(device) {};

    bool CheckTexture(const std::string& name) const {
        return textures_.find(name) != textures_.end();
    };

    HRESULT LoadTexture(std::shared_ptr<Texture>& texture, const std::string& name);

    HRESULT LoadTexture(const std::string& name) {
        std::shared_ptr<Texture> texture;
        return LoadTexture(texture, name);
    };

    HRESULT LoadHDRTexture(std::shared_ptr<Texture>& texture, const std::string& name);

    HRESULT LoadHDRTexture(const std::string& name) {
        std::shared_ptr<Texture> texture;
        return LoadHDRTexture(texture, name);
    };

    HRESULT GetTexture(std::shared_ptr<Texture>& texture, const std::string& name) const {
        auto result = textures_.find(name);
        if (result == textures_.end()) {
            return E_FAIL;
        }
        else {
            texture = result->second;
            return S_OK;
        }
    };

    void ClearTextures() {
        textures_.clear();
    };

    void Cleanup() {
        device_.reset();
        ClearTextures();
    };

    ~TextureManager() = default;

private:
    std::shared_ptr<Device> device_; // provided externally <-
    std::map<std::string, std::shared_ptr<Texture>> textures_; // textures are transmitted outward ->
};