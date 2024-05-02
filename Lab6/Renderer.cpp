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
            height_, 0.01f, 500.0f));
        dirLight_ = std::make_shared<DirectionalLight>();
    }
    if (SUCCEEDED(result)) {
        skybox_ = std::shared_ptr<Skybox>(new Skybox());
        result = skybox_->Init(device_, managerStorage_, camera_, environmentMap_);
    }
    if (SUCCEEDED(result)) {
        result = sceneManager_.Init(device_, managerStorage_, camera_, dirLight_, width_, height_);
    }
    if (SUCCEEDED(result)) {
        UINT index = 0;
        UINT count = 0;
        result = sceneManager_.LoadScene("models/scene/untitled.gltf", index, count);
        if (SUCCEEDED(result) && (index != 0 || count != 1)) {
            result = E_FAIL;
        }
    }
    /*if (SUCCEEDED(result)) {
        result = sphere_.Init(device_, managerStorage_, camera_);
    }*/
    if (SUCCEEDED(result)) {
        result = toneMapping_.Init(device_, managerStorage_, width_, height_);
    }
    if (SUCCEEDED(result)) {
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

        result = device_->GetDevice()->CreateTexture2D(&desc, nullptr, &depthBuffer_);
    }
    if (SUCCEEDED(result)) {
        result = device_->GetDevice()->CreateDepthStencilView(depthBuffer_, nullptr, &depthStencilView_);
    }
    if (SUCCEEDED(result)) {
        result = InitImgui(hWnd);
    }

#ifdef _DEBUG 
    if (SUCCEEDED(result)) {
        result = device_->GetDeviceContext()->QueryInterface(__uuidof(pAnnotation_), reinterpret_cast<void**>(&pAnnotation_));
    }
#endif // _DEBUG

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

        std::string str = "Render mode";
        ImGui::Text(str.c_str());

        static int cur = 0;
        if (ImGui::Combo("Mode", &cur, "default\0point light fresnel\0point light ndf\0point light geometry\0shadow splits")) {
            switch (cur) {
            case 0:
                default_ = true;
                //sphere_.SetMode(SimpleObject::DEFAULT);
                sceneManager_.SetMode(SceneManager::DEFAULT);
                toneMapping_.ResetEyeAdaptation();
                break;
            case 1:
                default_ = false;
                //sphere_.SetMode(SimpleObject::FRESNEL);
                sceneManager_.SetMode(SceneManager::FRESNEL);
                toneMapping_.ResetEyeAdaptation();
                break;
            case 2:
                default_ = false;
                //sphere_.SetMode(SimpleObject::NDF);
                sceneManager_.SetMode(SceneManager::NDF);
                toneMapping_.ResetEyeAdaptation();
                break;
            case 3:
                default_ = false;
                //sphere_.SetMode(SimpleObject::GEOMETRY);
                sceneManager_.SetMode(SceneManager::GEOMETRY);
                toneMapping_.ResetEyeAdaptation();
                break;
            case 4:
                default_ = true;
                sceneManager_.SetMode(SceneManager::SHADOW_SPLITS);
                toneMapping_.ResetEyeAdaptation();
                break;
            default:
                break;
            }
        }

        if (ImGui::Checkbox("Exclude transparent", &excludeTransparent_)) {
            sceneManager_.ExcludeTransparent(excludeTransparent_);
        }

        if (default_) {
            static float factor;
            factor = toneMapping_.GetFactor();
            str = "Exposure factor";
            ImGui::DragFloat(str.c_str(), &factor, 0.01f, 0.0f, 10.0f);
            toneMapping_.SetFactor(factor);
        }

        /*str = "Object";
        ImGui::Text(str.c_str());

        static float objCol[3];
        static float objRough;
        static float objMetal;

        objCol[0] = sphere_.color_.x;
        objCol[1] = sphere_.color_.y;
        objCol[2] = sphere_.color_.z;
        str = "Color";
        ImGui::ColorEdit3(str.c_str(), objCol);
        sphere_.color_ = XMFLOAT3(objCol[0], objCol[1], objCol[2]);

        objRough = sphere_.roughness_;
        str = "Roughness";
        ImGui::DragFloat(str.c_str(), &objRough, 0.01f, 0.0f, 1.0f);
        sphere_.roughness_ = objRough;

        objMetal = sphere_.metalness_;
        str = "Metalness";
        ImGui::DragFloat(str.c_str(), &objMetal, 0.01f, 0.0f, 1.0f);
        sphere_.metalness_ = objMetal;*/

        str = "Directional light";
        ImGui::Text(str.c_str());

        static float colDir[3];
        static float brightnessDir;

        colDir[0] = dirLight_->color.x;
        colDir[1] = dirLight_->color.y;
        colDir[2] = dirLight_->color.z;
        str = "Color";
        ImGui::ColorEdit3(str.c_str(), colDir);

        brightnessDir = dirLight_->color.w;
        str = "Brightness";
        ImGui::DragFloat(str.c_str(), &brightnessDir, 1.0f, 0.0f, 50.0f);
        dirLight_->color = XMFLOAT4(colDir[0], colDir[1], colDir[2], brightnessDir);

        str = "Phi";
        ImGui::DragFloat(str.c_str(), &dirLight_->phi, 0.01f, 0.0f, XM_2PI);

        str = "Theta";
        ImGui::DragFloat(str.c_str(), &dirLight_->theta, 0.01f, -XM_PIDIV2, XM_PIDIV2);

        str = "Distance";
        ImGui::DragFloat(str.c_str(), &dirLight_->r, 1.0f, 100.0f, 300.0f);

        dirLight_->Update(camera_->GetFocusPosition());

        str = "Point lights";
        ImGui::Text(str.c_str());
        ImGui::SameLine();
        if (ImGui::Button("+")) {
            if (lights_.size() < MAX_LIGHT)
                lights_.push_back({ XMFLOAT4(15.0f, 15.0f, 15.0f, 0.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) });
        }
        ImGui::SameLine();
        if (ImGui::Button("-")) {
            if (lights_.size() > 0)
                lights_.pop_back();
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

        ImGui::End();
    }
    ImGui::Render();
}

bool Renderer::Render() {
    UpdateImgui();

#ifdef _DEBUG
    pAnnotation_->BeginEvent(L"Render_scene");
    pAnnotation_->BeginEvent(L"Preliminary_preparations");
#endif

    device_->GetDeviceContext()->ClearState();
#ifdef _DEBUG
    pAnnotation_->BeginEvent(L"Create_shadow_maps");
#endif
    if (default_) {
        if (!sceneManager_.CreateShadowMaps({ 0 })) {
            return false;
        }
    }

#ifdef _DEBUG
    pAnnotation_->EndEvent();
    pAnnotation_->BeginEvent(L"Prepare_transparent");
#endif
    if (!sceneManager_.PrepareTransparent({ 0 })) {
        return false;
    }
#ifdef _DEBUG
    pAnnotation_->EndEvent();
#endif

    device_->GetDeviceContext()->RSSetViewports(1, &viewport_);

    D3D11_RECT rect;
    rect.left = 0;
    rect.top = 0;
    rect.right = width_;
    rect.bottom = height_;
    device_->GetDeviceContext()->RSSetScissorRects(1, &rect);

    static const FLOAT backColor[4] = { 0.25f, 0.25f, 0.25f, 1.0f };
    device_->GetDeviceContext()->ClearDepthStencilView(depthStencilView_, D3D11_CLEAR_DEPTH, 0.0f, 0);

    if (default_) {
        device_->GetDeviceContext()->ClearRenderTargetView(toneMapping_.GetRenderTarget().get(), backColor);
        ID3D11RenderTargetView* rtv[] = { toneMapping_.GetRenderTarget().get() };
        device_->GetDeviceContext()->OMSetRenderTargets(1, rtv, depthStencilView_);
    }
    else {
        device_->GetDeviceContext()->ClearRenderTargetView(swapChain_.GetRenderTarget().get(), backColor);
        ID3D11RenderTargetView* rtv[] = { swapChain_.GetRenderTarget().get() };
        device_->GetDeviceContext()->OMSetRenderTargets(1, rtv, depthStencilView_);
    }

#ifdef _DEBUG
    pAnnotation_->EndEvent();
    pAnnotation_->BeginEvent(L"Draw_scene");
#endif

    /*if (!sphere_.Render(irradianceMap_, prefilteredMap_, BRDF_, lights_)) {
        return false;
    }*/

    if (!sceneManager_.Render(irradianceMap_, prefilteredMap_, BRDF_, skybox_, lights_, { 0 })) {
        return false;
    }

    if (default_) {
#ifdef _DEBUG
        pAnnotation_->EndEvent();
        pAnnotation_->BeginEvent(L"Tone_mapping");
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
    pAnnotation_->EndEvent();
#endif

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    HRESULT result = swapChain_.Present();

#ifdef _DEBUG
    pAnnotation_->EndEvent();
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

    SAFE_RELEASE(depthBuffer_);
    SAFE_RELEASE(depthStencilView_);
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

    HRESULT result = device_->GetDevice()->CreateTexture2D(&desc, nullptr, &depthBuffer_);
    if (SUCCEEDED(result)) {
        result = device_->GetDevice()->CreateDepthStencilView(depthBuffer_, nullptr, &depthStencilView_);
    }
    if (FAILED(result)) {
        return false;
    }

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
    float dx = camera_->GetDistanceToFocus() * forwardBack / 30.0f,
        dy = camera_->GetDistanceToFocus() * upDown / 30.0f,
        dz = -camera_->GetDistanceToFocus() * rightLeft / 30.0f;
    camera_->Move(dx, dy, dz);
    dirLight_->Update(camera_->GetFocusPosition());
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
        dirLight_->Update(camera_->GetFocusPosition());
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

    //sphere_.Cleanup();
    toneMapping_.Cleanup();
    sceneManager_.Cleanup();
    swapChain_.Cleanup();

    SAFE_RELEASE(depthBuffer_);
    SAFE_RELEASE(depthStencilView_);
    
#ifdef _DEBUG
    SAFE_RELEASE(pAnnotation_);
#endif

    factory_.reset();
    selectedAdapter_.reset();
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