#pragma once

#include "ManagerStorage.hpp"
#include "Device.hpp"
#include "SwapChain.hpp"
#include "Camera.hpp"
#include "ToneMapping.h"
#include "CubemapGenerator.h"
#include "SkyBox.h"
#include "Light.hpp"
#include "Scene.h"
#include <vector>
#include <string>


class Renderer {
public:
    static constexpr int defaultWidth = 1280;
    static constexpr int defaultHeight = 720;

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
    Renderer() {
        viewport_.TopLeftX = 0;
        viewport_.TopLeftY = 0;
        viewport_.Width = defaultWidth;
        viewport_.Height = defaultHeight;
        viewport_.MinDepth = 0.0f;
        viewport_.MaxDepth = 1.0f;
    };

    HRESULT InitImgui(HWND hWnd);
    HRESULT GenerateTextures();
    void UpdateImgui();

    std::shared_ptr<IDXGIFactory> factory_; // transmitted outward ->
    std::shared_ptr<IDXGIAdapter> selectedAdapter_; // transmitted outward ->
#ifdef _DEBUG
    std::shared_ptr<ID3DUserDefinedAnnotation> annotation_; // transmitted outward ->
#endif // _DEBUG

    std::shared_ptr<Device> device_; // transmitted outward ->
    std::shared_ptr<Camera> camera_; // transmitted outward ->
    std::shared_ptr<ManagerStorage> managerStorage_; // transmitted outward ->
    std::shared_ptr<Skybox> skybox_; // transmitted outward ->

    SwapChain swapChain_;
    ToneMapping toneMapping_;
    SceneManager sceneManager_;

    std::shared_ptr<ID3D11ShaderResourceView> environmentMap_; // transmitted outward ->
    std::shared_ptr<ID3D11ShaderResourceView> irradianceMap_; // transmitted outward ->
    std::shared_ptr<ID3D11ShaderResourceView> prefilteredMap_; // transmitted outward ->
    std::shared_ptr<ID3D11ShaderResourceView> BRDF_; // transmitted outward ->

    std::vector<PointLight> lights_;
    std::shared_ptr<DirectionalLight> dirLight_; // transmitted outward ->

    D3D11_VIEWPORT viewport_;
    int width_ = defaultWidth;
    int height_ = defaultHeight;

    bool default_ = true;
    bool shadowSplits_ = false;
    
    int mousePrevX_ = -1;
    int mousePrevY_ = -1;

    XMFLOAT3 dirLightFocus = { 0.0f, 0.0f, 0.0f };
};