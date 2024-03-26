#include "Renderer.h"
#include <string>

const D3D11_INPUT_ELEMENT_DESC Renderer::SimpleVertexDesc[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
};
const D3D11_INPUT_ELEMENT_DESC Renderer::VertexDesc[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

Renderer& Renderer::GetInstance() {
    static Renderer instance;
    return instance;
}

Renderer::Renderer() :
    pGeometryManager_(nullptr),
    pILManager_(nullptr),
    pPSManager_(nullptr),
    pVSManager_(nullptr),
    pSamplerManager_(nullptr),
    pTextureManager_(nullptr, nullptr),
    width_(defaultWidth),
    height_(defaultHeight) {}

bool Renderer::Init(HINSTANCE hInstance, HWND hWnd) {
    HRESULT result = CreateDevice();

    if (SUCCEEDED(result)) {
        result = CreateSwapChain(hWnd);
    }
    if (SUCCEEDED(result)) {
        result = CreateSamplers();
    }
    if (SUCCEEDED(result)) {
        result = pSamplerManager_.get("default", pSampler_);
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
        result = CreateWMBuffer();
    }
    if (SUCCEEDED(result)) {
        result = CreateSWMBuffer();
    }
    if (SUCCEEDED(result)) {
        result = CreateVMBuffer();
    }
    if (SUCCEEDED(result)) {
        result = LoadShaders();
    }
    if (SUCCEEDED(result)) {
        result = LoadGeometry();
    }
    if (SUCCEEDED(result)) {
        result = LoadTextures();
    }
    if (SUCCEEDED(result)) {
        result = InitSkybox();
    }
    if (SUCCEEDED(result)) {
        result = InitObjects();
    }
    if (SUCCEEDED(result)) {
        result = toneMapping_.Init(pDevice_, pDeviceContext_, pVSManager_, pPSManager_, pSamplerManager_, width_, height_);
    }
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
    if (SUCCEEDED(result)) {
        result = InitImgui(hWnd);
    }

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

HRESULT Renderer::CreateDevice() {
    HRESULT result = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory_);
    if (SUCCEEDED(result)) {
        IDXGIAdapter* pAdapter = nullptr;
        UINT adapterIdx = 0;
        while (SUCCEEDED(pFactory_->EnumAdapters(adapterIdx, &pAdapter))) {
            DXGI_ADAPTER_DESC desc;
            result = pAdapter->GetDesc(&desc);
            if (SUCCEEDED(result) && wcscmp(desc.Description, L"Microsoft Basic Render Driver")) {
                pSelectedAdapter_ = pAdapter;
                break;
            }
            pAdapter->Release();
            adapterIdx++;
        }
    }
    if (pSelectedAdapter_ == nullptr) {
        return E_FAIL;
    }

    D3D_FEATURE_LEVEL level;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* deviceContext = nullptr;
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif // _DEBUG
    result = D3D11CreateDevice(
        pSelectedAdapter_,
        D3D_DRIVER_TYPE_UNKNOWN,
        NULL,
        flags,
        levels,
        1,
        D3D11_SDK_VERSION,
        &device,
        &level,
        &deviceContext
    );
    if (D3D_FEATURE_LEVEL_11_0 != level || !SUCCEEDED(result)) {
        SAFE_RELEASE(device);
        SAFE_RELEASE(deviceContext);
        return E_FAIL;
    }
    pDevice_ = std::shared_ptr<ID3D11Device>(device, utilities::DXRelPtrDeleter<ID3D11Device*>);
    pDeviceContext_ = std::shared_ptr<ID3D11DeviceContext>(deviceContext, utilities::DXPtrDeleter<ID3D11DeviceContext*>);

    return S_OK;
}

HRESULT Renderer::CreateSwapChain(HWND hWnd) {
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
    HRESULT result = pFactory_->CreateSwapChain(pDevice_.get(), &swapChainDesc, &pSwapChain_);

    ID3D11Texture2D* pBackBuffer = NULL;
    if (SUCCEEDED(result)) {
        result = pSwapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    }
    if (SUCCEEDED(result)) {
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // Формат RTV совпадает с форматом текстуры
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;

        result = pDevice_->CreateRenderTargetView(pBackBuffer, &rtvDesc, &pRenderTargetView_);
    }
    SAFE_RELEASE(pBackBuffer);
    return result;
}

HRESULT Renderer::CreateSamplers() {
    pSamplerManager_.setDevice(pDevice_);

    HRESULT result = pSamplerManager_.loadSampler(D3D11_FILTER_ANISOTROPIC, "default", D3D11_TEXTURE_ADDRESS_WRAP);
    if (SUCCEEDED(result)) {
        result = pSamplerManager_.loadSampler(D3D11_FILTER_MINIMUM_ANISOTROPIC, "min", D3D11_TEXTURE_ADDRESS_WRAP);
    }
    if (SUCCEEDED(result)) {
        result = pSamplerManager_.loadSampler(D3D11_FILTER_MAXIMUM_ANISOTROPIC, "max", D3D11_TEXTURE_ADDRESS_WRAP);
    }
    if (SUCCEEDED(result)) {
        result = pSamplerManager_.loadSampler(D3D11_FILTER_MIN_MAG_MIP_LINEAR, "avg", D3D11_TEXTURE_ADDRESS_CLAMP);
    }
    return result;
}

HRESULT Renderer::CreateWMBuffer() {
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(WorldMatrixBuffer);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    WorldMatrixBuffer worldMatrixBuffer;
    worldMatrixBuffer.worldMatrix = DirectX::XMMatrixIdentity();
    worldMatrixBuffer.color = XMFLOAT3(0.0f, 0.0f, 0.0f);
    worldMatrixBuffer.metalness = 0.0f;
    worldMatrixBuffer.roughness = 0.0f;

    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = &worldMatrixBuffer;
    data.SysMemPitch = sizeof(worldMatrixBuffer);
    data.SysMemSlicePitch = 0;

    return pDevice_->CreateBuffer(&desc, &data, &pWorldMatrixBuffer_);
}

HRESULT Renderer::CreateSWMBuffer() {
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(SkyboxWorldMatrixBuffer);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    SkyboxWorldMatrixBuffer worldMatrixBuffer;
    worldMatrixBuffer.worldMatrix = DirectX::XMMatrixIdentity();
    worldMatrixBuffer.size = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = &worldMatrixBuffer;
    data.SysMemPitch = sizeof(worldMatrixBuffer);
    data.SysMemSlicePitch = 0;

    return pDevice_->CreateBuffer(&desc, &data, &pSkyboxWorldMatrixBuffer_);
}

HRESULT Renderer::CreateVMBuffer() {
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(ViewMatrixBuffer);
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    return pDevice_->CreateBuffer(&desc, nullptr, &pViewMatrixBuffer_);
}

HRESULT Renderer::LoadShaders() {
    pVSManager_.setDevice(pDevice_);
    pILManager_.setDevice(pDevice_);
    pPSManager_.setDevice(pDevice_);
    PSManager psManager(pDevice_, L"shaders");
    VSManager vsManager(pDevice_, L"shaders");

    HRESULT result = pVSManager_.loadVS(L"shaders/VS.hlsl", nullptr, "sphere",
        &pILManager_, VertexDesc, sizeof(VertexDesc) / sizeof(VertexDesc[0]));
    result = vsManager.LoadShader(L"VS.hlsl", {});
    if (SUCCEEDED(result)) {
        D3D_SHADER_MACRO shaderMacros[] = { {"DEFAULT"}, {NULL, NULL} };
        result = pPSManager_.loadPS(L"shaders/PS.hlsl", shaderMacros, "default");
        result = psManager.LoadShader(L"PS.hlsl", { "DEFAULT" });
    }
    if (SUCCEEDED(result)) {
        D3D_SHADER_MACRO shaderMacros[] = { {"FRESNEL"}, {NULL, NULL} };
        result = pPSManager_.loadPS(L"shaders/PS.hlsl", shaderMacros, "fresnel");
        result = psManager.LoadShader(L"PS.hlsl", { "FRESNEL" });
    }
    if (SUCCEEDED(result)) {
        D3D_SHADER_MACRO shaderMacros[] = { {"ND"}, {NULL, NULL} };
        result = pPSManager_.loadPS(L"shaders/PS.hlsl", shaderMacros, "ndf");
        result = psManager.LoadShader(L"PS.hlsl", { "ND" });
    }
    if (SUCCEEDED(result)) {
        D3D_SHADER_MACRO shaderMacros[] = { {"GEOMETRY"}, {NULL, NULL} };
        result = pPSManager_.loadPS(L"shaders/PS.hlsl", shaderMacros, "geometry");
        result = psManager.LoadShader(L"PS.hlsl", { "GEOMETRY" });
    }
    if (SUCCEEDED(result)) {
        result = pVSManager_.loadVS(L"shaders/CubeMapVS.hlsl", nullptr, "skybox",
            &pILManager_, SimpleVertexDesc, sizeof(SimpleVertexDesc) / sizeof(SimpleVertexDesc[0]));
    }
    if (SUCCEEDED(result)) {
        result = pPSManager_.loadPS(L"shaders/CubeMapPS.hlsl", nullptr, "skybox");
    }
    if (SUCCEEDED(result)) {
        result = pVSManager_.loadVS(L"shaders/mappingVS.hlsl", nullptr, "mapping");
    }
    if (SUCCEEDED(result)) {
        result = pPSManager_.loadPS(L"shaders/brightnessPS.hlsl", nullptr, "brightness");
    }
    if (SUCCEEDED(result)) {
        result = pPSManager_.loadPS(L"shaders/downsamplePS.hlsl", nullptr, "downsample");
    }
    if (SUCCEEDED(result)) {
        result = pPSManager_.loadPS(L"shaders/toneMapPS.hlsl", nullptr, "tonemap");
    }
    if (SUCCEEDED(result)) {
        result = pPSManager_.loadPS(L"shaders/cubemapGeneratorPS.hlsl", nullptr, "cubemapGenerator");
    }
    if (SUCCEEDED(result)) {
        result = pPSManager_.loadPS(L"shaders/cubemapGeneratorIrradiancePS.hlsl", nullptr, "cubemapGeneratorIrradiance");
    }
    if (SUCCEEDED(result)) {
        result = pVSManager_.loadVS(L"shaders/cubemapGeneratorVS.hlsl", nullptr, "cubemapGenerator",
            &pILManager_, SimpleVertexDesc, sizeof(SimpleVertexDesc) / sizeof(SimpleVertexDesc[0]));
    }
    if (SUCCEEDED(result)) {
        result = pPSManager_.loadPS(L"shaders/copyToCubemapPS.hlsl", nullptr, "copyToCubemapPS");
    }
    if (SUCCEEDED(result)) {
        result = pPSManager_.loadPS(L"shaders/prefilteredColorPS.hlsl", nullptr, "prefilteredColorPS");
    }
    if (SUCCEEDED(result)) {
        result = pVSManager_.loadVS(L"shaders/brdfVS.hlsl", nullptr, "brdf");
    }
    if (SUCCEEDED(result)) {
        result = pPSManager_.loadPS(L"shaders/brdfPS.hlsl", nullptr, "brdf");
    }
    return result;
}

HRESULT Renderer::LoadGeometry() {
    pGeometryManager_.setDevice(pDevice_);

    UINT LatLines = 40, LongLines = 40;
    UINT numVertices = ((LatLines - 2) * LongLines) + 2;
    UINT numIndices = (((LatLines - 3) * (LongLines) * 2) + (LongLines * 2)) * 3;

    float phi = 0.0f;
    float theta = 0.0f;

    //========================================================================================================================================================================
    //std::vector<SimpleVertex> skyboxVertices(numVertices);

    std::vector<Vertex> sphereVertices(numVertices);

    XMVECTOR currVertPos = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

    //skyboxVertices[0].pos.x = 0.0f;
    //skyboxVertices[0].pos.y = 0.0f;
    //skyboxVertices[0].pos.z = 1.0f;

    sphereVertices[0].pos.x = 0.0f;
    sphereVertices[0].pos.y = 0.0f;
    sphereVertices[0].pos.z = 1.0f;
    sphereVertices[0].norm.x = 0.0f;
    sphereVertices[0].norm.y = 0.0f;
    sphereVertices[0].norm.z = 1.0f;

    for (UINT i = 0; i < LatLines - 2; i++) {
        theta = (i + 1) * (XM_PI / (LatLines - 1));
        XMMATRIX Rotationx = XMMatrixRotationX(theta);
        for (UINT j = 0; j < LongLines; j++) {
            phi = j * (XM_2PI / LongLines);
            XMMATRIX Rotationy = XMMatrixRotationZ(phi);
            currVertPos = XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), (Rotationx * Rotationy));
            currVertPos = XMVector3Normalize(currVertPos);
            //skyboxVertices[i * (__int64)LongLines + j + 1].pos.x = XMVectorGetX(currVertPos);
            //skyboxVertices[i * (__int64)LongLines + j + 1].pos.y = XMVectorGetY(currVertPos);
            //skyboxVertices[i * (__int64)LongLines + j + 1].pos.z = XMVectorGetZ(currVertPos);

            sphereVertices[i * (__int64)LongLines + j + 1].pos.x = XMVectorGetX(currVertPos);
            sphereVertices[i * (__int64)LongLines + j + 1].pos.y = XMVectorGetY(currVertPos);
            sphereVertices[i * (__int64)LongLines + j + 1].pos.z = XMVectorGetZ(currVertPos);
            sphereVertices[i * (__int64)LongLines + j + 1].norm.x = XMVectorGetX(currVertPos);
            sphereVertices[i * (__int64)LongLines + j + 1].norm.y = XMVectorGetY(currVertPos);
            sphereVertices[i * (__int64)LongLines + j + 1].norm.z = XMVectorGetZ(currVertPos);
        }
    }

    //skyboxVertices[(__int64)numVertices - 1].pos.x = 0.0f;
    //skyboxVertices[(__int64)numVertices - 1].pos.y = 0.0f;
    //skyboxVertices[(__int64)numVertices - 1].pos.z = -1.0f;

    sphereVertices[(__int64)numVertices - 1].pos.x = 0.0f;
    sphereVertices[(__int64)numVertices - 1].pos.y = 0.0f;
    sphereVertices[(__int64)numVertices - 1].pos.z = -1.0f;
    sphereVertices[(__int64)numVertices - 1].norm.x = 0.0f;
    sphereVertices[(__int64)numVertices - 1].norm.y = 0.0f;
    sphereVertices[(__int64)numVertices - 1].norm.z = -1.0f;

    //std::vector<UINT> skyboxIndices(numIndices);
    std::vector<UINT> sphereIndices(numIndices);

    UINT k = 0;
    for (UINT i = 0; i < LongLines - 1; i++) {
        //skyboxIndices[k] = 0;
        //skyboxIndices[(__int64)k + 2] = i + 1;
        //skyboxIndices[(__int64)k + 1] = i + 2;

        sphereIndices[k] = i + 1;
        sphereIndices[(__int64)k + 2] = 0;
        sphereIndices[(__int64)k + 1] = i + 2;
        k += 3;
    }
    //skyboxIndices[k] = 0;
    //skyboxIndices[(__int64)k + 2] = LongLines;
    //skyboxIndices[(__int64)k + 1] = 1;

    sphereIndices[k] = LongLines;
    sphereIndices[(__int64)k + 2] = 0;
    sphereIndices[(__int64)k + 1] = 1;
    k += 3;

    for (UINT i = 0; i < LatLines - 3; i++) {
        for (UINT j = 0; j < LongLines - 1; j++) {
            //skyboxIndices[k] = i * LongLines + j + 1;
            //skyboxIndices[(__int64)k + 1] = i * LongLines + j + 2;
            //skyboxIndices[(__int64)k + 2] = (i + 1) * LongLines + j + 1;

            //skyboxIndices[(__int64)k + 3] = (i + 1) * LongLines + j + 1;
            //skyboxIndices[(__int64)k + 4] = i * LongLines + j + 2;
            //skyboxIndices[(__int64)k + 5] = (i + 1) * LongLines + j + 2;

            sphereIndices[(__int64)k + 2] = i * LongLines + j + 1;
            sphereIndices[(__int64)k + 1] = i * LongLines + j + 2;
            sphereIndices[k] = (i + 1) * LongLines + j + 1;

            sphereIndices[(__int64)k + 5] = (i + 1) * LongLines + j + 1;
            sphereIndices[(__int64)k + 4] = i * LongLines + j + 2;
            sphereIndices[(__int64)k + 3] = (i + 1) * LongLines + j + 2;

            k += 6;
        }

        //skyboxIndices[k] = (i * LongLines) + LongLines;
        //skyboxIndices[(__int64)k + 1] = (i * LongLines) + 1;
        //skyboxIndices[(__int64)k + 2] = ((i + 1) * LongLines) + LongLines;

        //skyboxIndices[(__int64)k + 3] = ((i + 1) * LongLines) + LongLines;
        //skyboxIndices[(__int64)k + 4] = (i * LongLines) + 1;
        //skyboxIndices[(__int64)k + 5] = ((i + 1) * LongLines) + 1;

        sphereIndices[(__int64)k + 2] = (i * LongLines) + LongLines;
        sphereIndices[(__int64)k + 1] = (i * LongLines) + 1;
        sphereIndices[k] = ((i + 1) * LongLines) + LongLines;

        sphereIndices[(__int64)k + 5] = ((i + 1) * LongLines) + LongLines;
        sphereIndices[(__int64)k + 4] = (i * LongLines) + 1;
        sphereIndices[(__int64)k + 3] = ((i + 1) * LongLines) + 1;

        k += 6;
    }

    for (UINT i = 0; i < LongLines - 1; i++) {
        //skyboxIndices[k] = numVertices - 1;
        //skyboxIndices[(__int64)k + 2] = (numVertices - 1) - (i + 1);
        //skyboxIndices[(__int64)k + 1] = (numVertices - 1) - (i + 2);

        sphereIndices[(__int64)k + 2] = numVertices - 1;
        sphereIndices[k] = (numVertices - 1) - (i + 1);
        sphereIndices[(__int64)k + 1] = (numVertices - 1) - (i + 2);
        k += 3;
    }

    //skyboxIndices[k] = numVertices - 1;
    //skyboxIndices[(__int64)k + 2] = (numVertices - 1) - LongLines;
    //skyboxIndices[(__int64)k + 1] = numVertices - 2;

    sphereIndices[(__int64)k + 2] = numVertices - 1;
    sphereIndices[k] = (numVertices - 1) - LongLines;
    sphereIndices[(__int64)k + 1] = numVertices - 2;

    /*HRESULT result = pGeometryManager_.loadGeometry(&skyboxVertices[0], sizeof(SimpleVertex) * numVertices,
        &skyboxIndices[0], sizeof(UINT) * numIndices, "skybox");
    if (SUCCEEDED(result)) {
        result = pGeometryManager_.loadGeometry(&sphereVertices[0], sizeof(Vertex) * numVertices,
            &sphereIndices[0], sizeof(UINT) * numIndices, "sphere");
    }*/
    HRESULT result = pGeometryManager_.loadGeometry(&sphereVertices[0], sizeof(Vertex) * numVertices,
            &sphereIndices[0], sizeof(UINT) * numIndices, "sphere");

    return result;
}

HRESULT Renderer::LoadTextures() {
    pTextureManager_.setDevice(pDevice_);
    pTextureManager_.setDeviceContext(pDeviceContext_);
    HRESULT result;
#ifndef _DEBUG
    result = pTextureManager_.loadHDRTexture("textures/hdr_text.hdr", "hdr");
#else  // Маркер ресурса для отладочной сборки
    result = pTextureManager_.loadHDRTexture("textures/hdr_text.hdr", "hdr", "HDRTextImages");
#endif

    if (SUCCEEDED(result)) {
        CubemapGenerator cubeMapGen(pDevice_, pDeviceContext_, pSamplerManager_, pTextureManager_, pILManager_, pPSManager_, pVSManager_,
            pGeometryManager_);
        result = cubeMapGen.init();
        if (SUCCEEDED(result)) {
            result = cubeMapGen.generateEnvironmentMap("evironment");
        }
        if (SUCCEEDED(result)) {
            result = cubeMapGen.generateIrradianceMap("evironment", "irradiance");
        }
        if (SUCCEEDED(result)) {
            result = cubeMapGen.generatePrefilteredMap("evironment", "prefiltered");
        }
        if (SUCCEEDED(result)) {
            result = cubeMapGen.generateBRDF("brdf");
        }
    }

    return result;
}

HRESULT Renderer::InitSkybox() {
    HRESULT result = S_OK;
    //skybox.worldMatrix = DirectX::XMMatrixIdentity();;
    //skybox.size = 1.0f;
    //HRESULT result = pGeometryManager_.get("skybox", skybox.geometry);
    if (SUCCEEDED(result)) {
        result = pVSManager_.get("skybox", skybox.VS);
    }
    if (SUCCEEDED(result)) {
        result = pILManager_.get("skybox", skybox.IL);
    }
    if (SUCCEEDED(result)) {
        result = pPSManager_.get("skybox", skybox.PS);
    }
    if (SUCCEEDED(result)) {
        result = pTextureManager_.get("evironment", skybox.texture);
    }
    return result;
}

HRESULT Renderer::InitObjects() {
    sphere.worldMatrix = DirectX::XMMatrixIdentity();;
    sphere.color = XMFLOAT3(1.0f, 0.71f, 0.29f);
    sphere.metalness = 1.0f;
    sphere.roughness = 0.01f;
    HRESULT result = pGeometryManager_.get("sphere", sphere.geometry);
    if (SUCCEEDED(result)) {
        result = pVSManager_.get("sphere", sphere.VS);
    }
    if (SUCCEEDED(result)) {
        result = pILManager_.get("sphere", sphere.IL);
    }
    if (SUCCEEDED(result)) {
        result = pPSManager_.get("default", sphere.PS);
    }
    if (SUCCEEDED(result)) {
        result = pTextureManager_.get("irradiance", sphere.irradianceMap);
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
        result = ImGui_ImplDX11_Init(pDevice_.get(), pDeviceContext_.get());
    }
    return result ? S_OK : E_FAIL;
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
    UpdateImgui();

    InputHandler();

    WorldMatrixBuffer worldMatrixBuffer;
    worldMatrixBuffer.worldMatrix = sphere.worldMatrix;
    worldMatrixBuffer.color = sphere.color;
    worldMatrixBuffer.roughness = sphere.roughness;
    worldMatrixBuffer.metalness = sphere.metalness;
    pDeviceContext_->UpdateSubresource(pWorldMatrixBuffer_, 0, nullptr, &worldMatrixBuffer, 0, 0);

    XMMATRIX mView = pCamera_->GetViewMatrix();
    XMMATRIX mProjection = XMMatrixPerspectiveFovLH(XM_PI / 3, width_ / (FLOAT)height_, 100.0f, 0.01f);
    XMFLOAT3 cameraPos = pCamera_->GetPosition();
    D3D11_MAPPED_SUBRESOURCE subresource;
    HRESULT result = pDeviceContext_->Map(pViewMatrixBuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &subresource);
    if (SUCCEEDED(result)) {
        ViewMatrixBuffer& sceneBuffer = *reinterpret_cast<ViewMatrixBuffer*>(subresource.pData);
        sceneBuffer.viewProjectionMatrix = XMMatrixMultiply(mView, mProjection);
        sceneBuffer.cameraPos = XMFLOAT4(cameraPos.x, cameraPos.y, cameraPos.z, 1.0f);
        sceneBuffer.lightParams = XMINT4(int(lights_.size()), 0, 0, 0);
        for (int i = 0; i < lights_.size(); i++) {
            sceneBuffer.lights[i].pos = lights_[i].pos;
            sceneBuffer.lights[i].color = lights_[i].color;
        }
        pDeviceContext_->Unmap(pViewMatrixBuffer_, 0);
    }
    if (SUCCEEDED(result)) {
        SkyboxWorldMatrixBuffer skyboxWorldMatrixBuffer;
        skyboxWorldMatrixBuffer.worldMatrix = skybox.worldMatrix;
        skyboxWorldMatrixBuffer.size = XMFLOAT4(skybox.size, 0.0f, 0.0f, 0.0f);
        pDeviceContext_->UpdateSubresource(pSkyboxWorldMatrixBuffer_, 0, nullptr, &skyboxWorldMatrixBuffer, 0, 0);
    }
    if (SUCCEEDED(result)) {
        ImGui::Render();
    }

    return SUCCEEDED(result);
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
        if (ImGui::Combo("Mode", &cur, "default\0fresnel\0ndf\0geometry")) {
            switch (cur) {
            case 0:
                default_ = true;
                pPSManager_.get("default", sphere.PS);
                break;
            case 1:
                default_ = false;
                pPSManager_.get("fresnel", sphere.PS);
                break;
            case 2:
                default_ = false;
                pPSManager_.get("ndf", sphere.PS);
                break;
            case 3:
                default_ = false;
                pPSManager_.get("geometry", sphere.PS);
                break;
            default:
                break;
            }
        }

        if (default_) {
            static float factor;
            factor = toneMapping_.GetFactor();
            str = "Exposure factor";
            ImGui::DragFloat(str.c_str(), &factor, 0.01f, 0.0f, 10.0f);
            toneMapping_.SetFactor(factor);
        }

        str = "Object";
        ImGui::Text(str.c_str());

        static float objCol[3];
        static float objRough;
        static float objMetal;

        objCol[0] = sphere.color.x;
        objCol[1] = sphere.color.y;
        objCol[2] = sphere.color.z;
        str = "Color";
        ImGui::ColorEdit3(str.c_str(), objCol);
        sphere.color = XMFLOAT3(objCol[0], objCol[1], objCol[2]);

        objRough = sphere.roughness;
        str = "Roughness";
        ImGui::DragFloat(str.c_str(), &objRough, 0.01f, 0.0f, 1.0f);
        sphere.roughness = objRough;

        objMetal = sphere.metalness;
        str = "Metalness";
        ImGui::DragFloat(str.c_str(), &objMetal, 0.01f, 0.0f, 1.0f);
        sphere.metalness = objMetal;

        str = "Lights";
        ImGui::Text(str.c_str());
        ImGui::SameLine();
        if (ImGui::Button("+")) {
            if (lights_.size() < MAX_LIGHT)
                lights_.push_back({ XMFLOAT4(5.0f, 5.0f, 5.0f, 0.f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) });
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
            ImGui::DragFloat3(str.c_str(), pos[i], 0.1f, -15.0f, 15.0f);
            lights_[i].pos = XMFLOAT4(pos[i][0], pos[i][1], pos[i][2], 1.0f);

            col[i][0] = lights_[i].color.x;
            col[i][1] = lights_[i].color.y;
            col[i][2] = lights_[i].color.z;
            str = "Color " + std::to_string(i);
            ImGui::ColorEdit3(str.c_str(), col[i]);
            lights_[i].color = XMFLOAT4(col[i][0], col[i][1], col[i][2], lights_[i].color.w);

            brightness[i] = lights_[i].color.w;
            str = "Brightness " + std::to_string(i);
            ImGui::DragFloat(str.c_str(), &brightness[i], 1.0f, 1.0f, 1000.0f);
            lights_[i].color.w = brightness[i];
        }

        ImGui::End();
    }
}

bool Renderer::Render() {
#ifdef _DEBUG
    pAnnotation_->BeginEvent(L"Render_scene");
    pAnnotation_->BeginEvent(L"Preliminary_preparations");
#endif

    if (!UpdateScene()) {
#ifdef _DEBUG
        pAnnotation_->EndEvent();
        pAnnotation_->EndEvent();
#endif
        return false;
    }

    pDeviceContext_->ClearState();

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

    ID3D11SamplerState* samplers[] = { pSampler_.get() };
    static const FLOAT backColor[4] = { 0.25f, 0.25f, 0.25f, 1.0f };

    if (default_) {
        toneMapping_.ClearRenderTarget();
        toneMapping_.SetRenderTarget();
    }
    else {
        pDeviceContext_->OMSetRenderTargets(1, &pRenderTargetView_, nullptr);
        pDeviceContext_->ClearRenderTargetView(pRenderTargetView_, backColor);
        pDeviceContext_->RSSetViewports(1, &viewport);
    }

#ifdef _DEBUG
    pAnnotation_->EndEvent();
    pAnnotation_->BeginEvent(L"Draw_skybox");
#endif

    pDeviceContext_->PSSetSamplers(0, 1, samplers);
    RenderSkybox();

#ifdef _DEBUG
    pAnnotation_->EndEvent();
    pAnnotation_->BeginEvent(L"Draw_sphere");
#endif

    pDeviceContext_->PSSetSamplers(0, 1, samplers);
    RenderObjects();

    if (default_) {
#ifdef _DEBUG
        pAnnotation_->EndEvent();
        pAnnotation_->BeginEvent(L"Tone_mapping");
#endif

        toneMapping_.RenderBrightness();

        pDeviceContext_->OMSetRenderTargets(1, &pRenderTargetView_, nullptr);
        pDeviceContext_->PSSetSamplers(0, 1, samplers);

        pDeviceContext_->ClearRenderTargetView(pRenderTargetView_, backColor);

        pDeviceContext_->RSSetViewports(1, &viewport);

        toneMapping_.RenderTonemap();
    }

#ifdef _DEBUG
    pAnnotation_->EndEvent();
#endif

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    HRESULT result = pSwapChain_->Present(0, 0);

#ifdef _DEBUG
    pAnnotation_->EndEvent();
#endif

    return SUCCEEDED(result);
}

void Renderer::RenderSkybox() {
    ID3D11ShaderResourceView* resources[] = { skybox.texture->getSRV() };
    pDeviceContext_->PSSetShaderResources(0, 1, resources);

    pDeviceContext_->IASetIndexBuffer(skybox.geometry->getIndexBuffer(), DXGI_FORMAT_R32_UINT, 0);
    ID3D11Buffer* vertexBuffers[] = { skybox.geometry->getVertexBuffer() };
    UINT strides[] = { skybox.vertexSize };
    UINT offsets[] = { 0 };
    pDeviceContext_->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    pDeviceContext_->IASetInputLayout(skybox.IL.get());
    pDeviceContext_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pDeviceContext_->VSSetShader(skybox.VS.get(), nullptr, 0);
    pDeviceContext_->VSSetConstantBuffers(0, 1, &pSkyboxWorldMatrixBuffer_);
    pDeviceContext_->VSSetConstantBuffers(1, 1, &pViewMatrixBuffer_);
    pDeviceContext_->PSSetShader(skybox.PS.get(), nullptr, 0);

    pDeviceContext_->DrawIndexed(skybox.geometry->getNumIndices(), 0, 0);
}

void Renderer::RenderObjects() {
    std::shared_ptr<SimpleTexture> prefilteredText;
    std::shared_ptr<SimpleTexture> brdfText;
    pTextureManager_.get("prefiltered", prefilteredText);
    pTextureManager_.get("brdf", brdfText);

    std::shared_ptr<ID3D11SamplerState> avgSample;
    pSamplerManager_.get("avg", avgSample);

    ID3D11SamplerState* samplers[] = { avgSample.get() };
    pDeviceContext_->PSSetSamplers(1, 1, samplers);

    ID3D11ShaderResourceView* resources[] = { sphere.irradianceMap->getSRV(), prefilteredText->getSRV(), brdfText->getSRV() };
    pDeviceContext_->PSSetShaderResources(0, 3, resources);

    pDeviceContext_->IASetIndexBuffer(sphere.geometry->getIndexBuffer(), DXGI_FORMAT_R32_UINT, 0);
    ID3D11Buffer* vertexBuffers[] = { sphere.geometry->getVertexBuffer() };
    UINT strides[] = { sphere.vertexSize };
    UINT offsets[] = { 0 };
    pDeviceContext_->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    pDeviceContext_->IASetInputLayout(sphere.IL.get());
    pDeviceContext_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pDeviceContext_->VSSetConstantBuffers(0, 1, &pWorldMatrixBuffer_);
    pDeviceContext_->VSSetConstantBuffers(1, 1, &pViewMatrixBuffer_);
    pDeviceContext_->PSSetConstantBuffers(0, 1, &pWorldMatrixBuffer_);
    pDeviceContext_->PSSetConstantBuffers(1, 1, &pViewMatrixBuffer_);
    pDeviceContext_->VSSetShader(sphere.VS.get(), nullptr, 0);
    pDeviceContext_->PSSetShader(sphere.PS.get(), nullptr, 0);

    pDeviceContext_->DrawIndexed(sphere.geometry->getNumIndices(), 0, 0);
}

bool Renderer::Resize(UINT width, UINT height) {
    width_ = max(width, 8);
    height_ = max(height, 8);

    if (!ResizeSwapChain())
        return false;

    ResizeSkybox();

    if (!SUCCEEDED(toneMapping_.Resize(width_, height_)))
        return false;

    return true;
}

bool Renderer::ResizeSwapChain() {
    if (pSwapChain_ == nullptr)
        return false;

    SAFE_RELEASE(pRenderTargetView_);

    auto result = pSwapChain_->ResizeBuffers(2, width_, height_, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    if (!SUCCEEDED(result))
        return false;

    ID3D11Texture2D* pBuffer;
    result = pSwapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBuffer);
    if (!SUCCEEDED(result))
        return false;

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // Формат RTV совпадает с форматом текстуры
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;

    result = pDevice_->CreateRenderTargetView(pBuffer, &rtvDesc, &pRenderTargetView_);
    SAFE_RELEASE(pBuffer);
    if (!SUCCEEDED(result))
        return false;

    return true;
}

void Renderer::ResizeSkybox() {
    float n = 0.01f;
    float fov = XM_PI / 3;
    float halfW = tanf(fov / 2) * n;
    float halfH = height_ / float(width_) * halfW;
    skybox.size = sqrtf(n * n + halfH * halfH + halfW * halfW) * 1.1f;
}

void Renderer::Cleanup() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (pDeviceContext_ != nullptr)
        pDeviceContext_->ClearState();

    pSampler_.reset();
    pDeviceContext_.reset();
    skybox.Cleanup();
    sphere.Cleanup();
    toneMapping_.Cleanup();
    pGeometryManager_.Cleanup();
    pVSManager_.Cleanup();
    pILManager_.Cleanup();
    pPSManager_.Cleanup();
    pTextureManager_.Cleanup();
    pSamplerManager_.Cleanup();

    SAFE_RELEASE(pRenderTargetView_);
    SAFE_RELEASE(pSwapChain_);

    SAFE_RELEASE(pFactory_);
    SAFE_RELEASE(pSelectedAdapter_);

    SAFE_RELEASE(pRasterizerState_);
    SAFE_RELEASE(pViewMatrixBuffer_);
    SAFE_RELEASE(pWorldMatrixBuffer_);
    SAFE_RELEASE(pSkyboxWorldMatrixBuffer_);
    
    if (pCamera_) {
        delete pCamera_;
        pCamera_ = nullptr;
    }
    if (pInput_) {
        delete pInput_;
        pInput_ = nullptr;
    }

#ifdef _DEBUG // Получение информации о "подвисших" объектах в отладочной сборке
    SAFE_RELEASE(pAnnotation_);
    if (pDevice_.get() != nullptr) {
        ID3D11Debug* d3dDebug = nullptr;
        pDevice_->QueryInterface(IID_PPV_ARGS(&d3dDebug));

        UINT references = pDevice_->Release();
        pDevice_ = nullptr;
        if (references > 1) {
            d3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
        }
        SAFE_RELEASE(d3dDebug);
    }
#endif
}

Renderer::~Renderer() {
    Cleanup();
}