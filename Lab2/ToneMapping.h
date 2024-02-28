#pragma once

#include "framework.h"

#include <vector>
#include <chrono>

class ToneMapping
{
	struct Texture
	{
		ID3D11Texture2D* texture = nullptr;
		ID3D11RenderTargetView* RTV = nullptr;
		ID3D11ShaderResourceView* SRV = nullptr;
	};

	struct ScaledFrame
	{
		Texture avg;
		Texture min;
		Texture max;
	};

	struct AdaptBuffer {
		XMFLOAT4 adapt;
	};

public:
	ToneMapping();
	~ToneMapping();

	HRESULT Init(ID3D11Device*, int, int);
	void SetRenderTarget(ID3D11DeviceContext*);
	void ClearRenderTarget(ID3D11DeviceContext*);
	void RenderBrightness(ID3D11DeviceContext*);
	void RenderTonemap(ID3D11DeviceContext*);
	HRESULT Resize(ID3D11Device*, int, int);
	HRESULT createTexture(ID3D11Device*, Texture&, int);

private:
	HRESULT CreateTextures(ID3D11Device*, int, int);
	void CleanUpTexutes();

private:
	ID3D11Texture2D* m_frameTexture;
	ID3D11RenderTargetView* m_frameRTV;
	ID3D11ShaderResourceView* m_frameSRV;
	int n = 0;

	ID3D11Texture2D* m_readAvgTexture;

	ID3D11Buffer* m_adaptBuffer;

	ID3D11SamplerState* m_sampler_avg;
	ID3D11SamplerState* m_sampler_min;
	ID3D11SamplerState* m_sampler_max;

	ID3D11VertexShader* m_mappingVS;
	ID3D11PixelShader* m_brightnessPS;
	ID3D11PixelShader* m_downsamplePS;
	ID3D11PixelShader* m_toneMapPS;

	std::vector<ScaledFrame> m_scaledFrames;
	std::chrono::time_point<std::chrono::steady_clock> m_lastFrame;

	float adapt = 0.0f;
	float s = 0.01f;
};