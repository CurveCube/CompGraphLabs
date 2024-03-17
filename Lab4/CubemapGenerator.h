#pragma once

#include "framework.h"
#include <memory>
#include "SimpleManager.h"
#include <vector>

class CubemapGenerator
{
    static const UINT sideSize = 512;
    static const UINT irradianceSideSize = 64;

    struct SimpleVertex {
        XMFLOAT3 pos;
    };

    struct CameraBuffer {
        XMMATRIX viewProjMatrix;
    };

public:
    CubemapGenerator(std::shared_ptr<ID3D11Device>&, std::shared_ptr <ID3D11DeviceContext>&,
        SimpleSamplerManager&, SimpleTextureManager&,
        SimpleILManager&, SimplePSManager&,
        SimpleVSManager&, SimpleGeometryManager&);

    HRESULT init();

    HRESULT generateCubeMap(const std::string&);
    HRESULT generateIrradianceMap(const std::string&, const std::string&);

    void Cleanup();

    ~CubemapGenerator() {
        Cleanup();
        SAFE_RELEASE(pCameraBuffer);
    }

private:
    HRESULT createTexture(UINT);
    HRESULT createCubemapTexture(UINT, const std::string&);
    HRESULT createBuffer();
    HRESULT renderToCubeMap(UINT, int);
    HRESULT createGeometry();
    HRESULT renderMapSide(int);
    HRESULT renderIrradianceMapSide(const std::string&, int);

private:
    std::shared_ptr<ID3D11Device> device_;
    std::shared_ptr <ID3D11DeviceContext> deviceContext_;

    SimpleSamplerManager& samplerManager_;
    SimpleTextureManager& textureManager_;
    SimpleILManager& ILManager_;
    SimplePSManager& PSManager_;
    SimpleVSManager& VSManager_;
    SimpleGeometryManager& GManager_;

    ID3D11Texture2D* subTexture = nullptr;
    ID3D11RenderTargetView* subRTV = nullptr;
    ID3D11ShaderResourceView* subSRV = nullptr;
    ID3D11RenderTargetView* cubemapRTV[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    ID3D11Buffer* pCameraBuffer = nullptr;

    DirectX::XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(XM_PI / 2, 1.0f, 0.1f, 10.0f);
    std::vector<DirectX::XMMATRIX> viewMatrices;
    std::vector<std::string> sides;
};
