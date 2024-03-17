#include "CubemapGenerator.h"

HRESULT CubemapGenerator::generate(
    std::shared_ptr<ID3D11Device> device,
    std::shared_ptr<ID3D11DeviceContext> deviceContext,
	SimpleSamplerManager& samplerManager, 
	SimpleTextureManager& textureManager,
	SimpleILManager& ILManager, 
	SimplePSManager& PSManager, 
	SimpleVSManager& VSManager, 
	SimpleGeometryManager& GManager
)
{
    HRESULT result = createHdrMappedTexture(device, deviceContext);
    createCubemapTexture(device);

    result = createGgeometry(GManager);
    createBuffer(device);

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

    sides = { "X+", "X-", "Y+", "Y-", "Z+", "Z-"};

    for (int i = 0; i < 6; i++)
    {
        renderSubHdr(deviceContext, PSManager, VSManager, samplerManager, textureManager, ILManager, GManager, i);
    
        renderToCubeMap(deviceContext, PSManager, VSManager, samplerManager, i);
    }

    return result;
}

void CubemapGenerator::Cleanup()
{
	SAFE_RELEASE(subHdrMappedTexture);
	SAFE_RELEASE(subHdrMappedRTV);
    SAFE_RELEASE(subHdrMappedSRV);
    //SAFE_RELEASE(cubemapTexture);
    for (int i = 0; i < 6; ++i)
        SAFE_RELEASE(cubemapRTV[i]);
    //SAFE_RELEASE(cubemapSRV);
    SAFE_RELEASE(pCameraBuffer);
}

HRESULT CubemapGenerator::createHdrMappedTexture(std::shared_ptr<ID3D11Device> device, std::shared_ptr <ID3D11DeviceContext> deviceContext)
{
    HRESULT result;
    {
        D3D11_TEXTURE2D_DESC textureDesc = {};

        textureDesc.Width = sideSize;
        textureDesc.Height = sideSize;
        textureDesc.MipLevels = 1;
        textureDesc.ArraySize = 1;
        textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.Usage = D3D11_USAGE_DEFAULT;
        textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        textureDesc.CPUAccessFlags = 0;
        textureDesc.MiscFlags = 0;

        result = device->CreateTexture2D(&textureDesc, NULL, &subHdrMappedTexture);
    }

    if (SUCCEEDED(result))
    {
        D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
        renderTargetViewDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        renderTargetViewDesc.Texture2D.MipSlice = 0;

        result = device->CreateRenderTargetView(subHdrMappedTexture, &renderTargetViewDesc, &subHdrMappedRTV);
    }

    if (SUCCEEDED(result))
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
        shaderResourceViewDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
        shaderResourceViewDesc.Texture2D.MipLevels = 1;

        result = device->CreateShaderResourceView(subHdrMappedTexture, &shaderResourceViewDesc, &subHdrMappedSRV);
    }

    return result;
}

HRESULT CubemapGenerator::createBuffer(std::shared_ptr<ID3D11Device> device)
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

    return device->CreateBuffer(&desc, &data, &pCameraBuffer);
}

HRESULT CubemapGenerator::createCubemapTexture(std::shared_ptr<ID3D11Device> device) {
    HRESULT result;
    {
        D3D11_TEXTURE2D_DESC textureDesc = {};

        textureDesc.Width = sideSize;
        textureDesc.Height = sideSize;
        textureDesc.MipLevels = 0;
        textureDesc.ArraySize = 6;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        textureDesc.Usage = D3D11_USAGE_DEFAULT;
        textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        textureDesc.CPUAccessFlags = 0;
        textureDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS | D3D11_RESOURCE_MISC_TEXTURECUBE;
        result = device->CreateTexture2D(&textureDesc, 0, &cubemapTexture);
    }

    if (SUCCEEDED(result))
    {
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
        rtvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Texture2DArray.MipSlice = 0;
        // Only create a view to one array element.
        rtvDesc.Texture2DArray.ArraySize = 1;
        for (int i = 0; i < 6; ++i)
        {
            // Create a render target view to the ith element.
            rtvDesc.Texture2DArray.FirstArraySlice = i;
            device->CreateRenderTargetView(cubemapTexture, &rtvDesc, &cubemapRTV[i]);
        }
    }

    if (SUCCEEDED(result))
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
        shaderResourceViewDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
        shaderResourceViewDesc.Texture2D.MipLevels = 1;

        result = device->CreateShaderResourceView(cubemapTexture, &shaderResourceViewDesc, &cubemapSRV);
    }

    return result;
}

void CubemapGenerator::renderToCubeMap(
    std::shared_ptr <ID3D11DeviceContext> deviceContext, 
    SimplePSManager& PSManager, 
    SimpleVSManager& VSManager,
    SimpleSamplerManager& samplerManager,
    int num)
{
    deviceContext->OMSetRenderTargets(1, &cubemapRTV[num], nullptr);

    std::shared_ptr<ID3D11PixelShader> ps;
    std::shared_ptr<ID3D11VertexShader> vs;
    PSManager.get("copyToCubemapPS", ps);
    VSManager.get("mapping", vs);

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = sideSize;
    viewport.Height = sideSize;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    deviceContext->RSSetViewports(1, &viewport);

    std::shared_ptr<ID3D11SamplerState> sampler;
    samplerManager.get("default", sampler);

    ID3D11SamplerState* samplers[] = {
        sampler.get()
    };
    deviceContext->PSSetSamplers(0, 1, samplers);

    ID3D11ShaderResourceView* resources[] = {
        subHdrMappedSRV
    };
    deviceContext->PSSetShaderResources(0, 1, resources);
    deviceContext->OMSetDepthStencilState(nullptr, 0);
    deviceContext->RSSetState(nullptr);
    deviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    deviceContext->IASetInputLayout(nullptr);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    deviceContext->VSSetShader(vs.get(), nullptr, 0);
    deviceContext->PSSetShader(ps.get(), nullptr, 0);
    deviceContext->Draw(6, 0);
}

HRESULT CubemapGenerator::createGgeometry(SimpleGeometryManager& GManager)
{
    SimpleVertex geometryXplus[] = { XMFLOAT3(0.5f, -0.5f, 0.5f),
                                    XMFLOAT3(0.5f, -0.5f, -0.5f),
                                    XMFLOAT3(0.5f, 0.5f, -0.5f),
                                    XMFLOAT3(0.5f, 0.5f, 0.5f) };
    UINT indicesXplus[] = { 3, 2, 1, 0, 3, 1 };

    HRESULT result = GManager.loadGeometry(geometryXplus, sizeof(XMFLOAT3) * 4, indicesXplus, sizeof(UINT) * 6, "X+");

    SimpleVertex geometryXminus[] = { XMFLOAT3(-0.5f, -0.5f, 0.5f),
                                XMFLOAT3(-0.5f, -0.5f, -0.5f),
                                XMFLOAT3(-0.5f, 0.5f, -0.5f),
                                XMFLOAT3(-0.5f, 0.5f, 0.5f) };
    UINT indicesXminus[] = { 0, 1, 2, 3, 0, 2 };

    result = GManager.loadGeometry(geometryXminus, sizeof(XMFLOAT3) * 4, indicesXminus, sizeof(UINT) * 6, "X-");

    SimpleVertex geometryYplus[] = { XMFLOAT3(-0.5f, 0.5f, 0.5f),
                                XMFLOAT3(0.5f, 0.5f, -0.5f),
                                XMFLOAT3(-0.5f, 0.5f, -0.5f),
                                XMFLOAT3(0.5f, 0.5f, 0.5f) };
    UINT indicesYplus[] = { 3, 0, 2, 1, 3, 2 };

    result = GManager.loadGeometry(geometryYplus, sizeof(XMFLOAT3) * 4, indicesYplus, sizeof(UINT) * 6, "Y+");

    SimpleVertex geometryYminus[] = { XMFLOAT3(-0.5f, -0.5f, 0.5f),
                                XMFLOAT3(0.5f, -0.5f, -0.5f),
                                XMFLOAT3(-0.5f, -0.5f, -0.5f),
                                XMFLOAT3(0.5f, -0.5f, 0.5f) };
    UINT indicesYminus[] = { 3, 1, 2, 0, 3, 2 };

    result = GManager.loadGeometry(geometryYminus, sizeof(XMFLOAT3) * 4, indicesYminus, sizeof(UINT) * 6, "Y-");

    SimpleVertex geometryZplus[] = { XMFLOAT3(-0.5f, 0.5f, 0.5f),
                            XMFLOAT3(0.5f, 0.5f, 0.5f),
                            XMFLOAT3(0.5f, -0.5f, 0.5f),
                            XMFLOAT3(-0.5f, -0.5f, 0.5f) };
    UINT indicesZplus[] = { 0, 1, 2, 3, 0, 2 };

    result = GManager.loadGeometry(geometryZplus, sizeof(XMFLOAT3) * 4, indicesZplus, sizeof(UINT) * 6, "Z+");

    SimpleVertex geometryZminus[] = { XMFLOAT3(-0.5f, -0.5f, -0.5f),
                                XMFLOAT3(0.5f, -0.5f, -0.5f),
                                XMFLOAT3(-0.5f, 0.5f, -0.5f),
                                XMFLOAT3(0.5f, 0.5f, -0.5f) };
    UINT indicesZminus[] = { 1, 3, 2, 0, 1, 2 };

    result = GManager.loadGeometry(geometryZminus, sizeof(XMFLOAT3) * 4, indicesZminus, sizeof(UINT) * 6, "Z-");

    return result;
}

HRESULT CubemapGenerator::renderSubHdr(std::shared_ptr <ID3D11DeviceContext> deviceContext,
    SimplePSManager& PSManager,
    SimpleVSManager& VSManager,
    SimpleSamplerManager& samplerManager,
    SimpleTextureManager& textureManager,
    SimpleILManager& ILManager,
    SimpleGeometryManager& GManager,
    int num
)
{
    static const FLOAT backColor[4] = { 0.25f, 0.25f, 0.25f, 1.0f };
    deviceContext->ClearRenderTargetView(subHdrMappedRTV, backColor);

    std::shared_ptr<SimpleTexture> hdrText;
    HRESULT result = textureManager.get("hdr", hdrText);

    ID3D11ShaderResourceView* resources[] = {
        hdrText->getSRV()
    };

    deviceContext->PSSetShaderResources(0, 1, resources);

    deviceContext->OMSetRenderTargets(1, &subHdrMappedRTV, nullptr);

    std::shared_ptr<ID3D11SamplerState> sampler;
    result = samplerManager.get("default", sampler);
    ID3D11SamplerState* samplers[] = {
        sampler.get()
    };
    deviceContext->PSSetSamplers(0, 1, samplers);

    std::shared_ptr<ID3D11PixelShader> ps;
    std::shared_ptr<ID3D11VertexShader> vs;
    result = PSManager.get("cubemapGenerator", ps);
    result = VSManager.get("cubemapGenerator", vs);

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = sideSize;
    viewport.Height = sideSize;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    deviceContext->RSSetViewports(1, &viewport);

    std::shared_ptr<Geometry> geom;
    GManager.get(sides[num], geom);

    if (SUCCEEDED(result)) {
        CameraBuffer cameraBuffer;
        cameraBuffer.viewProjMatrix = DirectX::XMMatrixMultiply(viewMatrices[num], projectionMatrix);
        deviceContext->UpdateSubresource(pCameraBuffer, 0, nullptr, &cameraBuffer, 0, 0);
    }
    std::shared_ptr<ID3D11InputLayout> il;
    ILManager.get("cubemapGenerator", il);
    deviceContext->IASetInputLayout(il.get());
    ID3D11Buffer* vertexBuffers[] = { geom->getVertexBuffer() };
    UINT strides[] = { sizeof(SimpleVertex) };
    UINT offsets[] = { 0 };
    deviceContext->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    deviceContext->IASetIndexBuffer(geom->getIndexBuffer(), DXGI_FORMAT_R32_UINT, 0);
    deviceContext->OMSetDepthStencilState(nullptr, 0);
    deviceContext->RSSetState(nullptr);
    deviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);


    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    deviceContext->VSSetShader(vs.get(), nullptr, 0);
    deviceContext->VSSetConstantBuffers(0, 1, &pCameraBuffer);
    deviceContext->PSSetShader(ps.get(), nullptr, 0);

    deviceContext->DrawIndexed(6, 0, 0);

    return result;
}

void CubemapGenerator::getCubemapTexture(std::shared_ptr<SimpleTexture>& text)
{
    ID3D11Resource* textureResource = cubemapTexture;
    std::shared_ptr<SimpleTexture> cubemapST(new SimpleTexture(textureResource, cubemapSRV));
    text = cubemapST;
}