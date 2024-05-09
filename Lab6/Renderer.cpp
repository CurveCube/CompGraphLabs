#include "Renderer.h"

Renderer& Renderer::GetInstance() {
    static Renderer instance;
    return instance;
}

bool Renderer::Init(HINSTANCE hInstance, HWND hWnd) {
    IDXGIFactory* factory = nullptr;
    IDXGIAdapter* adapter = nullptr;
    HRESULT result = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
    if (SUCCEEDED(result)) {
        IDXGIAdapter* currentAdapter = nullptr;
        UINT adapterIdx = 0;
        while (SUCCEEDED(factory->EnumAdapters(adapterIdx, &currentAdapter))) {
            DXGI_ADAPTER_DESC desc;
            result = currentAdapter->GetDesc(&desc);
            if (SUCCEEDED(result) && wcscmp(desc.Description, L"Microsoft Basic Render Driver")) {
                adapter = currentAdapter;
                break;
            }
            currentAdapter->Release();
            adapterIdx++;
        }
    }
    if (adapter == nullptr) {
        return E_FAIL;
    }
    if (SUCCEEDED(result)) {
        factory_ = std::shared_ptr<IDXGIFactory>(factory, utilities::DXPtrDeleter<IDXGIFactory*>);
        selectedAdapter_ = std::shared_ptr<IDXGIAdapter>(adapter, utilities::DXPtrDeleter<IDXGIAdapter*>);
        device_ = std::shared_ptr<Device>(new Device());
        result = device_->Init(selectedAdapter_);
    }
#ifdef _DEBUG 
    if (SUCCEEDED(result)) {
        ID3DUserDefinedAnnotation* annotationPtr = nullptr;
        result = device_->GetDeviceContext()->QueryInterface(__uuidof(annotationPtr), reinterpret_cast<void**>(&annotationPtr));
        if (SUCCEEDED(result)) {
            annotation_ = std::shared_ptr<ID3DUserDefinedAnnotation>(annotationPtr, utilities::DXPtrDeleter<ID3DUserDefinedAnnotation*>);
        }
    }
#endif
    if (SUCCEEDED(result)) {
        result = swapChain_.Init(factory_, device_, hWnd, width_, height_);
    }
    if (SUCCEEDED(result)) {
        managerStorage_ = std::shared_ptr<ManagerStorage>(new ManagerStorage());
        result = managerStorage_->Init(device_);
    }
    if (SUCCEEDED(result)) {
        result = GenerateTextures();
    }
    if (SUCCEEDED(result)) {
        camera_ = std::shared_ptr<Camera>(new Camera(XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 100.0f, 50.0f), XM_PI / 3, width_,
            height_, 0.01f, 800.0f));
        dirLight_ = std::make_shared<DirectionalLight>();
    }
    if (SUCCEEDED(result)) {
        skybox_ = std::shared_ptr<Skybox>(new Skybox());
        result = skybox_->Init(device_, managerStorage_, camera_, environmentMap_);
    }
    if (SUCCEEDED(result)) {
#ifdef _DEBUG
        result = sceneManager_.Init(device_, managerStorage_, camera_, dirLight_, width_, height_, annotation_);
#else
        result = sceneManager_.Init(device_, managerStorage_, camera_, dirLight_, width_, height_);
#endif
    }
    if (SUCCEEDED(result)) {
        UINT index = 0;
        UINT count = 0;
        result = sceneManager_.LoadScene("models/scene/untitled.gltf", index, count);
        if (SUCCEEDED(result) && (index != 0 || count != 1)) {
            result = E_FAIL;
        }
    }
    if (SUCCEEDED(result)) {
        result = toneMapping_.Init(device_, managerStorage_, width_, height_);
    }
    if (SUCCEEDED(result)) {
        result = InitImgui(hWnd);
    }

    if (FAILED(result)) {
        Cleanup();
    }

    return SUCCEEDED(result);
}

HRESULT Renderer::GenerateTextures() {
    CubemapGenerator cubeMapGen(device_, managerStorage_);
    HRESULT result = cubeMapGen.Init();
    if (SUCCEEDED(result)) {
        result = cubeMapGen.GenerateEnvironmentMap("textures/hdr_text.hdr", environmentMap_);
    }
    if (SUCCEEDED(result)) {
        result = cubeMapGen.GenerateIrradianceMap(irradianceMap_);
    }
    if (SUCCEEDED(result)) {
        result = cubeMapGen.GeneratePrefilteredMap(prefilteredMap_);
    }
    if (SUCCEEDED(result)) {
        result = cubeMapGen.GenerateBRDF(BRDF_);
    }
    return result;
}

HRESULT Renderer::InitImgui(HWND hWnd) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    bool result = ImGui_ImplWin32_Init(hWnd);
    if (result) {
        result = ImGui_ImplDX11_Init(device_->GetDevice().get(), device_->GetDeviceContext().get());
    }
    return result ? S_OK : E_FAIL;
}

void Renderer::UpdateImgui() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    static bool window = true;

    if (window) {
        ImGui::Begin("ImGui", &window);

        ImGui::Text("Render mode");

        static int cur = 0;
        if (ImGui::Combo("Mode", &cur, "default\0point light fresnel\0point light ndf\0point light geometry\0shadow splits\0ssao mask")) {
            switch (cur) {
            case 0:
                default_ = true;
                shadowSplits_ = false;
                sceneManager_.withSSAO = true;
                sceneManager_.SetMode(SceneManager::Mode::DEFAULT);
                toneMapping_.ResetEyeAdaptation();
                break;
            case 1:
                default_ = false;
                shadowSplits_ = false;
                sceneManager_.withSSAO = false;
                sceneManager_.SetMode(SceneManager::Mode::FRESNEL);
                toneMapping_.ResetEyeAdaptation();
                break;
            case 2:
                default_ = false;
                shadowSplits_ = false;
                sceneManager_.withSSAO = false;
                sceneManager_.SetMode(SceneManager::Mode::NDF);
                toneMapping_.ResetEyeAdaptation();
                break;
            case 3:
                default_ = false;
                shadowSplits_ = false;
                sceneManager_.withSSAO = false;
                sceneManager_.SetMode(SceneManager::Mode::GEOMETRY);
                toneMapping_.ResetEyeAdaptation();
                break;
            case 4:
                default_ = false;
                shadowSplits_ = true;
                sceneManager_.withSSAO = false;
                sceneManager_.SetMode(SceneManager::Mode::SHADOW_SPLITS);
                toneMapping_.ResetEyeAdaptation();
                break;
            case 5:
                default_ = false;
                shadowSplits_ = false;
                sceneManager_.withSSAO = true;
                sceneManager_.SetMode(SceneManager::Mode::SSAO_MASK);
                toneMapping_.ResetEyeAdaptation();
                break;
            default:
                break;
            }
        }

        ImGui::Checkbox("Deferred render", &sceneManager_.deferredRender);
        ImGui::Checkbox("Exclude transparent", &sceneManager_.excludeTransparent);

        if (default_) {
            static float factor;
            factor = toneMapping_.GetFactor();
            ImGui::DragFloat("Exposure factor", &factor, 0.01f, 0.0f, 10.0f);
            toneMapping_.SetFactor(factor);

            ImGui::Checkbox("With SSAO", &sceneManager_.withSSAO);
        }

        if (sceneManager_.withSSAO) {
            ImGui::DragFloat("SSAO radius", &sceneManager_.SSAORadius, 0.01f, 0.0f, 10.0f);

            ImGui::DragFloat("SSAO depth limit", &sceneManager_.SSAODepthLimit, 0.000001f, 0.0f, 0.00001f, "%.6f");
        }

        if (default_ || shadowSplits_) {
            ImGui::Text("Directional light");

            static float colDir[3];
            static float brightnessDir;

            colDir[0] = dirLight_->color.x;
            colDir[1] = dirLight_->color.y;
            colDir[2] = dirLight_->color.z;
            ImGui::ColorEdit3("Color", colDir);

            brightnessDir = dirLight_->color.w;
            ImGui::DragFloat("Brightness", &brightnessDir, 1.0f, 0.0f, 50.0f);
            dirLight_->color = XMFLOAT4(colDir[0], colDir[1], colDir[2], brightnessDir);

            ImGui::DragFloat("Phi", &dirLight_->phi, 0.01f, 0.0f, XM_2PI);

            ImGui::DragFloat("Theta", &dirLight_->theta, 0.01f, -XM_PIDIV2, XM_PIDIV2);

            ImGui::DragFloat("Distance", &dirLight_->r, 1.0f, 100.0f, 300.0f);

            static float focus[3];
            focus[0] = dirLightFocus.x;
            focus[1] = dirLightFocus.y;
            focus[2] = dirLightFocus.z;
            ImGui::DragFloat3("Directional light focus position", focus, 1.0f, -115.0f, 115.0f);
            dirLightFocus = XMFLOAT3(focus[0], focus[1], focus[2]);
            dirLight_->Update(dirLightFocus);

            ImGui::DragInt("Shadow depth bias", &sceneManager_.depthBias, 1, 0, 32);

            ImGui::DragFloat("Shadow slope scale bias", &sceneManager_.slopeScaleBias, 0.1f, 0.0f, 10.0f);
        }

        if (!shadowSplits_ && (default_ || !sceneManager_.withSSAO)) {
            ImGui::Text("Point lights");
            ImGui::SameLine();
            if (ImGui::Button("+")) {
                if (lights_.size() < MAX_LIGHT)
                    lights_.push_back({ XMFLOAT4(15.0f, 15.0f, 15.0f, 0.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 3000.0f) });
            }
            ImGui::SameLine();
            if (ImGui::Button("-")) {
                if (lights_.size() > 0)
                    lights_.pop_back();
            }

            if (sceneManager_.deferredRender) {
                ImGui::DragFloat("Smooth clamp point light radiance limit", &sceneManager_.smoothClampRadianceLimit, 0.01f, 0.2f, 10.0f);
                ImGui::DragFloat("Clamp point light radiance limit", &sceneManager_.clampRadianceLimit, 0.01f, 0.1f, 10.0f);
            }

            static float col[MAX_LIGHT][3];
            static float pos[MAX_LIGHT][3];
            static float brightness[MAX_LIGHT];
            for (int i = 0; i < lights_.size(); i++) {
                std::string str = "Light " + std::to_string(i);
                ImGui::Text(str.c_str());

                pos[i][0] = lights_[i].pos.x;
                pos[i][1] = lights_[i].pos.y;
                pos[i][2] = lights_[i].pos.z;
                str = "Pos " + std::to_string(i);
                ImGui::DragFloat3(str.c_str(), pos[i], 1.0f, -115.0f, 115.0f);
                lights_[i].pos = XMFLOAT4(pos[i][0], pos[i][1], pos[i][2], 1.0f);

                col[i][0] = lights_[i].color.x;
                col[i][1] = lights_[i].color.y;
                col[i][2] = lights_[i].color.z;
                str = "Color " + std::to_string(i);
                ImGui::ColorEdit3(str.c_str(), col[i]);
                lights_[i].color = XMFLOAT4(col[i][0], col[i][1], col[i][2], lights_[i].color.w);

                brightness[i] = lights_[i].color.w;
                str = "Brightness " + std::to_string(i);
                ImGui::DragFloat(str.c_str(), &brightness[i], 10.0f, 1.0f, 10000.0f);
                lights_[i].color.w = brightness[i];
            }
        }

        ImGui::End();
    }
    ImGui::Render();
}

bool Renderer::Render() {
    UpdateImgui();

#ifdef _DEBUG
    annotation_->BeginEvent(L"Render_scene");
#endif

    device_->GetDeviceContext()->ClearState();

    D3D11_RECT rect;
    rect.left = 0;
    rect.top = 0;
    rect.right = width_;
    rect.bottom = height_;
    device_->GetDeviceContext()->RSSetScissorRects(1, &rect);

    static const FLOAT backColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (default_) {
        device_->GetDeviceContext()->ClearRenderTargetView(toneMapping_.GetRenderTarget().get(), backColor);
        sceneManager_.SetRenderTarget(toneMapping_.GetRenderTarget());
    }
    else {
        device_->GetDeviceContext()->ClearRenderTargetView(swapChain_.GetRenderTarget().get(), backColor);
        sceneManager_.SetRenderTarget(swapChain_.GetRenderTarget());
    }
    sceneManager_.SetViewport(viewport_);

    if (!sceneManager_.Render(irradianceMap_, prefilteredMap_, BRDF_, skybox_, lights_, { 0 })) {
        return false;
    }

    if (default_) {
#ifdef _DEBUG
        annotation_->BeginEvent(L"Tone_mapping");
#endif

        toneMapping_.CalculateBrightness();

        ID3D11RenderTargetView* rtv[] = { swapChain_.GetRenderTarget().get() };
        device_->GetDeviceContext()->OMSetRenderTargets(1, rtv, nullptr);
        device_->GetDeviceContext()->RSSetViewports(1, &viewport_);

        if (!toneMapping_.RenderTonemap()) {
            return false;
        }
    }

#ifdef _DEBUG
    annotation_->EndEvent();
#endif

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    HRESULT result = swapChain_.Present();

#ifdef _DEBUG
    annotation_->EndEvent();
#endif

    return SUCCEEDED(result);
}

bool Renderer::Resize(UINT width, UINT height) {
    if (!device_ || !swapChain_.IsInit()) {
        return false;
    }

    width_ = max(width, 8);
    height_ = max(height, 8);
    viewport_.Width = width_;
    viewport_.Height = height_;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Format = DXGI_FORMAT_D32_FLOAT;
    desc.ArraySize = 1;
    desc.MipLevels = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.Height = height_;
    desc.Width = width_;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;

    camera_->Resize(width_, height_);
    if (!swapChain_.Resize(width_, height_)) {
        return false;
    }
    if (!skybox_->Resize(width_, height_)) {
        return false;
    }
    if (!toneMapping_.Resize(width_, height_)) {
        return false;
    }
    if (!sceneManager_.Resize(width_, height_)) {
        return false;
    }

    return true;
}

void Renderer::MoveCamera(int upDown, int rightLeft, int forwardBack) {
    float dx = camera_->GetDistanceToFocus() * forwardBack / 50.0f,
        dy = camera_->GetDistanceToFocus() * upDown / 50.0f,
        dz = -camera_->GetDistanceToFocus() * rightLeft / 50.0f;
    camera_->Move(dx, dy, dz);
}

void Renderer::OnMouseWheel(int delta) {
    camera_->Zoom(-delta / 100.0f);
}

void Renderer::OnMouseRButtonDown(int x, int y) {
    mousePrevX_ = x;
    mousePrevY_ = y;
}

void Renderer::OnMouseMove(int x, int y) {
    if (mousePrevX_ >= 0) {
        camera_->Rotate((mousePrevX_ - x) / 200.0f, (y - mousePrevY_) / 200.0f);
        mousePrevX_ = x;
        mousePrevY_ = y;
    }
}

void Renderer::OnMouseRButtonUp() {
    mousePrevX_ = -1;
    mousePrevY_ = -1;
}

void Renderer::Cleanup() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    toneMapping_.Cleanup();
    sceneManager_.Cleanup();
    swapChain_.Cleanup();

    factory_.reset();
    selectedAdapter_.reset();
#ifdef _DEBUG
    annotation_.reset();
#endif
    camera_.reset();
    dirLight_.reset();
    managerStorage_.reset();
    skybox_.reset();
    environmentMap_.reset();
    irradianceMap_.reset();
    prefilteredMap_.reset();
    BRDF_.reset();
    device_.reset();
}