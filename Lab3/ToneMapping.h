#pragma once

#include "framework.h"
#include "SimpleManager.h"
#include <vector>
#include <chrono>

class ToneMapping {
    struct Texture {
        ID3D11Texture2D* texture = nullptr;
        ID3D11RenderTargetView* RTV = nullptr;
        ID3D11ShaderResourceView* SRV = nullptr;
    };

    struct ScaledFrame {
        Texture avg;
        Texture min;
        Texture max;
    };

    struct AdaptBuffer {
        XMFLOAT4 adapt;
    };

public:
    ToneMapping() = default;
    ~ToneMapping();

    HRESULT Init(std::shared_ptr <ID3D11Device> device, std::shared_ptr <ID3D11DeviceContext> deviceContext,
                 SimpleVSManager& VSManager, SimplePSManager& PSManager, SimpleSamplerManager& samplerManager,
                 int textureWidth, int textureHeight);
    void SetRenderTarget();
    void ClearRenderTarget();
    void RenderBrightness();
    void RenderTonemap();
    void ResetEyeAdaptation();
    HRESULT Resize(int textureWidth, int textureHeight);
    void Cleanup();

private:
    HRESULT CreateTextures(int textureWidth, int textureHeight);
    HRESULT CreateScaledFrame(ScaledFrame& scaledFrame, int num);
    HRESULT CreateTexture(Texture& texture, int textureWidth, int textureHeight, DXGI_FORMAT format);
    HRESULT CreateTexture2D(ID3D11Texture2D** texture, int textureWidth, int textureHeight, DXGI_FORMAT format, bool CPUAccess = false);
    void CleanUpTextures();

private:
    std::shared_ptr <ID3D11Device> m_device;
    std::shared_ptr <ID3D11DeviceContext> m_deviceContext;

    Texture m_frame;
    int n = 0;

    ID3D11Texture2D* m_readAvgTexture = nullptr;

    ID3D11Buffer* m_adaptBuffer = nullptr;

    std::shared_ptr <ID3D11SamplerState> m_sampler_avg;
    std::shared_ptr <ID3D11SamplerState> m_sampler_min;
    std::shared_ptr <ID3D11SamplerState> m_sampler_max;

    std::shared_ptr <ID3D11VertexShader> m_mappingVS;
    std::shared_ptr <ID3D11PixelShader> m_brightnessPS;
    std::shared_ptr <ID3D11PixelShader> m_downsamplePS;
    std::shared_ptr <ID3D11PixelShader> m_toneMapPS;

    std::vector<ScaledFrame> m_scaledFrames;
    std::chrono::time_point<std::chrono::steady_clock> m_lastFrame;

    float adapt = -1.0f;
    float s = 0.5f;
};