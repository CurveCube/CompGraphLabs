#include "CubemapGenerator.h"

CubemapGenerator::CubemapGenerator(
    std::shared_ptr<ID3D11Device>& device,
    std::shared_ptr <ID3D11DeviceContext>& deviceContext,
    SimpleSamplerManager& samplerManager,
    SimpleTextureManager& textureManager,
    SimpleILManager& ILManager,
    SimplePSManager& PSManager,
    SimpleVSManager& VSManager,
    SimpleGeometryManager& GManager
) : device_(device), deviceContext_(deviceContext),
    samplerManager_(samplerManager), textureManager_(textureManager),
    ILManager_(ILManager), PSManager_(PSManager),
    VSManager_(VSManager), GManager_(GManager)
{
    viewMatrices = {
        DirectX::XMMatrixLookToLH(
            DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
            DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),
            DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
        ),
        DirectX::XMMatrixLookToLH(
            DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
            DirectX::XMVectorSet(-1.0f, 0.0f, 0.0f, 0.0f),
            DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
        ),
        DirectX::XMMatrixLookToLH(
            DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
            DirectX::XMVectorSet(0.0f,  1.0f,  0.0f, 0.0f),
            DirectX::XMVectorSet(0.0f,  0.0f,  -1.0f, 0.0f)
        ),
        DirectX::XMMatrixLookToLH(
            DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
            DirectX::XMVectorSet(0.0f,  -1.0f,  0.0f, 0.0f),
            DirectX::XMVectorSet(0.0f,  0.0f,  1.0f, 0.0f)
        ),
        DirectX::XMMatrixLookToLH(
            DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
            DirectX::XMVectorSet(0.0f,  0.0f,  1.0f, 0.0f),
            DirectX::XMVectorSet(0.0f, 1.0f,  0.0f, 0.0f)
        ),
        DirectX::XMMatrixLookToLH(
            DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
            DirectX::XMVectorSet(0.0f,  0.0f,  -1.0f, 0.0f),
            DirectX::XMVectorSet(0.0f, 1.0f,  0.0f, 0.0f)
        ),
    };

    sides = { "X+", "X-", "Y+", "Y-", "Z+", "Z-" };
}

HRESULT CubemapGenerator::init()
{
    HRESULT  result = createGeometry();
    if (SUCCEEDED(result)) {
        result = createBuffer();
    }
    return result;
}

HRESULT CubemapGenerator::generateEnvironmentMap(const std::string& key)
{
    Cleanup();

    HRESULT result = createTexture(sideSize);
    if (SUCCEEDED(result)) {
        result = createCubemapTexture(sideSize, key, true);
    }

    for (int i = 0; i < 6 && SUCCEEDED(result); i++)
    {
        result = renderMapSide(i);
        if (SUCCEEDED(result)) {
            result = renderToCubeMap(sideSize, i);
        }
    }

    std::shared_ptr<SimpleTexture> envMapTexture;
    textureManager_.get(key, envMapTexture);
    deviceContext_->GenerateMips(envMapTexture->getSRV());

    return result;
}

HRESULT CubemapGenerator::generateIrradianceMap(
    const std::string& hdrCubeMapKey,
    const std::string& key
)
{
    Cleanup();

    HRESULT result = createTexture(irradianceSideSize);
    if (SUCCEEDED(result)) {
        result = createCubemapTexture(irradianceSideSize, key, false);
    }

    for (int i = 0; i < 6 && SUCCEEDED(result); i++)
    {
        result = renderIrradianceMapSide(hdrCubeMapKey, i);
        if (SUCCEEDED(result)) {
            result = renderToCubeMap(irradianceSideSize, i);
        }
    }

    return result;
}

void CubemapGenerator::Cleanup()
{
    SAFE_RELEASE(subTexture);
    SAFE_RELEASE(subRTV);
    SAFE_RELEASE(subSRV);
    for (int i = 0; i < 6; ++i)
        SAFE_RELEASE(cubemapRTV[i]);
}

HRESULT CubemapGenerator::createTexture(UINT size)
{
    HRESULT result;
    {
        D3D11_TEXTURE2D_DESC textureDesc = {};

        textureDesc.Width = size;
        textureDesc.Height = size;
        textureDesc.MipLevels = 1;
        textureDesc.ArraySize = 1;
        textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.Usage = D3D11_USAGE_DEFAULT;
        textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        textureDesc.CPUAccessFlags = 0;
        textureDesc.MiscFlags = 0;

        result = device_->CreateTexture2D(&textureDesc, nullptr, &subTexture);
    }

    if (SUCCEEDED(result))
    {
        D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
        renderTargetViewDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        renderTargetViewDesc.Texture2D.MipSlice = 0;

        result = device_->CreateRenderTargetView(subTexture, &renderTargetViewDesc, &subRTV);
    }

    if (SUCCEEDED(result))
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
        shaderResourceViewDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
        shaderResourceViewDesc.Texture2D.MipLevels = 1;

        result = device_->CreateShaderResourceView(subTexture, &shaderResourceViewDesc, &subSRV);
    }

    return result;
}

HRESULT CubemapGenerator::createBuffer()
{
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(CameraBuffer);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    CameraBuffer cameraBuffer;
    cameraBuffer.viewProjMatrix = DirectX::XMMatrixIdentity();

    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = &cameraBuffer;
    data.SysMemPitch = sizeof(cameraBuffer);
    data.SysMemSlicePitch = 0;

    return device_->CreateBuffer(&desc, &data, &pCameraBuffer);
}

HRESULT CubemapGenerator::createCubemapTexture(UINT size, const std::string& key, bool withMipMap)
{
    ID3D11Texture2D* cubemapTexture = nullptr;
    ID3D11ShaderResourceView* cubemapSRV = nullptr;
    HRESULT result;
    {
        D3D11_TEXTURE2D_DESC textureDesc = {};

        textureDesc.Width = size;
        textureDesc.Height = size;
        textureDesc.MipLevels = 0;
        textureDesc.ArraySize = 6;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        textureDesc.Usage = D3D11_USAGE_DEFAULT;
        textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        textureDesc.CPUAccessFlags = 0;
        textureDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS | D3D11_RESOURCE_MISC_TEXTURECUBE;
        result = device_->CreateTexture2D(&textureDesc, 0, &cubemapTexture);
    }

    if (SUCCEEDED(result))
    {
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
        rtvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Texture2DArray.MipSlice = 0;
        // Only create a view to one array element.
        rtvDesc.Texture2DArray.ArraySize = 1;
        for (int i = 0; i < 6 && SUCCEEDED(result); ++i)
        {
            // Create a render target view to the ith element.
            rtvDesc.Texture2DArray.FirstArraySlice = i;
            result = device_->CreateRenderTargetView(cubemapTexture, &rtvDesc, &cubemapRTV[i]);
        }
    }

    if (SUCCEEDED(result))
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
        shaderResourceViewDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
        shaderResourceViewDesc.Texture2D.MipLevels = withMipMap ? -1 : 1;

        result = device_->CreateShaderResourceView(cubemapTexture, &shaderResourceViewDesc, &cubemapSRV);
    }

    if (SUCCEEDED(result))
    {
        textureManager_.setDevice(device_);
        textureManager_.setDeviceContext(deviceContext_);
        result = textureManager_.loadTexture(cubemapTexture, cubemapSRV, key);
    }

    return result;
}

HRESULT CubemapGenerator::renderToCubeMap(UINT size, int num)
{
    deviceContext_->OMSetRenderTargets(1, &cubemapRTV[num], nullptr);

    std::shared_ptr<ID3D11PixelShader> ps;
    std::shared_ptr<ID3D11VertexShader> vs;
    HRESULT result = PSManager_.get("copyToCubemapPS", ps);
    if (!SUCCEEDED(result))
        return result;
    result = VSManager_.get("mapping", vs);
    if (!SUCCEEDED(result))
        return result;

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = size;
    viewport.Height = size;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    deviceContext_->RSSetViewports(1, &viewport);

    std::shared_ptr<ID3D11SamplerState> sampler;
    result = samplerManager_.get("default", sampler);
    if (!SUCCEEDED(result))
        return result;

    ID3D11SamplerState* samplers[] = {
        sampler.get()
    };
    deviceContext_->PSSetSamplers(0, 1, samplers);

    ID3D11ShaderResourceView* resources[] = {
        subSRV
    };
    deviceContext_->PSSetShaderResources(0, 1, resources);
    deviceContext_->OMSetDepthStencilState(nullptr, 0);
    deviceContext_->RSSetState(nullptr);
    deviceContext_->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    deviceContext_->IASetInputLayout(nullptr);
    deviceContext_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    deviceContext_->VSSetShader(vs.get(), nullptr, 0);
    deviceContext_->PSSetShader(ps.get(), nullptr, 0);
    deviceContext_->Draw(6, 0);

    resources[0] = nullptr;
    deviceContext_->PSSetShaderResources(0, 1, resources);

    return result;
}

HRESULT CubemapGenerator::createGeometry()
{
    GManager_.setDevice(device_);

    SimpleVertex geometryXplus[] = { XMFLOAT3(0.5f, -0.5f, 0.5f),
                                    XMFLOAT3(0.5f, -0.5f, -0.5f),
                                    XMFLOAT3(0.5f, 0.5f, -0.5f),
                                    XMFLOAT3(0.5f, 0.5f, 0.5f) };
    UINT indicesXplus[] = { 3, 2, 1, 0, 3, 1 };

    HRESULT result = GManager_.loadGeometry(geometryXplus, sizeof(XMFLOAT3) * 4, indicesXplus, sizeof(UINT) * 6, "X+");
    if (!SUCCEEDED(result))
        return result;

    SimpleVertex geometryXminus[] = { XMFLOAT3(-0.5f, -0.5f, 0.5f),
                                XMFLOAT3(-0.5f, -0.5f, -0.5f),
                                XMFLOAT3(-0.5f, 0.5f, -0.5f),
                                XMFLOAT3(-0.5f, 0.5f, 0.5f) };
    UINT indicesXminus[] = { 0, 1, 2, 3, 0, 2 };

    result = GManager_.loadGeometry(geometryXminus, sizeof(XMFLOAT3) * 4, indicesXminus, sizeof(UINT) * 6, "X-");
    if (!SUCCEEDED(result))
        return result;

    SimpleVertex geometryYplus[] = { XMFLOAT3(-0.5f, 0.5f, 0.5f),
                                XMFLOAT3(0.5f, 0.5f, -0.5f),
                                XMFLOAT3(-0.5f, 0.5f, -0.5f),
                                XMFLOAT3(0.5f, 0.5f, 0.5f) };
    UINT indicesYplus[] = { 3, 0, 2, 1, 3, 2 };

    result = GManager_.loadGeometry(geometryYplus, sizeof(XMFLOAT3) * 4, indicesYplus, sizeof(UINT) * 6, "Y+");
    if (!SUCCEEDED(result))
        return result;

    SimpleVertex geometryYminus[] = { XMFLOAT3(-0.5f, -0.5f, 0.5f),
                                XMFLOAT3(0.5f, -0.5f, -0.5f),
                                XMFLOAT3(-0.5f, -0.5f, -0.5f),
                                XMFLOAT3(0.5f, -0.5f, 0.5f) };
    UINT indicesYminus[] = { 3, 1, 2, 0, 3, 2 };

    result = GManager_.loadGeometry(geometryYminus, sizeof(XMFLOAT3) * 4, indicesYminus, sizeof(UINT) * 6, "Y-");
    if (!SUCCEEDED(result))
        return result;

    SimpleVertex geometryZplus[] = { XMFLOAT3(-0.5f, 0.5f, 0.5f),
                            XMFLOAT3(0.5f, 0.5f, 0.5f),
                            XMFLOAT3(0.5f, -0.5f, 0.5f),
                            XMFLOAT3(-0.5f, -0.5f, 0.5f) };
    UINT indicesZplus[] = { 0, 1, 2, 3, 0, 2 };

    result = GManager_.loadGeometry(geometryZplus, sizeof(XMFLOAT3) * 4, indicesZplus, sizeof(UINT) * 6, "Z+");
    if (!SUCCEEDED(result))
        return result;

    SimpleVertex geometryZminus[] = { XMFLOAT3(-0.5f, -0.5f, -0.5f),
                                XMFLOAT3(0.5f, -0.5f, -0.5f),
                                XMFLOAT3(-0.5f, 0.5f, -0.5f),
                                XMFLOAT3(0.5f, 0.5f, -0.5f) };
    UINT indicesZminus[] = { 1, 3, 2, 0, 1, 2 };

    result = GManager_.loadGeometry(geometryZminus, sizeof(XMFLOAT3) * 4, indicesZminus, sizeof(UINT) * 6, "Z-");

    return result;
}

HRESULT CubemapGenerator::renderMapSide(int num)
{
    static const FLOAT backColor[4] = { 0.25f, 0.25f, 0.25f, 1.0f };
    deviceContext_->ClearRenderTargetView(subRTV, backColor);

    deviceContext_->OMSetRenderTargets(1, &subRTV, nullptr);

    std::shared_ptr<SimpleTexture> hdrText;
    HRESULT result = textureManager_.get("hdr", hdrText);
    if (!SUCCEEDED(result))
        return result;

    ID3D11ShaderResourceView* resources[] = {
        hdrText->getSRV()
    };

    deviceContext_->PSSetShaderResources(0, 1, resources);

    std::shared_ptr<ID3D11SamplerState> sampler;
    result = samplerManager_.get("default", sampler);
    if (!SUCCEEDED(result))
        return result;

    ID3D11SamplerState* samplers[] = {
        sampler.get()
    };
    deviceContext_->PSSetSamplers(0, 1, samplers);

    std::shared_ptr<ID3D11PixelShader> ps;
    std::shared_ptr<ID3D11VertexShader> vs;
    result = PSManager_.get("cubemapGenerator", ps);
    if (!SUCCEEDED(result))
        return result;
    result = VSManager_.get("cubemapGenerator", vs);
    if (!SUCCEEDED(result))
        return result;

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = sideSize;
    viewport.Height = sideSize;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    deviceContext_->RSSetViewports(1, &viewport);

    std::shared_ptr<Geometry> geom;
    result = GManager_.get(sides[num], geom);
    if (!SUCCEEDED(result))
        return result;

    if (SUCCEEDED(result)) {
        CameraBuffer cameraBuffer;
        cameraBuffer.viewProjMatrix = DirectX::XMMatrixMultiply(viewMatrices[num], projectionMatrix);
        deviceContext_->UpdateSubresource(pCameraBuffer, 0, nullptr, &cameraBuffer, 0, 0);
    }
    std::shared_ptr<ID3D11InputLayout> il;
    result = ILManager_.get("cubemapGenerator", il);
    if (!SUCCEEDED(result))
        return result;
    deviceContext_->IASetInputLayout(il.get());
    ID3D11Buffer* vertexBuffers[] = { geom->getVertexBuffer() };
    UINT strides[] = { sizeof(SimpleVertex) };
    UINT offsets[] = { 0 };
    deviceContext_->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    deviceContext_->IASetIndexBuffer(geom->getIndexBuffer(), DXGI_FORMAT_R32_UINT, 0);
    deviceContext_->OMSetDepthStencilState(nullptr, 0);
    deviceContext_->RSSetState(nullptr);
    deviceContext_->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

    deviceContext_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    deviceContext_->VSSetShader(vs.get(), nullptr, 0);
    deviceContext_->VSSetConstantBuffers(0, 1, &pCameraBuffer);
    deviceContext_->PSSetShader(ps.get(), nullptr, 0);

    deviceContext_->DrawIndexed(6, 0, 0);

    return result;
}

HRESULT CubemapGenerator::renderIrradianceMapSide(const std::string& hdrCubeMapKey, int num)
{
    static const FLOAT backColor[4] = { 0.25f, 0.25f, 0.25f, 1.0f };
    deviceContext_->ClearRenderTargetView(subRTV, backColor);

    deviceContext_->OMSetRenderTargets(1, &subRTV, nullptr);

    std::shared_ptr<SimpleTexture> hdrCubeMap;
    HRESULT result = textureManager_.get(hdrCubeMapKey, hdrCubeMap);
    if (!SUCCEEDED(result))
        return result;

    ID3D11ShaderResourceView* resources[] = {
        hdrCubeMap->getSRV()
    };

    deviceContext_->PSSetShaderResources(0, 1, resources);

    std::shared_ptr<ID3D11SamplerState> sampler;
    result = samplerManager_.get("default", sampler);
    if (!SUCCEEDED(result))
        return result;

    ID3D11SamplerState* samplers[] = {
        sampler.get()
    };
    deviceContext_->PSSetSamplers(0, 1, samplers);

    std::shared_ptr<ID3D11PixelShader> ps;
    std::shared_ptr<ID3D11VertexShader> vs;
    result = PSManager_.get("cubemapGeneratorIrradiance", ps);
    if (!SUCCEEDED(result))
        return result;
    result = VSManager_.get("cubemapGenerator", vs);
    if (!SUCCEEDED(result))
        return result;

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = irradianceSideSize;
    viewport.Height = irradianceSideSize;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    deviceContext_->RSSetViewports(1, &viewport);

    std::shared_ptr<Geometry> geom;
    result = GManager_.get(sides[num], geom);
    if (!SUCCEEDED(result))
        return result;

    if (SUCCEEDED(result)) {
        CameraBuffer cameraBuffer;
        cameraBuffer.viewProjMatrix = DirectX::XMMatrixMultiply(viewMatrices[num], projectionMatrix);
        deviceContext_->UpdateSubresource(pCameraBuffer, 0, nullptr, &cameraBuffer, 0, 0);
    }
    std::shared_ptr<ID3D11InputLayout> il;
    result = ILManager_.get("cubemapGenerator", il);
    if (!SUCCEEDED(result))
        return result;
    deviceContext_->IASetInputLayout(il.get());
    ID3D11Buffer* vertexBuffers[] = { geom->getVertexBuffer() };
    UINT strides[] = { sizeof(SimpleVertex) };
    UINT offsets[] = { 0 };
    deviceContext_->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    deviceContext_->IASetIndexBuffer(geom->getIndexBuffer(), DXGI_FORMAT_R32_UINT, 0);
    deviceContext_->OMSetDepthStencilState(nullptr, 0);
    deviceContext_->RSSetState(nullptr);
    deviceContext_->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

    deviceContext_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    deviceContext_->VSSetShader(vs.get(), nullptr, 0);
    deviceContext_->VSSetConstantBuffers(0, 1, &pCameraBuffer);
    deviceContext_->PSSetShader(ps.get(), nullptr, 0);

    deviceContext_->DrawIndexed(6, 0, 0);

    return result;
}