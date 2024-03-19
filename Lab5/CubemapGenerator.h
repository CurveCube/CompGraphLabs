#pragma once

#include "framework.h"
#include <memory>
#include "SimpleManager.h"
#include <vector>

class CubemapGenerator
{
    static const UINT sideSize = 512;
    static const UINT irradianceSideSize = 32;
    static const UINT prefilteredSideSize = 128;

    struct SimpleVertex {
        XMFLOAT3 pos;
    };

    struct CameraBuffer {
        XMMATRIX viewProjMatrix;
    };

    struct RoughnessBuffer {
        XMFLOAT4 roughness;
    };

public:
    CubemapGenerator(std::shared_ptr<ID3D11Device>&, std::shared_ptr <ID3D11DeviceContext>&,
        SimpleSamplerManager&, SimpleTextureManager&,
        SimpleILManager&, SimplePSManager&,
        SimpleVSManager&, SimpleGeometryManager&);

    HRESULT init();

    HRESULT generateEnvironmentMap(const std::string&);
    HRESULT generateIrradianceMap(const std::string&, const std::string&);
    HRESULT generatePrefilteredMap(const std::string&, const std::string&);
    HRESULT generateBRDF(const std::string&);

    void Cleanup();

    ~CubemapGenerator() {
        Cleanup();
        SAFE_RELEASE(pCameraBuffer);
        SAFE_RELEASE(pRoughnessBuffer);
    }

private:
    HRESULT createTexture(UINT);
    HRESULT createBRDFTexture(const std::string&);
    HRESULT createCubemapTexture(UINT, const std::string&, bool withMipMap);
    HRESULT createBuffer();
    HRESULT renderToCubeMap(UINT, int, ID3D11RenderTargetView*);
    HRESULT createGeometry();
    HRESULT renderMapSide(int);
    HRESULT renderIrradianceMapSide(const std::string&, int);
    HRESULT renderPrefilteredColor(const std::string&, int, int, int);
    HRESULT createCubemapRTV(const std::string&);
    HRESULT createPrefilteredRTV(const std::string&, int, int);
    HRESULT renderBRDF();

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
    ID3D11RenderTargetView* prefilteredRTV = nullptr;
    ID3D11Buffer* pRoughnessBuffer = nullptr;

    ID3D11RenderTargetView* brdfRTV = nullptr;

    DirectX::XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(XM_PI / 2, 1.0f, 0.1f, 10.0f);
    std::vector<DirectX::XMMATRIX> viewMatrices;
    std::vector<std::string> sides;
    std::vector<float> prefilteredRoughness;
};
