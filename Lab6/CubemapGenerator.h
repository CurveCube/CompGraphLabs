#pragma once

#include "ManagerStorage.hpp"


class CubemapGenerator {
    static const UINT sideSize = 512;
    static const UINT irradianceSideSize = 32;
    static const UINT prefilteredSideSize = 128;

    enum Sides {
        XPLUS,
        XMINUS,
        YPLUS,
        YMINUS,
        ZPLUS,
        ZMINUS
    };

    struct Vertex {
        XMFLOAT3 pos;
    };

    struct ViewMatrixBuffer {
        XMMATRIX viewProjectionMatrix;
    };

    struct RoughnessBuffer {
        XMFLOAT4 roughness;
    };

    struct Side {
        ID3D11Buffer* vertexBuffer_ = nullptr;
        ID3D11Buffer* indexBuffer_ = nullptr;
        UINT numIndices_ = 6;
    };

public:
    CubemapGenerator(const std::shared_ptr<Device>& device, const std::shared_ptr<ManagerStorage>& managerStorage);

    HRESULT Init();
    HRESULT GenerateEnvironmentMap(const std::string& hdrname, std::shared_ptr<ID3D11ShaderResourceView>& environmentMap);
    HRESULT GenerateIrradianceMap(std::shared_ptr<ID3D11ShaderResourceView>& irradianceMap);
    HRESULT GeneratePrefilteredMap(std::shared_ptr<ID3D11ShaderResourceView>& prefilteredMap);
    HRESULT GenerateBRDF(std::shared_ptr<ID3D11ShaderResourceView>& BRDF);
    void Cleanup();

    bool IsInit() {
        return !!viewMatrixBuffer_;
    };

    ~CubemapGenerator() {
        Cleanup();
    }

private:
    HRESULT CreateSides();
    HRESULT CreateSide(const std::vector<Vertex>& vertices, const std::vector<UINT>& indices);
    HRESULT CreateBuffers();
    HRESULT CreateCubemapTexture(UINT size, ID3D11Texture2D** texture, std::shared_ptr<ID3D11ShaderResourceView>& SRV, bool withMipMap = false);
    HRESULT CreateCubemapSubRTV(ID3D11Texture2D* texture, Sides side);
    HRESULT CreatePrefilteredSubRTV(Sides side, int mipSlice);
    HRESULT CreateBRDFTexture();
    HRESULT RenderEnvironmentMapSide(Sides side);
    HRESULT RenderIrradianceMapSide(Sides side);
    HRESULT RenderPrefilteredMap(Sides side, int mipSlice, int mipMapSize);
    HRESULT RenderBRDF();
    void CleanupSubResources();

    static const std::vector<D3D11_INPUT_ELEMENT_DESC> VertexDesc;

    std::shared_ptr<Device> device_; // provided externally <-
    std::shared_ptr<ManagerStorage> managerStorage_; // provided externally <-
    std::shared_ptr<Texture> hdrtexture_; // provided externally <-

    ID3D11Buffer* viewMatrixBuffer_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* roughnessBuffer_ = nullptr; // always remains only inside the class #

    std::shared_ptr<ID3D11ShaderResourceView> environmentMap_; // transmitted outward ->
    std::shared_ptr<ID3D11ShaderResourceView> irradianceMap_; // transmitted outward ->
    std::shared_ptr<ID3D11ShaderResourceView> prefilteredMap_; // transmitted outward ->
    std::shared_ptr<ID3D11ShaderResourceView> BRDF_; // transmitted outward ->

    std::shared_ptr<ID3D11RasterizerState> rasterizerState_; // provided externally <-
    std::shared_ptr<ID3D11SamplerState> samplerDefault_; // provided externally <-
    std::shared_ptr<ID3D11SamplerState> samplerAvg_; // provided externally <-

    std::vector<Side> sides_; // always remains only inside the class #

    ID3D11Texture2D* environmentMapTexture_ = nullptr; // always remains only inside the class #
    ID3D11Texture2D* irradianceMapTexture_ = nullptr; // always remains only inside the class #
    ID3D11Texture2D* prefilteredMapTexture_ = nullptr; // always remains only inside the class #
    ID3D11Texture2D* BRDFTexture_ = nullptr; // always remains only inside the class #
    ID3D11RenderTargetView* subRTV_ = nullptr; // always remains only inside the class #

    XMMATRIX projectionMatrix_ = XMMatrixPerspectiveFovLH(XM_PI / 2, 1.0f, 0.1f, 10.0f);
    std::vector<XMMATRIX> viewMatrices_;
    std::vector<float> prefilteredRoughness_;
};