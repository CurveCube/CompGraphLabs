#pragma once

#include "ManagerStorage.hpp"
#include "Device.hpp"
#include "SwapChain.hpp"
#include "Camera.hpp"
#include "ToneMapping.h"
#include "CubemapGenerator.h"
#include "SkyBox.h"
#include "Light.h"
#include <vector>
#include <string>

#include "SimpleObject.hpp" // temporary, will be replaced with scene


class Renderer {
public:
    static constexpr UINT defaultWidth = 1280;
    static constexpr UINT defaultHeight = 720;

    Renderer(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;

    static Renderer& GetInstance();
    bool Init(HINSTANCE hInstance, HWND hWnd);
    bool Render();
    bool Resize(UINT width, UINT height);
    void MoveCamera(int upDown, int rightLeft, int forwardBack);
    void OnMouseWheel(int delta);
    void OnMouseRButtonDown(int x, int y);
    void OnMouseMove(int x, int y);
    void OnMouseRButtonUp();
    void Cleanup();

    ~Renderer() {
        Cleanup();
    };

private:
    Renderer();

    HRESULT InitImgui(HWND hWnd);
    HRESULT CreateDepthStencilViews();
    HRESULT GenerateTextures();
    void UpdateImgui();

    static const UINT shadowMapSize = 1024;
    D3D11_VIEWPORT shadowMapViewport_;
    D3D11_VIEWPORT viewport_;

    std::shared_ptr<IDXGIFactory> factory_; // transmitted outward ->
    std::shared_ptr<IDXGIAdapter> selectedAdapter_; // transmitted outward ->
    std::shared_ptr<Device> device_; // transmitted outward ->
    std::shared_ptr<Camera> camera_; // transmitted outward ->
    std::shared_ptr<ManagerStorage> managerStorage_; // transmitted outward ->

    std::shared_ptr<ID3D11ShaderResourceView> environmentMap_; // transmitted outward ->
    std::shared_ptr<ID3D11ShaderResourceView> irradianceMap_; // transmitted outward ->
    std::shared_ptr<ID3D11ShaderResourceView> prefilteredMap_; // transmitted outward ->
    std::shared_ptr<ID3D11ShaderResourceView> BRDF_; // transmitted outward ->

    SwapChain swapChain_;
    ToneMapping toneMapping_;
    Skybox skybox_;
    SimpleObject sphere_;

    std::vector<SpotLight> lights_;

#ifdef _DEBUG
    ID3DUserDefinedAnnotation* pAnnotation_ = nullptr; // always remains only inside the class #
#endif // _DEBUG
    ID3D11Texture2D* depthBuffer_ = nullptr; // always remains only inside the class #
    ID3D11DepthStencilView* depthStencilView_ = nullptr; // always remains only inside the class #
    ID3D11Texture2D* shadowBuffers_[CSM_SPLIT_COUNT]; // always remains only inside the class #
    ID3D11DepthStencilView* shadowViews_[CSM_SPLIT_COUNT]; // always remains only inside the class #

    bool default_ = true;
    unsigned int width_ = defaultWidth;
    unsigned int height_ = defaultHeight;
    int mousePrevX_ = -1;
    int mousePrevY_ = -1;
};