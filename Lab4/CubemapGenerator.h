#pragma once

#include "framework.h"
#include <memory>
#include "SimpleManager.h"
#include <vector>

class CubemapGenerator
{
    static const UINT sideSize = 512;
    static const UINT irradianceSideSize = 32;

    struct SimpleVertex {
        XMFLOAT3 pos;
    };

    struct CameraBuffer {
        XMMATRIX viewProjMatrix;
    };

public:
    CubemapGenerator(std::shared_ptr<ID3D11Device>& device, std::shared_ptr <ID3D11DeviceContext>& deviceContext);

    HRESULT generateCubeMap(SimpleSamplerManager&, SimpleTextureManager&,
        SimpleILManager&, SimplePSManager&, 
        SimpleVSManager&, SimpleGeometryManager&,
        const std::string&);

    void Cleanup();

    ~CubemapGenerator() {
        Cleanup();
    }

private:
    HRESULT createHdrMappedTexture();
    HRESULT createCubemapTexture(SimpleTextureManager&, const std::string&);
    HRESULT createBuffer();
    HRESULT renderToCubeMap(SimplePSManager&, SimpleVSManager&, SimpleSamplerManager&, int);
    HRESULT createGeometry(SimpleGeometryManager&);
    HRESULT renderSubHdr(SimplePSManager&, SimpleVSManager&, 
        SimpleSamplerManager&, SimpleTextureManager&, 
        SimpleILManager&, SimpleGeometryManager&, int);

private:
    std::shared_ptr<ID3D11Device> device_;
    std::shared_ptr <ID3D11DeviceContext> deviceContext_;

    ID3D11Texture2D* subHdrMappedTexture = nullptr;
    ID3D11RenderTargetView* subHdrMappedRTV = nullptr;
    ID3D11ShaderResourceView* subHdrMappedSRV = nullptr;
    ID3D11RenderTargetView* cubemapRTV[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    ID3D11Buffer* pCameraBuffer = nullptr;

    DirectX::XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(XM_PI / 2, 1.0f, 0.1f, 10.0f);
    std::vector<DirectX::XMMATRIX> viewMatrices;
    std::vector<std::string> sides;
};
