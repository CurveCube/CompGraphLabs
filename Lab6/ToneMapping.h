#pragma once

#include "ManagerStorage.hpp"
#include <vector>
#include <chrono>

class ToneMapping {
    struct RawPtrTexture {
        ID3D11Texture2D* texture = nullptr;
        ID3D11RenderTargetView* RTV = nullptr;
        ID3D11ShaderResourceView* SRV = nullptr;
    };

    struct ScaledFrame {
        RawPtrTexture avg;
        RawPtrTexture min;
        RawPtrTexture max;
    };

    struct AdaptBuffer {
        XMFLOAT4 adapt;
    };

public:
    ToneMapping() = default;

    HRESULT Init(const std::shared_ptr<Device>& device, const std::shared_ptr<ManagerStorage>& managerStorage,
        int textureWidth, int textureHeight);
    bool CalculateBrightness();
    bool RenderTonemap();
    bool Resize(int textureWidth, int textureHeight);
    void Cleanup();

    bool IsInit() const {
        return !!device_;
    };

    std::shared_ptr<ID3D11RenderTargetView> GetRenderTarget() const {
        return frameRTV_;
    };

    void ResetEyeAdaptation() {
        adapt = -1.0f;
    };

    void SetFactor(float f) {
        factor = f;
    };

    float GetFactor() const {
        return factor;
    };

    ~ToneMapping() {
        Cleanup();
    };

private:
    HRESULT CreateTextures(int textureWidth, int textureHeight);
    HRESULT CreateScaledFrame(ScaledFrame& scaledFrame, int num);
    HRESULT CreateTexture(RawPtrTexture& texture, int textureWidth, int textureHeight, DXGI_FORMAT format);
    HRESULT CreateTexture2D(ID3D11Texture2D** texture, int textureWidth, int textureHeight, DXGI_FORMAT format, bool CPUAccess = false);
    void CleanupTextures();

    std::shared_ptr<Device> device_; // provided externally <-
    std::shared_ptr<ManagerStorage> managerStorage_; // provided externally <-

    ID3D11Texture2D* frame_ = nullptr; // always remains only inside the class #
    ID3D11ShaderResourceView* frameSRV_ = nullptr; // always remains only inside the class #
    std::shared_ptr<ID3D11RenderTargetView> frameRTV_; // transmitted outward ->

    ID3D11Texture2D* readAvgTexture_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* adaptBuffer_ = nullptr; // always remains only inside the class #

    std::shared_ptr<ID3D11SamplerState> samplerAvg_; // provided externally <-
    std::shared_ptr<ID3D11SamplerState> samplerMin_; // provided externally <-
    std::shared_ptr<ID3D11SamplerState> samplerMax_; // provided externally <-
    std::shared_ptr<ID3D11SamplerState> samplerDefault_; // provided externally <-

    std::shared_ptr<VertexShader> mappingVS_; // provided externally <-
    std::shared_ptr<PixelShader> brightnessPS_; // provided externally <-
    std::shared_ptr<PixelShader> downsamplePS_; // provided externally <-
    std::shared_ptr<PixelShader> tonemapPS_; // provided externally <-

    std::vector<ScaledFrame> scaledFrames_;  // always remains only inside the class #
    std::chrono::time_point<std::chrono::steady_clock> lastFrameTime_;

    int n = 0;
    float adapt = -1.0f;
    float factor = 1.0f;
    float s = 0.5f;
};