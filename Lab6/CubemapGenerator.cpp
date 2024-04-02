#include "CubemapGenerator.h"

const std::vector<D3D11_INPUT_ELEMENT_DESC> CubemapGenerator::VertexDesc = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

CubemapGenerator::CubemapGenerator(const std::shared_ptr<Device>& device, const std::shared_ptr<ManagerStorage>& managerStorage) :
    device_(device), managerStorage_(managerStorage) {
    viewMatrices_ = {
        XMMatrixLookToLH(
            XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
            XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),
            XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
        ),
        XMMatrixLookToLH(
            XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
            XMVectorSet(-1.0f, 0.0f, 0.0f, 0.0f),
            XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
        ),
        XMMatrixLookToLH(
            XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
            XMVectorSet(0.0f,  1.0f,  0.0f, 0.0f),
            XMVectorSet(0.0f,  0.0f,  -1.0f, 0.0f)
        ),
        XMMatrixLookToLH(
            XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
            XMVectorSet(0.0f,  -1.0f,  0.0f, 0.0f),
            XMVectorSet(0.0f,  0.0f,  1.0f, 0.0f)
        ),
        XMMatrixLookToLH(
            XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
            XMVectorSet(0.0f,  0.0f,  1.0f, 0.0f),
            XMVectorSet(0.0f, 1.0f,  0.0f, 0.0f)
        ),
        XMMatrixLookToLH(
            XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
            XMVectorSet(0.0f,  0.0f,  -1.0f, 0.0f),
            XMVectorSet(0.0f, 1.0f,  0.0f, 0.0f)
        ),
    };

    prefilteredRoughness_ = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
}

HRESULT CubemapGenerator::Init() {
    if (!device_->IsInit() || !managerStorage_->IsInit()) {
        return E_FAIL;
    }

    HRESULT result = CreateSides();
    if (SUCCEEDED(result)) {
        result = CreateBuffers();
    }
    return result;
}

HRESULT CubemapGenerator::CreateSides() {
    std::vector<Vertex> geometryXplus = {
        {XMFLOAT3(0.5f, -0.5f, 0.5f)},
        {XMFLOAT3(0.5f, -0.5f, -0.5f)},
        {XMFLOAT3(0.5f, 0.5f, -0.5f)},
        {XMFLOAT3(0.5f, 0.5f, 0.5f)}
    };
    std::vector<UINT> indicesXplus = { 3, 2, 1, 0, 3, 1 };

    HRESULT result = CreateSide(geometryXplus, indicesXplus);
    if (FAILED(result)) {
        return result;
    }

    std::vector<Vertex> geometryXminus = {
        {XMFLOAT3(-0.5f, -0.5f, 0.5f)},
        {XMFLOAT3(-0.5f, -0.5f, -0.5f)},
        {XMFLOAT3(-0.5f, 0.5f, -0.5f)},
        {XMFLOAT3(-0.5f, 0.5f, 0.5f)}
    };
    std::vector<UINT> indicesXminus = { 0, 1, 2, 3, 0, 2 };

    result = CreateSide(geometryXminus, indicesXminus);
    if (FAILED(result)) {
        return result;
    }

    std::vector<Vertex> geometryYplus = {
        {XMFLOAT3(-0.5f, 0.5f, 0.5f)},
        {XMFLOAT3(0.5f, 0.5f, -0.5f)},
        {XMFLOAT3(-0.5f, 0.5f, -0.5f)},
        {XMFLOAT3(0.5f, 0.5f, 0.5f)}
    };
    std::vector<UINT> indicesYplus = { 3, 0, 2, 1, 3, 2 };

    result = CreateSide(geometryYplus, indicesYplus);
    if (FAILED(result)) {
        return result;
    }

    std::vector<Vertex> geometryYminus = {
        {XMFLOAT3(-0.5f, -0.5f, 0.5f)},
        {XMFLOAT3(0.5f, -0.5f, -0.5f)},
        {XMFLOAT3(-0.5f, -0.5f, -0.5f)},
        {XMFLOAT3(0.5f, -0.5f, 0.5f)}
    };
    std::vector<UINT> indicesYminus = { 3, 1, 2, 0, 3, 2 };

    result = CreateSide(geometryYminus, indicesYminus);
    if (FAILED(result)) {
        return result;
    }

    std::vector<Vertex> geometryZplus = {
        {XMFLOAT3(-0.5f, 0.5f, 0.5f)},
        {XMFLOAT3(0.5f, 0.5f, 0.5f)},
        {XMFLOAT3(0.5f, -0.5f, 0.5f)},
        {XMFLOAT3(-0.5f, -0.5f, 0.5f)}
    };
    std::vector<UINT> indicesZplus = { 0, 1, 2, 3, 0, 2 };

    result = CreateSide(geometryZplus, indicesZplus);
    if (FAILED(result)) {
        return result;
    }

    std::vector<Vertex> geometryZminus = {
        {XMFLOAT3(-0.5f, -0.5f, -0.5f)},
        {XMFLOAT3(0.5f, -0.5f, -0.5f)},
        {XMFLOAT3(-0.5f, 0.5f, -0.5f)},
        {XMFLOAT3(0.5f, 0.5f, -0.5f)}
    };
    std::vector<UINT> indicesZminus = { 1, 3, 2, 0, 1, 2 };

    result = CreateSide(geometryZminus, indicesZminus);

    return result;
}

HRESULT CubemapGenerator::CreateSide(const std::vector<Vertex>& vertices, const std::vector<UINT>& indices) {
    Side side;
    
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(Vertex) * vertices.size();
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = vertices.data();
    data.SysMemPitch = sizeof(Vertex) * vertices.size();
    data.SysMemSlicePitch = 0;

    HRESULT result = device_->GetDevice()->CreateBuffer(&desc, &data, &side.vertexBuffer_);

    if (SUCCEEDED(result)) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(UINT) * indices.size();
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = indices.data();
        data.SysMemPitch = sizeof(UINT) * indices.size();
        data.SysMemSlicePitch = 0;

        result = device_->GetDevice()->CreateBuffer(&desc, &data, &side.indexBuffer_);
    }

    sides_.push_back(side);

    return result;
}

HRESULT CubemapGenerator::CreateBuffers() {
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(ViewMatrixBuffer);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    HRESULT result = device_->GetDevice()->CreateBuffer(&desc, nullptr, &viewMatrixBuffer_);

    if (SUCCEEDED(result)) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(RoughnessBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        result = device_->GetDevice()->CreateBuffer(&desc, nullptr, &roughnessBuffer_);
    }

    return result;
}

HRESULT CubemapGenerator::GenerateEnvironmentMap(const std::string& hdrname, std::shared_ptr<ID3D11ShaderResourceView>& environmentMap) {
    if (!IsInit()) {
        return E_FAIL;
    }

    HRESULT result = managerStorage_->GetTextureManager()->LoadHDRTexture(hdrtexture_, hdrname);
    if (SUCCEEDED(result)) {
        result = CreateCubemapTexture(sideSize, &environmentMapTexture_, environmentMap_, true);
    }

    for (int i = 0; i < 6 && SUCCEEDED(result); i++) {
        result = CreateCubemapSubRTV(environmentMapTexture_, (Sides)i);
        if (SUCCEEDED(result)) {
            result = RenderEnvironmentMapSide((Sides)i);
        }
    }

    if (SUCCEEDED(result)) {
        environmentMap = environmentMap_;
    }

    return result;
}

HRESULT CubemapGenerator::CreateCubemapTexture(UINT size, ID3D11Texture2D** texture, std::shared_ptr<ID3D11ShaderResourceView>& SRV, bool withMipMap) {
    SAFE_RELEASE(*texture);

    ID3D11ShaderResourceView* cubemapSRV = nullptr;
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = size;
    textureDesc.Height = size;
    textureDesc.MipLevels = withMipMap ? 0 : 1;
    textureDesc.ArraySize = 6;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS | D3D11_RESOURCE_MISC_TEXTURECUBE;
    HRESULT result = device_->GetDevice()->CreateTexture2D(&textureDesc, nullptr, texture);

    if (SUCCEEDED(result)) {
        D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
        shaderResourceViewDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
        shaderResourceViewDesc.Texture2D.MipLevels = withMipMap ? -1 : 1;

        result = device_->GetDevice()->CreateShaderResourceView(*texture, &shaderResourceViewDesc, &cubemapSRV);
    }

    if (SUCCEEDED(result)) {
        SRV = std::shared_ptr<ID3D11ShaderResourceView>(cubemapSRV, utilities::DXPtrDeleter<ID3D11ShaderResourceView*>);
    }

    return result;
}

HRESULT CubemapGenerator::CreateCubemapSubRTV(ID3D11Texture2D* texture, Sides side) {
    CleanupSubResources();

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
    rtvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
    rtvDesc.Texture2DArray.MipSlice = 0;
    rtvDesc.Texture2DArray.ArraySize = 1; // Only create a view to one array element.
    rtvDesc.Texture2DArray.FirstArraySlice = side;

    return device_->GetDevice()->CreateRenderTargetView(texture, &rtvDesc, &subRTV_);
}

HRESULT CubemapGenerator::GenerateIrradianceMap(std::shared_ptr<ID3D11ShaderResourceView>& irradianceMap) {
    if (!IsInit()) {
        return E_FAIL;
    }

    HRESULT result = CreateCubemapTexture(irradianceSideSize, &irradianceMapTexture_, irradianceMap_, false);

    for (int i = 0; i < 6 && SUCCEEDED(result); i++) {
        result = CreateCubemapSubRTV(irradianceMapTexture_, (Sides)i);
        if (SUCCEEDED(result)) {
            result = RenderIrradianceMapSide((Sides)i);
        }
    }

    if (SUCCEEDED(result)) {
        irradianceMap = irradianceMap_;
    }

    return result;
}

HRESULT CubemapGenerator::GeneratePrefilteredMap(std::shared_ptr<ID3D11ShaderResourceView>& prefilteredMap) {
    if (!IsInit()) {
        return E_FAIL;
    }

    HRESULT result = CreateCubemapTexture(prefilteredSideSize, &prefilteredMapTexture_, prefilteredMap_, true);

    for (int i = 0; i < 6 && SUCCEEDED(result); i++) {
        size_t mipmapSize = prefilteredSideSize;
        for (int j = 0; j < prefilteredRoughness_.size() && SUCCEEDED(result); j++) {
            result = CreatePrefilteredSubRTV((Sides)i, j);
            if (SUCCEEDED(result)) {
                result = RenderPrefilteredMap((Sides)i, j, mipmapSize);
            }
            mipmapSize >>= 1;
        }
    }

    if (SUCCEEDED(result)) {
        prefilteredMap = prefilteredMap_;
    }

    return result;
}

HRESULT CubemapGenerator::GenerateBRDF(std::shared_ptr<ID3D11ShaderResourceView>& BRDF) {
    if (!IsInit()) {
        return E_FAIL;
    }

    HRESULT result = CreateBRDFTexture();
    if (SUCCEEDED(result)) {
        result = RenderBRDF();
    }
    if (SUCCEEDED(result)) {
        BRDF = BRDF_;
    }

    return result;
}

void CubemapGenerator::Cleanup() {
    device_.reset();
    managerStorage_.reset();
    hdrtexture_.reset();
    environmentMap_.reset();
    irradianceMap_.reset();
    prefilteredMap_.reset();
    BRDF_.reset();

    for (auto& side : sides_) {
        SAFE_RELEASE(side.indexBuffer_);
        SAFE_RELEASE(side.vertexBuffer_);
    }
    SAFE_RELEASE(viewMatrixBuffer_);
    SAFE_RELEASE(roughnessBuffer_);
    SAFE_RELEASE(environmentMapTexture_);
    SAFE_RELEASE(irradianceMapTexture_);
    SAFE_RELEASE(prefilteredMapTexture_);
    SAFE_RELEASE(BRDFTexture_);
    CleanupSubResources();
}

void CubemapGenerator::CleanupSubResources() {
    SAFE_RELEASE(subRTV_);
};

HRESULT CubemapGenerator::RenderBRDF() {
    CleanupSubResources();

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
    rtvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;

    HRESULT result = device_->GetDevice()->CreateRenderTargetView(BRDFTexture_, &rtvDesc, &subRTV_);
    if (FAILED(result)) {
        return result;
    }

    device_->GetDeviceContext()->OMSetRenderTargets(1, &subRTV_, nullptr);

    std::shared_ptr<VertexShader> vs;
    std::shared_ptr<PixelShader> ps;
    result = managerStorage_->GetVSManager()->LoadShader(vs, L"shaders/brdfVS.hlsl");
    if (FAILED(result)) {
        return result;
    }
    result = managerStorage_->GetPSManager()->LoadShader(ps, L"shaders/brdfPS.hlsl");
    if (FAILED(result)) {
        return result;
    }

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = prefilteredSideSize;
    viewport.Height = prefilteredSideSize;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    device_->GetDeviceContext()->RSSetViewports(1, &viewport);

    device_->GetDeviceContext()->OMSetDepthStencilState(nullptr, 0);
    std::shared_ptr<ID3D11RasterizerState> rasterizerState;
    result = managerStorage_->GetStateManager()->CreateRasterizerState(rasterizerState);
    if (FAILED(result)) {
        return result;
    }
    device_->GetDeviceContext()->RSSetState(rasterizerState.get());
    device_->GetDeviceContext()->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    device_->GetDeviceContext()->IASetInputLayout(vs->GetInputLayout().get());
    device_->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    device_->GetDeviceContext()->VSSetShader(vs->GetShader().get(), nullptr, 0);
    device_->GetDeviceContext()->PSSetShader(ps->GetShader().get(), nullptr, 0);
    device_->GetDeviceContext()->Draw(6, 0);

    return result;
}

HRESULT CubemapGenerator::CreatePrefilteredSubRTV(Sides side, int mipSlice) {
    CleanupSubResources();

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
    rtvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
    rtvDesc.Texture2DArray.MipSlice = mipSlice;
    rtvDesc.Texture2DArray.ArraySize = 1; // Only create a view to one array element.
    rtvDesc.Texture2DArray.FirstArraySlice = side;

    return device_->GetDevice()->CreateRenderTargetView(prefilteredMapTexture_, &rtvDesc, &subRTV_);;
}

HRESULT CubemapGenerator::CreateBRDFTexture() {
    ID3D11ShaderResourceView* srv = nullptr;
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = prefilteredSideSize;
    textureDesc.Height = prefilteredSideSize;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;

    HRESULT result = device_->GetDevice()->CreateTexture2D(&textureDesc, nullptr, &BRDFTexture_);
    if (SUCCEEDED(result)) {
        D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
        shaderResourceViewDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
        shaderResourceViewDesc.Texture2D.MipLevels = 1;

        result = device_->GetDevice()->CreateShaderResourceView(BRDFTexture_, &shaderResourceViewDesc, &srv);
    }
    if (SUCCEEDED(result)) {
        BRDF_ = std::shared_ptr<ID3D11ShaderResourceView>(srv, utilities::DXPtrDeleter<ID3D11ShaderResourceView*>);
    }

    return result;
}

HRESULT CubemapGenerator::RenderEnvironmentMapSide(Sides side) {
    device_->GetDeviceContext()->OMSetRenderTargets(1, &subRTV_, nullptr);

    ID3D11ShaderResourceView* resources[] = { hdrtexture_->GetSRV().get() };
    device_->GetDeviceContext()->PSSetShaderResources(0, 1, resources);

    std::shared_ptr<ID3D11SamplerState> sampler;
    HRESULT result = managerStorage_->GetStateManager()->CreateSamplerState(sampler, D3D11_FILTER_ANISOTROPIC);
    if (FAILED(result)) {
        return result;
    }

    ID3D11SamplerState* samplers[] = { sampler.get() };
    device_->GetDeviceContext()->PSSetSamplers(0, 1, samplers);

    std::shared_ptr<VertexShader> vs;
    std::shared_ptr<PixelShader> ps;
    result = managerStorage_->GetVSManager()->LoadShader(vs, L"shaders/cubemapGeneratorVS.hlsl", {}, VertexDesc);
    if (FAILED(result)) {
        return result;
    }
    result = managerStorage_->GetPSManager()->LoadShader(ps, L"shaders/cubemapGeneratorPS.hlsl");
    if (FAILED(result)) {
        return result;
    }

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = sideSize;
    viewport.Height = sideSize;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    device_->GetDeviceContext()->RSSetViewports(1, &viewport);

    ViewMatrixBuffer viewMatrix;
    viewMatrix.viewProjectionMatrix = XMMatrixMultiply(viewMatrices_[side], projectionMatrix_);
    device_->GetDeviceContext()->UpdateSubresource(viewMatrixBuffer_, 0, nullptr, &viewMatrix, 0, 0);

    device_->GetDeviceContext()->IASetInputLayout(vs->GetInputLayout().get());
    ID3D11Buffer* vertexBuffers[] = { sides_[side].vertexBuffer_ };
    UINT strides[] = { sizeof(Vertex) };
    UINT offsets[] = { 0 };
    device_->GetDeviceContext()->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    device_->GetDeviceContext()->IASetIndexBuffer(sides_[side].indexBuffer_, DXGI_FORMAT_R32_UINT, 0);
    device_->GetDeviceContext()->OMSetDepthStencilState(nullptr, 0);
    std::shared_ptr<ID3D11RasterizerState> rasterizerState;
    result = managerStorage_->GetStateManager()->CreateRasterizerState(rasterizerState);
    if (FAILED(result)) {
        return result;
    }
    device_->GetDeviceContext()->RSSetState(rasterizerState.get());
    device_->GetDeviceContext()->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    device_->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    device_->GetDeviceContext()->VSSetShader(vs->GetShader().get(), nullptr, 0);
    device_->GetDeviceContext()->VSSetConstantBuffers(0, 1, &viewMatrixBuffer_);
    device_->GetDeviceContext()->PSSetShader(ps->GetShader().get(), nullptr, 0);
    device_->GetDeviceContext()->DrawIndexed(6, 0, 0);

    return result;
}

HRESULT CubemapGenerator::RenderIrradianceMapSide(Sides side) {
    device_->GetDeviceContext()->OMSetRenderTargets(1, &subRTV_, nullptr);

    ID3D11ShaderResourceView* resources[] = { environmentMap_.get() };
    device_->GetDeviceContext()->PSSetShaderResources(0, 1, resources);

    std::shared_ptr<ID3D11SamplerState> sampler;
    HRESULT result = managerStorage_->GetStateManager()->CreateSamplerState(sampler, D3D11_FILTER_ANISOTROPIC);
    if (FAILED(result)) {
        return result;
    }

    ID3D11SamplerState* samplers[] = { sampler.get() };
    device_->GetDeviceContext()->PSSetSamplers(0, 1, samplers);

    std::shared_ptr<VertexShader> vs;
    std::shared_ptr<PixelShader> ps;
    result = managerStorage_->GetVSManager()->LoadShader(vs, L"shaders/cubemapGeneratorVS.hlsl", {}, VertexDesc);
    if (FAILED(result)) {
        return result;
    }
    result = managerStorage_->GetPSManager()->LoadShader(ps, L"shaders/cubemapGeneratorIrradiancePS.hlsl");
    if (FAILED(result)) {
        return result;
    }

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = irradianceSideSize;
    viewport.Height = irradianceSideSize;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    device_->GetDeviceContext()->RSSetViewports(1, &viewport);

    ViewMatrixBuffer viewMatrix;
    viewMatrix.viewProjectionMatrix = XMMatrixMultiply(viewMatrices_[side], projectionMatrix_);
    device_->GetDeviceContext()->UpdateSubresource(viewMatrixBuffer_, 0, nullptr, &viewMatrix, 0, 0);

    device_->GetDeviceContext()->IASetInputLayout(vs->GetInputLayout().get());
    ID3D11Buffer* vertexBuffers[] = { sides_[side].vertexBuffer_ };
    UINT strides[] = { sizeof(Vertex) };
    UINT offsets[] = { 0 };
    device_->GetDeviceContext()->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    device_->GetDeviceContext()->IASetIndexBuffer(sides_[side].indexBuffer_, DXGI_FORMAT_R32_UINT, 0);
    device_->GetDeviceContext()->OMSetDepthStencilState(nullptr, 0);
    std::shared_ptr<ID3D11RasterizerState> rasterizerState;
    result = managerStorage_->GetStateManager()->CreateRasterizerState(rasterizerState);
    if (FAILED(result)) {
        return result;
    }
    device_->GetDeviceContext()->RSSetState(rasterizerState.get());
    device_->GetDeviceContext()->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    device_->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    device_->GetDeviceContext()->VSSetShader(vs->GetShader().get(), nullptr, 0);
    device_->GetDeviceContext()->VSSetConstantBuffers(0, 1, &viewMatrixBuffer_);
    device_->GetDeviceContext()->PSSetShader(ps->GetShader().get(), nullptr, 0);
    device_->GetDeviceContext()->DrawIndexed(6, 0, 0);

    return result;
}

HRESULT CubemapGenerator::RenderPrefilteredMap(Sides side, int mipSlice, int mipMapSize) {
    device_->GetDeviceContext()->OMSetRenderTargets(1, &subRTV_, nullptr);

    ID3D11ShaderResourceView* resources[] = { environmentMap_.get() };
    device_->GetDeviceContext()->PSSetShaderResources(0, 1, resources);

    std::shared_ptr<ID3D11SamplerState> sampler;
    HRESULT result = managerStorage_->GetStateManager()->CreateSamplerState(sampler, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP,
        D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP);
    if (FAILED(result)) {
        return result;
    }

    ID3D11SamplerState* samplers[] = { sampler.get() };
    device_->GetDeviceContext()->PSSetSamplers(0, 1, samplers);

    std::shared_ptr<VertexShader> vs;
    std::shared_ptr<PixelShader> ps;
    result = managerStorage_->GetVSManager()->LoadShader(vs, L"shaders/cubemapGeneratorVS.hlsl", {}, VertexDesc);
    if (FAILED(result)) {
        return result;
    }
    result = managerStorage_->GetPSManager()->LoadShader(ps, L"shaders/prefilteredColorPS.hlsl");
    if (FAILED(result)) {
        return result;
    }

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = mipMapSize;
    viewport.Height = mipMapSize;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    device_->GetDeviceContext()->RSSetViewports(1, &viewport);

    ViewMatrixBuffer viewMatrix;
    viewMatrix.viewProjectionMatrix = XMMatrixMultiply(viewMatrices_[side], projectionMatrix_);
    device_->GetDeviceContext()->UpdateSubresource(viewMatrixBuffer_, 0, nullptr, &viewMatrix, 0, 0);

    RoughnessBuffer roughnessBuffer;
    roughnessBuffer.roughness.x = prefilteredRoughness_[mipSlice];
    device_->GetDeviceContext()->UpdateSubresource(roughnessBuffer_, 0, nullptr, &roughnessBuffer, 0, 0);

    device_->GetDeviceContext()->IASetInputLayout(vs->GetInputLayout().get());
    ID3D11Buffer* vertexBuffers[] = { sides_[side].vertexBuffer_ };
    UINT strides[] = { sizeof(Vertex) };
    UINT offsets[] = { 0 };
    device_->GetDeviceContext()->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    device_->GetDeviceContext()->IASetIndexBuffer(sides_[side].indexBuffer_, DXGI_FORMAT_R32_UINT, 0);
    device_->GetDeviceContext()->OMSetDepthStencilState(nullptr, 0);
    std::shared_ptr<ID3D11RasterizerState> rasterizerState;
    result = managerStorage_->GetStateManager()->CreateRasterizerState(rasterizerState);
    if (FAILED(result)) {
        return result;
    }
    device_->GetDeviceContext()->RSSetState(rasterizerState.get());
    device_->GetDeviceContext()->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    device_->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    device_->GetDeviceContext()->VSSetShader(vs->GetShader().get(), nullptr, 0);
    device_->GetDeviceContext()->VSSetConstantBuffers(0, 1, &viewMatrixBuffer_);
    device_->GetDeviceContext()->PSSetConstantBuffers(0, 1, &roughnessBuffer_);
    device_->GetDeviceContext()->PSSetShader(ps->GetShader().get(), nullptr, 0);
    device_->GetDeviceContext()->DrawIndexed(6, 0, 0);

    return result;
}