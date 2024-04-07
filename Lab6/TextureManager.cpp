#pragma once

#include "TextureManager.h"

#define STB_IMAGE_IMPLEMENTATION
#include "tinygltf/stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION

HRESULT TextureManager::LoadTexture(std::shared_ptr<Texture>& texture, const std::string& name) {
    if (SUCCEEDED(GetTexture(texture, name))) {
        return S_OK;
    }

    if (!device_->IsInit()) {
        return E_FAIL;
    }

    int width, height, nrComponents;
    unsigned char* data = stbi_load(name.c_str(), &width, &height, &nrComponents, 4);
    if (!data) {
        return E_FAIL;
    }

    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA initData;
    initData.pSysMem = data;
    initData.SysMemPitch = width * sizeof(unsigned char) * 4;
    initData.SysMemSlicePitch = width * height * sizeof(unsigned char) * 4;

    ID3D11Texture2D* tex;
    ID3D11ShaderResourceView* SRV;
    ID3D11ShaderResourceView* SRVSRGB;
    HRESULT result = device_->GetDevice()->CreateTexture2D(&textureDesc, &initData, &tex);
    if (SUCCEEDED(result)) {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipLevels = 1;
        desc.Texture2D.MostDetailedMip = 0;
        result = device_->GetDevice()->CreateShaderResourceView(tex, &desc, &SRV);
    }
    if (SUCCEEDED(result)) {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipLevels = 1;
        desc.Texture2D.MostDetailedMip = 0;
        result = device_->GetDevice()->CreateShaderResourceView(tex, &desc, &SRVSRGB);
    }
    if (SUCCEEDED(result)) {
        texture = std::make_shared<Texture>(tex, SRV, SRVSRGB);
        textures_.emplace(name, texture);
    }

    stbi_image_free(data);

    return result;
};

HRESULT TextureManager::LoadHDRTexture(std::shared_ptr<Texture>& texture, const std::string& name) {
    if (SUCCEEDED(GetTexture(texture, name))) {
        return S_OK;
    }

    if (!device_->IsInit()) {
        return E_FAIL;
    }

    int width, height, nrComponents;
    float* data = stbi_loadf(name.c_str(), &width, &height, &nrComponents, 4);
    if (!data) {
        return E_FAIL;
    }

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

    ID3D11Texture2D* tex;
    ID3D11ShaderResourceView* SRV;
    HRESULT result = device_->GetDevice()->CreateTexture2D(&textureDesc, &initData, &tex);
    if (SUCCEEDED(result)) {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
        desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipLevels = 1;
        desc.Texture2D.MostDetailedMip = 0;
        result = device_->GetDevice()->CreateShaderResourceView(tex, &desc, &SRV);
    }
    if (SUCCEEDED(result)) {
        texture = std::make_shared<Texture>(tex, SRV);
        textures_.emplace(name, texture);
    }

    stbi_image_free(data);

    return result;
};