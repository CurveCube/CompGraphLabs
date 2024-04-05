#include "ToneMapping.h"

void ToneMapping::Cleanup() {
    CleanupTextures();

    SAFE_RELEASE(adaptBuffer_);

    samplerAvg_.reset();
    samplerMin_.reset();
    samplerMax_.reset();
    samplerDefault_.reset();
    mappingVS_.reset();
    brightnessPS_.reset();
    downsamplePS_.reset();
    tonemapPS_.reset();
    device_.reset();
    managerStorage_.reset();
};

void ToneMapping::CleanupTextures() {
    SAFE_RELEASE(frameSRV_);
    SAFE_RELEASE(frame_);
    for (auto& scaledFrame : scaledFrames_) {
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
    SAFE_RELEASE(readAvgTexture_);

    frameRTV_.reset();
    scaledFrames_.clear();
    n = 0;
}

HRESULT ToneMapping::Init(const std::shared_ptr<Device>& device, const std::shared_ptr<ManagerStorage>& managerStorage,
        int textureWidth, int textureHeight) {
    if (!device->IsInit() || !managerStorage->IsInit()) {
        return E_FAIL;
    }

    device_ = device;
    managerStorage_ = managerStorage;

    HRESULT result = CreateTextures(textureWidth, textureHeight);

    if (SUCCEEDED(result)) {
        result = managerStorage_->GetStateManager()->CreateSamplerState(samplerAvg_, D3D11_FILTER_MIN_MAG_MIP_LINEAR);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetStateManager()->CreateSamplerState(samplerMin_, D3D11_FILTER_MINIMUM_ANISOTROPIC);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetStateManager()->CreateSamplerState(samplerMax_, D3D11_FILTER_MAXIMUM_ANISOTROPIC);
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetStateManager()->CreateSamplerState(samplerDefault_, D3D11_FILTER_ANISOTROPIC);
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

        result = device_->GetDevice()->CreateBuffer(&desc, &data, &adaptBuffer_);
    }

    if (SUCCEEDED(result)) {
        result = managerStorage_->GetVSManager()->LoadShader(mappingVS_, L"shaders/mappingVS.hlsl");
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(brightnessPS_, L"shaders/brightnessPS.hlsl");
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(downsamplePS_, L"shaders/downsamplePS.hlsl");
    }
    if (SUCCEEDED(result)) {
        result = managerStorage_->GetPSManager()->LoadShader(tonemapPS_, L"shaders/tonemapPS.hlsl");
    }

    return result;
}

HRESULT ToneMapping::CreateTextures(int textureWidth, int textureHeight) {
    RawPtrTexture tmp;
    HRESULT result = CreateTexture(tmp, textureWidth, textureHeight, DXGI_FORMAT_R32G32B32A32_FLOAT);
    if (SUCCEEDED(result)) {
        frame_ = tmp.texture;
        frameSRV_ = tmp.SRV;
        frameRTV_ = std::shared_ptr<ID3D11RenderTargetView>(tmp.RTV, utilities::DXPtrDeleter<ID3D11RenderTargetView*>);

        int minSide = min(textureWidth, textureHeight);
        while (minSide >>= 1) {
            n++;
        }
        for (int i = 0; i < n + 1; i++) {
            ScaledFrame scaledFrame;
            result = CreateScaledFrame(scaledFrame, i);
            if (FAILED(result)) {
                break;
            }
            scaledFrames_.push_back(scaledFrame);
        }
    }

    if (SUCCEEDED(result)) {
        result = CreateTexture2D(&readAvgTexture_, 1, 1, DXGI_FORMAT_R32_FLOAT, true);
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

HRESULT ToneMapping::CreateTexture(RawPtrTexture& texture, int textureWidth, int textureHeight, DXGI_FORMAT format) {
    HRESULT result = CreateTexture2D(&texture.texture, textureWidth, textureHeight, format);

    if (SUCCEEDED(result)) {
        D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
        renderTargetViewDesc.Format = format;
        renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        renderTargetViewDesc.Texture2D.MipSlice = 0;

        result = device_->GetDevice()->CreateRenderTargetView(texture.texture, &renderTargetViewDesc, &texture.RTV);
    }

    if (SUCCEEDED(result)) {
        D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
        shaderResourceViewDesc.Format = format;
        shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
        shaderResourceViewDesc.Texture2D.MipLevels = 1;

        result = device_->GetDevice()->CreateShaderResourceView(texture.texture, &shaderResourceViewDesc, &texture.SRV);
    }

    return result;
}

HRESULT ToneMapping::CreateTexture2D(ID3D11Texture2D** texture, int textureWidth, int textureHeight, DXGI_FORMAT format, bool CPUAccess) {
    D3D11_TEXTURE2D_DESC textureDesc = {};

    textureDesc.Width = textureWidth;
    textureDesc.Height = textureHeight;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = format;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = CPUAccess ? D3D11_USAGE_STAGING : D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = CPUAccess ? 0 : D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = CPUAccess ? D3D11_CPU_ACCESS_READ : 0;
    textureDesc.MiscFlags = 0;

    return device_->GetDevice()->CreateTexture2D(&textureDesc, nullptr, texture);
}

bool ToneMapping::CalculateBrightness() {
    if (!IsInit()) {
        return false;
    }

    float color[4] = { 0.25f, 0.25f, 0.25f, 1.0f };
    for (int i = n; i >= 0; i--) {
        ID3D11RenderTargetView* views[] = {
            scaledFrames_[i].avg.RTV,
            scaledFrames_[i].min.RTV,
            scaledFrames_[i].max.RTV };
        device_->GetDeviceContext()->OMSetRenderTargets(3, views, nullptr);
        device_->GetDeviceContext()->ClearRenderTargetView(scaledFrames_[i].avg.RTV, color);
        device_->GetDeviceContext()->ClearRenderTargetView(scaledFrames_[i].min.RTV, color);
        device_->GetDeviceContext()->ClearRenderTargetView(scaledFrames_[i].max.RTV, color);
        
        ID3D11SamplerState* samplers[] = {
            samplerAvg_.get(),
            samplerMin_.get(),
            samplerMax_.get() };
        device_->GetDeviceContext()->PSSetSamplers(0, 3, samplers);

        D3D11_VIEWPORT viewport;
        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;
        viewport.Width = (FLOAT)pow(2, i);
        viewport.Height = (FLOAT)pow(2, i);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        device_->GetDeviceContext()->RSSetViewports(1, &viewport);

        ID3D11ShaderResourceView* resources[] = {
            i == n ? frameSRV_ : scaledFrames_[i + 1].avg.SRV,
            i == n ? frameSRV_ : scaledFrames_[i + 1].min.SRV,
            i == n ? frameSRV_ : scaledFrames_[i + 1].max.SRV
        };
        device_->GetDeviceContext()->PSSetShaderResources(0, 3, resources);
        device_->GetDeviceContext()->RSSetState(nullptr);
        device_->GetDeviceContext()->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
        device_->GetDeviceContext()->IASetInputLayout(nullptr);
        device_->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        device_->GetDeviceContext()->VSSetShader(mappingVS_->GetShader().get(), nullptr, 0);
        device_->GetDeviceContext()->PSSetShader(i == n ? brightnessPS_->GetShader().get() : downsamplePS_->GetShader().get(), nullptr, 0);
        device_->GetDeviceContext()->Draw(6, 0);
    }

    return true;
}

bool ToneMapping::RenderTonemap() {
    if (!IsInit()) {
        return false;
    }

    auto time = std::chrono::high_resolution_clock::now();
    float dtime = std::chrono::duration<float, std::milli>(time - lastFrameTime_).count() * 0.001;
    lastFrameTime_ = time;

    device_->GetDeviceContext()->CopyResource(readAvgTexture_, scaledFrames_[0].avg.texture);
    D3D11_MAPPED_SUBRESOURCE ResourceDesc = {};
    HRESULT result = device_->GetDeviceContext()->Map(readAvgTexture_, 0, D3D11_MAP_READ, 0, &ResourceDesc);
    if (FAILED(result)) {
        return false;
    }

    float avg;
    if (ResourceDesc.pData) {
        float* pData = reinterpret_cast<float*>(ResourceDesc.pData);
        avg = pData[0];
    }
    device_->GetDeviceContext()->Unmap(readAvgTexture_, 0);

    if (adapt >= 0.0) {
        adapt += (avg - adapt) * (1.0f - exp(-dtime / s));
    }
    else {
        adapt = avg;
    }

    AdaptBuffer adaptBuffer;
    adaptBuffer.adapt = XMFLOAT4(adapt, factor, 0.0f, 0.0f);

    device_->GetDeviceContext()->UpdateSubresource(adaptBuffer_, 0, nullptr, &adaptBuffer, 0, 0);

    ID3D11SamplerState* samplers[] = { samplerDefault_.get() };
    device_->GetDeviceContext()->PSSetSamplers(0, 1, samplers);

    ID3D11ShaderResourceView* resources[] = {
        frameSRV_, 
        scaledFrames_[0].avg.SRV,
        scaledFrames_[0].min.SRV,
        scaledFrames_[0].max.SRV
    };
    device_->GetDeviceContext()->PSSetShaderResources(0, 4, resources);
    device_->GetDeviceContext()->OMSetDepthStencilState(nullptr, 0);
    device_->GetDeviceContext()->RSSetState(nullptr);
    device_->GetDeviceContext()->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    device_->GetDeviceContext()->IASetInputLayout(mappingVS_->GetInputLayout().get());
    device_->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    device_->GetDeviceContext()->PSSetConstantBuffers(0, 1, &adaptBuffer_);
    device_->GetDeviceContext()->VSSetShader(mappingVS_->GetShader().get(), nullptr, 0);
    device_->GetDeviceContext()->PSSetShader(tonemapPS_->GetShader().get(), nullptr, 0);
    device_->GetDeviceContext()->Draw(6, 0);

    return true;
}

bool ToneMapping::Resize(int textureWidth, int textureHeight) {
    if (!IsInit()) {
        return false;
    }
    CleanupTextures();
    return SUCCEEDED(CreateTextures(textureWidth, textureHeight));
}