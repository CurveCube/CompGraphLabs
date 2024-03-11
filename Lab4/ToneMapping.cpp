#include "ToneMapping.h"

ToneMapping::~ToneMapping() {
    CleanUpTextures();
    SAFE_RELEASE(m_adaptBuffer);
}

void ToneMapping::Cleanup() {
    CleanUpTextures();
    SAFE_RELEASE(m_adaptBuffer);
    m_sampler_avg.reset();
    m_sampler_min.reset();
    m_sampler_max.reset();

    m_mappingVS.reset();
    m_brightnessPS.reset();
    m_downsamplePS.reset();
    m_toneMapPS.reset();

    m_device.reset();
    m_deviceContext.reset();
};

void ToneMapping::CleanUpTextures() {
    SAFE_RELEASE(m_frame.SRV);
    SAFE_RELEASE(m_frame.RTV);
    SAFE_RELEASE(m_frame.texture);

    for (auto& scaledFrame : m_scaledFrames)
    {
        SAFE_RELEASE(scaledFrame.avg.SRV);
        SAFE_RELEASE(scaledFrame.avg.RTV);
        SAFE_RELEASE(scaledFrame.avg.texture);
        SAFE_RELEASE(scaledFrame.min.SRV);
        SAFE_RELEASE(scaledFrame.min.RTV);
        SAFE_RELEASE(scaledFrame.min.texture);
        SAFE_RELEASE(scaledFrame.max.SRV);
        SAFE_RELEASE(scaledFrame.max.RTV);
        SAFE_RELEASE(scaledFrame.max.texture);
    }
    SAFE_RELEASE(m_readAvgTexture);

    m_scaledFrames.clear();
    n = 0;
}

HRESULT ToneMapping::Init(std::shared_ptr <ID3D11Device> device, std::shared_ptr <ID3D11DeviceContext> deviceContext,
                          SimpleVSManager& VSManager, SimplePSManager& PSManager, SimpleSamplerManager& samplerManager,
                          int textureWidth, int textureHeight) {
    m_device = device;
    m_deviceContext = deviceContext;

    HRESULT result = CreateTextures(textureWidth, textureHeight);

    if (SUCCEEDED(result)) {
        result = samplerManager.get("avg", m_sampler_avg);
    }
    if (SUCCEEDED(result)) {
        result = samplerManager.get("min", m_sampler_min);
    }
    if (SUCCEEDED(result)) {
        result = samplerManager.get("max", m_sampler_max);
    }

    if (SUCCEEDED(result)) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(AdaptBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        AdaptBuffer adaptBuffer;
        adaptBuffer.adapt = XMFLOAT4(0.0f, s, 0.0f, 0.0f);

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = &adaptBuffer;
        data.SysMemPitch = sizeof(adaptBuffer);
        data.SysMemSlicePitch = 0;

        result = device->CreateBuffer(&desc, &data, &m_adaptBuffer);
    }

    if (SUCCEEDED(result)) {
        result = VSManager.get("mapping", m_mappingVS);
    }
    if (SUCCEEDED(result)) {
        result = PSManager.get("brightness", m_brightnessPS);
    }
    if (SUCCEEDED(result)) {
        result = PSManager.get("downsample", m_downsamplePS);
    }
    if (SUCCEEDED(result)) {
        result = PSManager.get("tonemap", m_toneMapPS);
    }

    return result;
}

HRESULT ToneMapping::CreateTextures(int textureWidth, int textureHeight) {
    HRESULT result = CreateTexture(m_frame, textureWidth, textureHeight, DXGI_FORMAT_R16G16B16A16_FLOAT);

    if (SUCCEEDED(result)) {
        int minSide = min(textureWidth, textureHeight);

        while (minSide >>= 1) {
            n++;
        }

        for (int i = 0; i < n + 1; i++) {
            ScaledFrame scaledFrame;

            result = CreateScaledFrame(scaledFrame, i);
            if (!SUCCEEDED(result))
                break;

            m_scaledFrames.push_back(scaledFrame);
        }
    }

    if (SUCCEEDED(result)) {
        result = CreateTexture2D(&m_readAvgTexture, 1, 1, DXGI_FORMAT_R32_FLOAT, true);
    }

    return result;
}

HRESULT ToneMapping::CreateScaledFrame(ScaledFrame& scaledFrame, int num) {
    int size = pow(2, num);
    HRESULT result = CreateTexture(scaledFrame.avg, size, size, DXGI_FORMAT_R32_FLOAT);
    
    if (SUCCEEDED(result)) {
        result = CreateTexture(scaledFrame.min, size, size, DXGI_FORMAT_R32_FLOAT);
    }
    if (SUCCEEDED(result)) {
        result = CreateTexture(scaledFrame.max, size, size, DXGI_FORMAT_R32_FLOAT);
    }

    return result;
}

HRESULT ToneMapping::CreateTexture(Texture& texture, int textureWidth, int textureHeight, DXGI_FORMAT format) {
    HRESULT result = CreateTexture2D(&texture.texture, textureWidth, textureHeight, format);

    if (SUCCEEDED(result)) {
        D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
        renderTargetViewDesc.Format = format;
        renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        renderTargetViewDesc.Texture2D.MipSlice = 0;

        result = m_device->CreateRenderTargetView(texture.texture, &renderTargetViewDesc, &texture.RTV);
    }

    if (SUCCEEDED(result)) {
        D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
        shaderResourceViewDesc.Format = format;
        shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
        shaderResourceViewDesc.Texture2D.MipLevels = 1;

        result = m_device->CreateShaderResourceView(texture.texture, &shaderResourceViewDesc, &texture.SRV);
    }

    return result;
}

HRESULT ToneMapping::CreateTexture2D(ID3D11Texture2D** texture, int textureWidth, int textureHeight, DXGI_FORMAT format, bool CPUAccess) {
    D3D11_TEXTURE2D_DESC textureDesc = {};

    textureDesc.Width = textureWidth;
    textureDesc.Height = textureHeight;
    textureDesc.MipLevels = CPUAccess ? 0 : 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = format;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = CPUAccess ? D3D11_USAGE_STAGING : D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = CPUAccess ? 0 : D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = CPUAccess ? D3D11_CPU_ACCESS_READ : 0;
    textureDesc.MiscFlags = 0;

    return m_device->CreateTexture2D(&textureDesc, nullptr, texture);
}

void ToneMapping::SetRenderTarget() {
    m_deviceContext->OMSetRenderTargets(1, &m_frame.RTV, nullptr);
}

void ToneMapping::ClearRenderTarget() {
    m_deviceContext->OMSetRenderTargets(1, &m_frame.RTV, nullptr);

    float color[4] = { 0.25f, 0.25f, 0.25f, 1.0f };
    m_deviceContext->ClearRenderTargetView(m_frame.RTV, color);

    for (auto& scaledFrame : m_scaledFrames)
    {
        m_deviceContext->OMSetRenderTargets(1, &scaledFrame.avg.RTV, nullptr);
        m_deviceContext->ClearRenderTargetView(scaledFrame.avg.RTV, color);
    }
}

void ToneMapping::RenderBrightness() {
    for (int i = n; i >= 0; i--) {
        ID3D11RenderTargetView* views[] = {
            m_scaledFrames[i].avg.RTV,
            m_scaledFrames[i].min.RTV,
            m_scaledFrames[i].max.RTV };
        m_deviceContext->OMSetRenderTargets(3, views, nullptr);
        
        ID3D11SamplerState* samplers[] = {
            m_sampler_avg.get(),
            m_sampler_min.get(),
            m_sampler_max.get() };
        m_deviceContext->PSSetSamplers(0, 3, samplers);

        D3D11_VIEWPORT viewport;
        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;
        viewport.Width = (FLOAT)pow(2, i);
        viewport.Height = (FLOAT)pow(2, i);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        m_deviceContext->RSSetViewports(1, &viewport);

        ID3D11ShaderResourceView* resources[] = {
            i == n ? m_frame.SRV : m_scaledFrames[i + 1].avg.SRV,
            i == n ? m_frame.SRV : m_scaledFrames[i + 1].min.SRV,
            i == n ? m_frame.SRV : m_scaledFrames[i + 1].max.SRV
        };
        m_deviceContext->PSSetShaderResources(0, 3, resources);
        m_deviceContext->OMSetDepthStencilState(nullptr, 0);
        m_deviceContext->RSSetState(nullptr);
        m_deviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
        m_deviceContext->IASetInputLayout(nullptr);
        m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_deviceContext->VSSetShader(m_mappingVS.get(), nullptr, 0);
        m_deviceContext->PSSetShader(i == n ? m_brightnessPS.get() : m_downsamplePS.get(), nullptr, 0);
        m_deviceContext->Draw(6, 0);
    }
}

void ToneMapping::RenderTonemap() {
    auto time = std::chrono::high_resolution_clock::now();
    float dtime = std::chrono::duration<float, std::milli>(time - m_lastFrame).count() * 0.001;
    m_lastFrame = time;

    m_deviceContext->CopyResource(m_readAvgTexture, m_scaledFrames[0].avg.texture);
    D3D11_MAPPED_SUBRESOURCE ResourceDesc = {};
    HRESULT hr = m_deviceContext->Map(m_readAvgTexture, 0, D3D11_MAP_READ, 0, &ResourceDesc);

    float avg;
    if (ResourceDesc.pData)
    {
        float* pData = reinterpret_cast<float*>(ResourceDesc.pData);
        avg = pData[0];
    }
    m_deviceContext->Unmap(m_readAvgTexture, 0);

    if (adapt >= 0.0) {
        adapt += (avg - adapt) * (1.0f - exp(-dtime / s));
    }
    else {
        adapt = avg;
    }

    AdaptBuffer adaptBuffer;
    adaptBuffer.adapt = XMFLOAT4(adapt, 0.0f, 0.0f, 0.0f);

    m_deviceContext->UpdateSubresource(m_adaptBuffer, 0, nullptr, &adaptBuffer, 0, 0);

    ID3D11ShaderResourceView* resources[] = {
        m_frame.SRV, 
        m_scaledFrames[0].avg.SRV,
        m_scaledFrames[0].min.SRV,
        m_scaledFrames[0].max.SRV
    };
    m_deviceContext->PSSetShaderResources(0, 4, resources);
    m_deviceContext->OMSetDepthStencilState(nullptr, 0);
    m_deviceContext->RSSetState(nullptr);
    m_deviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    m_deviceContext->IASetInputLayout(nullptr);
    m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_deviceContext->PSSetConstantBuffers(0, 1, &m_adaptBuffer);
    m_deviceContext->VSSetShader(m_mappingVS.get(), nullptr, 0);
    m_deviceContext->PSSetShader(m_toneMapPS.get(), nullptr, 0);
    m_deviceContext->Draw(6, 0);
}

HRESULT ToneMapping::Resize(int textureWidth, int textureHeight) {
    CleanUpTextures();
    return CreateTextures(textureWidth, textureHeight);
}

void ToneMapping::ResetEyeAdaptation() {
    adapt = -1.0f;
}