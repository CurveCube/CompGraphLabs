#pragma once

#include "framework.h"
#include "Camera.h"
#include "Input.h"

struct Vertex {
    float x, y, z;
    float u, v;
};

struct WorldMatrixBuffer {
    XMMATRIX worldMatrix;
};

struct ViewMatrixBuffer {
    XMMATRIX viewProjectionMatrix;
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

    HRESULT InitScene();
    void InputHandler();
    bool UpdateScene();

    ID3D11Device* pDevice_;
    ID3D11DeviceContext* pDeviceContext_;
    IDXGISwapChain* pSwapChain_;
    ID3D11RenderTargetView* pRenderTargetView_;

    ID3D11Buffer* pVertexBuffer_;
    ID3D11Buffer* pIndexBuffer_;
    ID3D11InputLayout* pInputLayout_;
    ID3D11VertexShader* pVertexShader_;
    ID3D11PixelShader* pPixelShader_;

    ID3D11Buffer* pWorldMatrixBuffer_;
    ID3D11Buffer* pViewMatrixBuffer_;
    ID3D11RasterizerState* pRasterizerState_;
    ID3D11SamplerState* pSampler_;
    ID3D11Resource* pTexture_;
    ID3D11ShaderResourceView* pTextureView_;
#ifdef _DEBUG
    ID3DUserDefinedAnnotation* pAnnotation_;
#endif // _DEBUG

    Camera* pCamera_;
    Input* pInput_;

    UINT width_;
    UINT height_;
};