#pragma once

#include "framework.h"
#include "Camera.h"
#include "Input.h"
#include "Skybox.h"
#include "SimpleObject.h"
#include "ToneMapping.h"
#include <vector>
#include <string>

#define MAX_LIGHT 10

struct WorldMatrixBuffer {
    XMMATRIX worldMatrix;
    XMFLOAT3 color;
    float roughness;
    float metalness;
};

struct SkyboxWorldMatrixBuffer {
    XMMATRIX worldMatrix;
    XMFLOAT4 size;
};

struct Light {
    XMFLOAT4 pos;
    XMFLOAT4 color;
};

struct ViewMatrixBuffer {
    XMMATRIX viewProjectionMatrix;
    XMFLOAT4 cameraPos;
    XMINT4 lightParams;
    Light lights[MAX_LIGHT];
    XMFLOAT4 ambientColor;
};

class Renderer {
public:
    static constexpr UINT defaultWidth = 1280;
    static constexpr UINT defaultHeight = 720;

    static Renderer& GetInstance();
    Renderer(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;

    bool Init(HINSTANCE hInstance, HWND hWnd);
    bool Render();
    bool Resize(UINT width, UINT height);

    void Cleanup();
    ~Renderer();

private:
    Renderer();

    HRESULT LoadGeometry();
    HRESULT LoadShaders();
    HRESULT LoadTextures();
    HRESULT CreateSamplers();
    HRESULT InitSkybox();
    HRESULT InitObjects();
    HRESULT CreateDevice();
    HRESULT CreateSwapChain(HWND hWnd);
    HRESULT InitImgui(HWND hWnd);
    HRESULT CreateVMBuffer();
    HRESULT CreateWMBuffer();
    HRESULT CreateSWMBuffer();
    void InputHandler();
    bool UpdateScene();
    void UpdateImgui();
    void RenderSkybox();
    void RenderObjects();
    bool ResizeSwapChain();
    void ResizeSkybox();

    std::shared_ptr<ID3D11Device> pDevice_;
    std::shared_ptr<ID3D11DeviceContext> pDeviceContext_;
    std::shared_ptr<ID3D11SamplerState> pSampler_;

    IDXGIFactory* pFactory_ = nullptr;
    IDXGIAdapter* pSelectedAdapter_ = nullptr;
    IDXGISwapChain* pSwapChain_ = nullptr;
    ID3D11RenderTargetView* pRenderTargetView_ = nullptr;
    ID3D11RasterizerState* pRasterizerState_ = nullptr;

    SimpleGeometryManager pGeometryManager_;
    SimpleILManager pILManager_;
    SimplePSManager pPSManager_;
    SimpleVSManager pVSManager_;
    SimpleSamplerManager pSamplerManager_;
    SimpleTextureManager pTextureManager_;

    static const D3D11_INPUT_ELEMENT_DESC SimpleVertexDesc[];
    static const D3D11_INPUT_ELEMENT_DESC VertexDesc[];

    SimpleObject<Vertex> sphere;
    Skybox skybox;

    ID3D11Buffer* pWorldMatrixBuffer_ = nullptr;
    ID3D11Buffer* pSkyboxWorldMatrixBuffer_ = nullptr;
    ID3D11Buffer* pViewMatrixBuffer_ = nullptr;
#ifdef _DEBUG
    ID3DUserDefinedAnnotation* pAnnotation_ = nullptr;
#endif // _DEBUG

    Camera* pCamera_ = nullptr;
    Input* pInput_ = nullptr;

    std::vector<Light> lights_;

    UINT width_;
    UINT height_;

    ToneMapping toneMapping_;
    bool default_ = true;
};