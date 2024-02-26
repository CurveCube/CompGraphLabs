#include "Renderer.h"
#include <string>

#define SAFE_RELEASE(A) if ((A) != NULL) { (A)->Release(); (A) = NULL; }

Renderer& Renderer::GetInstance() {
    static Renderer instance;
    return instance;
}

Renderer::Renderer() :
    pDevice_(NULL),
    pDeviceContext_(NULL),
    pSwapChain_(NULL),
    pRenderTargetView_(NULL),
    pVertexBuffer_(NULL),
    pIndexBuffer_(NULL),
    pInputLayout_(NULL),
    pVertexShader_(NULL),
    pPixelShader_(NULL),
    pWorldMatrixBuffer_(NULL),
    pViewMatrixBuffer_(NULL),
    pRasterizerState_(NULL),
    pSampler_(NULL),
    pTexture_(NULL),
    pTextureView_(NULL),
#ifdef _DEBUG // Для добавления маркеров событий в отладочной сборке
    pAnnotation_(NULL),
#endif // _DEBUG
    pCamera_(NULL),
    pInput_(NULL),
    width_(defaultWidth),
    height_(defaultHeight) {}

bool Renderer::Init(HINSTANCE hInstance, HWND hWnd) {
    // Create a DirectX graphics interface factory.​
    IDXGIFactory* pFactory = nullptr;
    HRESULT result = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory);
    // Select hardware adapter
    IDXGIAdapter* pSelectedAdapter = NULL;
    if (SUCCEEDED(result)) {
        IDXGIAdapter* pAdapter = NULL;
        UINT adapterIdx = 0;
        while (SUCCEEDED(pFactory->EnumAdapters(adapterIdx, &pAdapter))) {
            DXGI_ADAPTER_DESC desc;
            result = pAdapter->GetDesc(&desc);
            if (SUCCEEDED(result) && wcscmp(desc.Description, L"Microsoft Basic Render Driver")) {
                pSelectedAdapter = pAdapter;
                break;
            }
            pAdapter->Release();
            adapterIdx++;
        }
    }
    if (pSelectedAdapter == NULL) {
        SAFE_RELEASE(pFactory);
        return false;
    }

    // Create DirectX11 device
    D3D_FEATURE_LEVEL level;
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif // _DEBUG
    result = D3D11CreateDevice(
        pSelectedAdapter,
        D3D_DRIVER_TYPE_UNKNOWN,
        NULL,
        flags,
        levels,
        1,
        D3D11_SDK_VERSION,
        &pDevice_,
        &level,
        &pDeviceContext_
    );
    if (D3D_FEATURE_LEVEL_11_0 != level || !SUCCEEDED(result)) {
        SAFE_RELEASE(pFactory);
        SAFE_RELEASE(pSelectedAdapter);
        Cleanup();
        return false;
    }

    // Create swap chain
    DXGI_SWAP_CHAIN_DESC swapChainDesc = { 0 };
    swapChainDesc.BufferCount = 2;
    swapChainDesc.BufferDesc.Width = defaultWidth;
    swapChainDesc.BufferDesc.Height = defaultHeight;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = hWnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = true;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags = 0;
    result = pFactory->CreateSwapChain(pDevice_, &swapChainDesc, &pSwapChain_);

    ID3D11Texture2D* pBackBuffer = NULL;
    if (SUCCEEDED(result)) {
        result = pSwapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    }
    if (SUCCEEDED(result)) {
        result = pDevice_->CreateRenderTargetView(pBackBuffer, NULL, &pRenderTargetView_);
    }
    if (SUCCEEDED(result)) {
        result = InitScene();
    }
    SAFE_RELEASE(pFactory);
    SAFE_RELEASE(pSelectedAdapter);
    SAFE_RELEASE(pBackBuffer);
    if (SUCCEEDED(result)) {
        pCamera_ = new Camera;
        if (!pCamera_) {
            result = S_FALSE;
        }
    }
    if (SUCCEEDED(result)) {
        pInput_ = new Input;
        if (!pInput_) {
            result = S_FALSE;
        }
    }
    if (SUCCEEDED(result)) {
        result = pInput_->Init(hInstance, hWnd);
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX11_Init(pDevice_, pDeviceContext_);

#ifdef _DEBUG 
    if (SUCCEEDED(result)) {
        result = pDeviceContext_->QueryInterface(__uuidof(pAnnotation_), reinterpret_cast<void**>(&pAnnotation_));
    }
#endif // _DEBUG

    if (FAILED(result)) {
        Cleanup();
    }

    return SUCCEEDED(result);
}

HRESULT Renderer::InitScene() {
    HRESULT result;

    static const Vertex Vertices[] = {
        {{-1.0, -1.0,  1.0}, {0,1}, {0,-1,0}},
        {{ 1.0, -1.0,  1.0}, {1,1}, {0,-1,0}},
        {{ 1.0, -1.0, -1.0}, {1,0}, {0,-1,0}},
        {{-1.0, -1.0, -1.0}, {0,0}, {0,-1,0}},

        {{-1.0,  1.0, -1.0}, {0,1}, {0,1,0}},
        {{ 1.0,  1.0, -1.0}, {1,1}, {0,1,0}},
        {{ 1.0,  1.0,  1.0}, {1,0}, {0,1,0}},
        {{-1.0,  1.0,  1.0}, {0,0}, {0,1,0}},

        {{ 1.0, -1.0, -1.0}, {0,1}, {1,0,0}},
        {{ 1.0, -1.0,  1.0}, {1,1}, {1,0,0}},
        {{ 1.0,  1.0,  1.0}, {1,0}, {1,0,0}},
        {{ 1.0,  1.0, -1.0}, {0,0}, {1,0,0}},

        {{-1.0, -1.0,  1.0}, {0,1}, {-1,0,0}},
        {{-1.0, -1.0, -1.0}, {1,1}, {-1,0,0}},
        {{-1.0,  1.0, -1.0}, {1,0}, {-1,0,0}},
        {{-1.0,  1.0,  1.0}, {0,0}, {-1,0,0}},

        {{ 1.0, -1.0,  1.0}, {0,1}, {0,0,1}},
        {{-1.0, -1.0,  1.0}, {1,1}, {0,0,1}},
        {{-1.0,  1.0,  1.0}, {1,0}, {0,0,1}},
        {{ 1.0,  1.0,  1.0}, {0,0}, {0,0,1}},

        {{-1.0, -1.0, -1.0}, {0,1}, {0,0,-1}},
        {{ 1.0, -1.0, -1.0}, {1,1}, {0,0,-1}},
        {{ 1.0,  1.0, -1.0}, {1,0}, {0,0,-1}},
        {{-1.0,  1.0, -1.0}, {0,0}, {0,0,-1}}
    };
    static const USHORT Indices[] = {
        0, 2, 1, 0, 3, 2,
        4, 6, 5, 4, 7, 6,
        8, 10, 9, 8, 11, 10,
        12, 14, 13, 12, 15, 14,
        16, 18, 17, 16, 19, 18,
        20, 22, 21, 20, 23, 22
    };
    static const D3D11_INPUT_ELEMENT_DESC InputDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(Vertices);
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = &Vertices;
    data.SysMemPitch = sizeof(Vertices);
    data.SysMemSlicePitch = 0;

    result = pDevice_->CreateBuffer(&desc, &data, &pVertexBuffer_);

    if (SUCCEEDED(result)) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(Indices);
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = &Indices;
        data.SysMemPitch = sizeof(Indices);
        data.SysMemSlicePitch = 0;

        result = pDevice_->CreateBuffer(&desc, &data, &pIndexBuffer_);
    }

    ID3D10Blob* vertexShaderBuffer = nullptr;
    ID3D10Blob* pixelShaderBuffer = nullptr;
    int flags = 0;
#ifdef _DEBUG // Включение/отключение отладочной информации для шейдеров в зависимости от конфигурации сборки
    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    D3DInclude includeObj;

    if (SUCCEEDED(result)) {
        result = D3DCompileFromFile(L"VS.hlsl", NULL, &includeObj, "main", "vs_5_0", flags, 0, &vertexShaderBuffer, NULL);
        if (SUCCEEDED(result)) {
            result = pDevice_->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), NULL, &pVertexShader_);
        }
    }
    if (SUCCEEDED(result)) {
        result = D3DCompileFromFile(L"PS.hlsl", NULL, &includeObj, "main", "ps_5_0", flags, 0, &pixelShaderBuffer, NULL);
        if (SUCCEEDED(result)) {
            result = pDevice_->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), NULL, &pPixelShader_);
        }
    }
    if (SUCCEEDED(result)) {
        int numElements = sizeof(InputDesc) / sizeof(InputDesc[0]);
        result = pDevice_->CreateInputLayout(InputDesc, numElements, vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &pInputLayout_);
    }

    SAFE_RELEASE(vertexShaderBuffer);
    SAFE_RELEASE(pixelShaderBuffer);

    if (SUCCEEDED(result)) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(WorldMatrixBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        WorldMatrixBuffer worldMatrixBuffer;
        worldMatrixBuffer.worldMatrix = DirectX::XMMatrixIdentity();
        worldMatrixBuffer.shine = XMFLOAT4(300.0f, 0.0f, 0.0f, 0.0f);

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = &worldMatrixBuffer;
        data.SysMemPitch = sizeof(worldMatrixBuffer);
        data.SysMemSlicePitch = 0;

        result = pDevice_->CreateBuffer(&desc, &data, &pWorldMatrixBuffer_);
    }
    if (SUCCEEDED(result)) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(ViewMatrixBuffer);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        result = pDevice_->CreateBuffer(&desc, nullptr, &pViewMatrixBuffer_);
    }
    if (SUCCEEDED(result)) {
        D3D11_RASTERIZER_DESC desc = {};
        desc.AntialiasedLineEnable = false;
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_BACK;
        desc.DepthBias = 0;
        desc.DepthBiasClamp = 0.0f;
        desc.FrontCounterClockwise = false;
        desc.DepthClipEnable = true;
        desc.ScissorEnable = false;
        desc.MultisampleEnable = false;
        desc.SlopeScaledDepthBias = 0.0f;

        result = pDevice_->CreateRasterizerState(&desc, &pRasterizerState_);
    }
    if (SUCCEEDED(result)) {
        result = CreateDDSTextureFromFile(pDevice_, pDeviceContext_, L"textures/156.dds", &pTexture_, &pTextureView_);
    }
#ifdef _DEBUG // Маркер ресурса для отладочной сборки
    if (SUCCEEDED(result)) {
        std::string textureName = "156TextureImages";
        result = pTexture_->SetPrivateData(WKPDID_D3DDebugObjectName, textureName.size(), textureName.c_str());
    }
#endif
    if (SUCCEEDED(result)) {
      D3D11_SAMPLER_DESC desc = {};

      desc.Filter = D3D11_FILTER_ANISOTROPIC;
      desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
      desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
      desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
      desc.MinLOD = -D3D11_FLOAT32_MAX;
      desc.MaxLOD = D3D11_FLOAT32_MAX;
      desc.MipLODBias = 0.0f;
      desc.MaxAnisotropy = 16;
      desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
      desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 1.0f;

      result = pDevice_->CreateSamplerState(&desc, &pSampler_);
    }

    return result;
}

void Renderer::InputHandler() {
    XMFLOAT3 mouse = pInput_->ReadMouse();
    pCamera_->Rotate(mouse.x / 200.0f, mouse.y / 200.0f);
    pCamera_->Zoom(-mouse.z / 100.0f);

    XMINT3 keyboard = pInput_->ReadKeyboard();
    float dx = pCamera_->GetDistance() * keyboard.x / 30.0f,
        dy = pCamera_->GetDistance() * keyboard.y / 30.0f,
        dz = pCamera_->GetDistance() * keyboard.z / 30.0f;
    pCamera_->Move(dx, dy, dz);
}

bool Renderer::UpdateScene() {
    HRESULT result;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    static bool window = true;

    if (window) {
        ImGui::Begin("ImGui", &window);

        if (ImGui::Button("+")) {
            if (lights_.size() < MAX_LIGHT)
                lights_.push_back({ XMFLOAT4(2.0f, 2.0f, 0.0f, 0.f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) });
        }
        ImGui::SameLine();
        if (ImGui::Button("-")) {
            if (lights_.size() > 0)
                lights_.pop_back();
        }

        static float col[MAX_LIGHT][3];
        static float pos[MAX_LIGHT][4];
        static float brightness[MAX_LIGHT];
        for (int i = 0; i < lights_.size(); i++) {
            std::string str = "Light " + std::to_string(i);
            ImGui::Text(str.c_str());

            pos[i][0] = lights_[i].pos.x;
            pos[i][1] = lights_[i].pos.y;
            pos[i][2] = lights_[i].pos.z;
            str = "Pos " + std::to_string(i);
            ImGui::DragFloat3(str.c_str(), pos[i], 0.1f, -4.0f, 4.0f);
            lights_[i].pos = XMFLOAT4(pos[i][0], pos[i][1], pos[i][2], 1.0f);

            col[i][0] = lights_[i].color.x;
            col[i][1] = lights_[i].color.y;
            col[i][2] = lights_[i].color.z;
            str = "Color " + std::to_string(i);
            ImGui::ColorEdit3(str.c_str(), col[i]);
            lights_[i].color = XMFLOAT4(col[i][0], col[i][1], col[i][2], lights_[i].color.w);

            brightness[i] = lights_[i].color.w;
            str = "Brightness " + std::to_string(i);
            ImGui::DragFloat(str.c_str(), &brightness[i], 1.0f, 1.0f, 100.0f);
            lights_[i].color.w = brightness[i];
        }

        ImGui::End();
    }

    InputHandler();

    static float t = 0.0f;
    static ULONGLONG timeStart = 0;
    ULONGLONG timeCur = GetTickCount64();
    if (timeStart == 0) {
        timeStart = timeCur;
    }
    t = (timeCur - timeStart) / 1000.0f;

    WorldMatrixBuffer worldMatrixBuffer;
    worldMatrixBuffer.worldMatrix = XMMatrixRotationY(t);
    worldMatrixBuffer.shine = XMFLOAT4(300.0f, 0.0f, 0.0f, 0.0f);

    pDeviceContext_->UpdateSubresource(pWorldMatrixBuffer_, 0, nullptr, &worldMatrixBuffer, 0, 0);

    XMMATRIX mView = pCamera_->GetViewMatrix();

    XMMATRIX mProjection = XMMatrixPerspectiveFovLH(XM_PI / 3, width_ / (FLOAT)height_, 100.0f, 0.01f);

    XMFLOAT3 cameraPos = pCamera_->GetPosition();
    D3D11_MAPPED_SUBRESOURCE subresource;
    result = pDeviceContext_->Map(pViewMatrixBuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &subresource);
    if (SUCCEEDED(result)) {
        ViewMatrixBuffer& sceneBuffer = *reinterpret_cast<ViewMatrixBuffer*>(subresource.pData);
        sceneBuffer.viewProjectionMatrix = XMMatrixMultiply(mView, mProjection);
        sceneBuffer.cameraPos = XMFLOAT4(cameraPos.x, cameraPos.y, cameraPos.z, 1.0f);
        sceneBuffer.ambientColor = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
        sceneBuffer.lightParams = XMINT4(int(lights_.size()), 0, 0, 0);
        for (int i = 0; i < lights_.size(); i++) {
            sceneBuffer.lights[i].pos = lights_[i].pos;
            sceneBuffer.lights[i].color = lights_[i].color;
        }
        pDeviceContext_->Unmap(pViewMatrixBuffer_, 0);
    }

    ImGui::Render();

    return SUCCEEDED(result);
}

bool Renderer::Render() {
#ifdef _DEBUG
    pAnnotation_->BeginEvent(L"Render_scene");
    pAnnotation_->BeginEvent(L"Preliminary_preparations");
#endif
    if (!UpdateScene())
        return false;

    pDeviceContext_->ClearState();

    ID3D11RenderTargetView* views[] = { pRenderTargetView_ };
    pDeviceContext_->OMSetRenderTargets(1, views, NULL);

    static const FLOAT backColor[4] = { 0.4f, 0.2f, 0.4f, 1.0f };
    pDeviceContext_->ClearRenderTargetView(pRenderTargetView_, backColor);

#ifdef _DEBUG
    pAnnotation_->EndEvent();
    pAnnotation_->BeginEvent(L"Draw_cube");
    pAnnotation_->BeginEvent(L"Setting_everything_necessary");
#endif

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = (FLOAT)width_;
    viewport.Height = (FLOAT)height_;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    pDeviceContext_->RSSetViewports(1, &viewport);

    D3D11_RECT rect;
    rect.left = 0;
    rect.top = 0;
    rect.right = width_;
    rect.bottom = height_;
    pDeviceContext_->RSSetScissorRects(1, &rect);

    ID3D11SamplerState* samplers[] = { pSampler_ };
    pDeviceContext_->PSSetSamplers(0, 1, samplers);

    ID3D11ShaderResourceView* resources[] = { pTextureView_ };
    pDeviceContext_->PSSetShaderResources(0, 1, resources);

    pDeviceContext_->IASetIndexBuffer(pIndexBuffer_, DXGI_FORMAT_R16_UINT, 0);
    ID3D11Buffer* vertexBuffers[] = { pVertexBuffer_ };
    UINT strides[] = { sizeof(Vertex) };
    UINT offsets[] = { 0 };
    pDeviceContext_->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    pDeviceContext_->IASetInputLayout(pInputLayout_);
    pDeviceContext_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pDeviceContext_->VSSetConstantBuffers(0, 1, &pWorldMatrixBuffer_);
    pDeviceContext_->VSSetConstantBuffers(1, 1, &pViewMatrixBuffer_);
    pDeviceContext_->PSSetConstantBuffers(0, 1, &pWorldMatrixBuffer_);
    pDeviceContext_->PSSetConstantBuffers(1, 1, &pViewMatrixBuffer_);
    pDeviceContext_->VSSetShader(pVertexShader_, nullptr, 0);
    pDeviceContext_->PSSetShader(pPixelShader_, nullptr, 0);
#ifdef _DEBUG
    pAnnotation_->EndEvent();
    pAnnotation_->BeginEvent(L"Actual_cube_drawing");
#endif
    pDeviceContext_->DrawIndexed(36, 0, 0);
#ifdef _DEBUG
    pAnnotation_->EndEvent();
    pAnnotation_->EndEvent();
#endif

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    HRESULT result = pSwapChain_->Present(0, 0);
#ifdef _DEBUG
    pAnnotation_->EndEvent();
#endif
    return SUCCEEDED(result);
}

bool Renderer::Resize(UINT width, UINT height) {
    if (pSwapChain_ == NULL)
        return false;

    SAFE_RELEASE(pRenderTargetView_);

    width_ = max(width, 8);
    height_ = max(height, 8);

    auto result = pSwapChain_->ResizeBuffers(2, width_, height_, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    if (!SUCCEEDED(result))
        return false;

    ID3D11Texture2D* pBuffer;
    result = pSwapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*) &pBuffer);
    if (!SUCCEEDED(result))
        return false;

    result = pDevice_->CreateRenderTargetView(pBuffer, NULL, &pRenderTargetView_);
    SAFE_RELEASE(pBuffer);
    if (!SUCCEEDED(result))
        return false;

    return true;
}

void Renderer::Cleanup() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (pDeviceContext_ != NULL)
        pDeviceContext_->ClearState();

    SAFE_RELEASE(pRenderTargetView_);
    SAFE_RELEASE(pDeviceContext_);
    SAFE_RELEASE(pSwapChain_);
    SAFE_RELEASE(pSampler_);

    SAFE_RELEASE(pVertexBuffer_);
    SAFE_RELEASE(pIndexBuffer_);
    SAFE_RELEASE(pInputLayout_);
    SAFE_RELEASE(pVertexShader_);
    SAFE_RELEASE(pPixelShader_);

    SAFE_RELEASE(pRasterizerState_);
    SAFE_RELEASE(pViewMatrixBuffer_);
    SAFE_RELEASE(pWorldMatrixBuffer_);
    SAFE_RELEASE(pTexture_);
    SAFE_RELEASE(pTextureView_);
    
    if (pCamera_) {
        delete pCamera_;
        pCamera_ = NULL;
    }
    if (pInput_) {
        delete pInput_;
        pCamera_ = NULL;
    }

#ifdef _DEBUG // Получение информации о "подвисших" объектах в отладочной сборке
    SAFE_RELEASE(pAnnotation_);
    if (pDevice_ != NULL) {
        ID3D11Debug* d3dDebug = NULL;
        pDevice_->QueryInterface(IID_PPV_ARGS(&d3dDebug));

        UINT references = pDevice_->Release();
        pDevice_ = NULL;
        if (references > 1) {
            d3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
        }
        SAFE_RELEASE(d3dDebug);
    }
#endif
    SAFE_RELEASE(pDevice_);
}

Renderer::~Renderer() {
    Cleanup();
}