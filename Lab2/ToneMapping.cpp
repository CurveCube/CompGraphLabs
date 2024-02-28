#include "ToneMapping.h"

#define SAFE_RELEASE(A) if ((A) != NULL) { (A)->Release(); (A) = NULL; }

ToneMapping::ToneMapping()
{
	m_frameTexture = nullptr;
	m_frameRTV = nullptr;
	m_frameSRV = nullptr;
	m_sampler_avg = nullptr;
	m_sampler_min = nullptr;
	m_sampler_max = nullptr;
	m_brightnessPS = nullptr;
	m_downsampleVS = nullptr;
	m_downsamplePS = nullptr;
	m_toneMapVS = nullptr;
	m_toneMapPS = nullptr;
}

ToneMapping::~ToneMapping()
{
	CleanUpTexutes();

	SAFE_RELEASE(m_sampler_avg);
	SAFE_RELEASE(m_sampler_min);
	SAFE_RELEASE(m_sampler_max);
	SAFE_RELEASE(m_brightnessPS);
	SAFE_RELEASE(m_downsampleVS);
	SAFE_RELEASE(m_downsamplePS);
	SAFE_RELEASE(m_toneMapVS);
	SAFE_RELEASE(m_toneMapPS);
}

void ToneMapping::CleanUpTexutes()
{
	SAFE_RELEASE(m_frameSRV);
	SAFE_RELEASE(m_frameRTV);
	SAFE_RELEASE(m_frameTexture);

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

	m_scaledFrames.clear();
	n = 0;
}

HRESULT ToneMapping::Init(ID3D11Device* device, int textureWidth, int textureHeight)
{
	HRESULT result = CreateTextures(device, textureWidth, textureHeight);

	if (SUCCEEDED(result))
	{
		D3D11_SAMPLER_DESC desc = {};
		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		
		desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.MinLOD = -FLT_MAX;
		desc.MaxLOD = FLT_MAX;
		desc.MipLODBias = 0.0f;
		desc.MaxAnisotropy = 16;
		desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 1.0f;
		result = device->CreateSamplerState(&desc, &m_sampler_avg);
	}

	if (SUCCEEDED(result))
	{
		D3D11_SAMPLER_DESC desc = {};
		desc.Filter = D3D11_FILTER_MINIMUM_ANISOTROPIC;

		desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.MinLOD = -FLT_MAX;
		desc.MaxLOD = FLT_MAX;
		desc.MipLODBias = 0.0f;
		desc.MaxAnisotropy = 16;
		desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 1.0f;
		result = device->CreateSamplerState(&desc, &m_sampler_min);
	}

	if (SUCCEEDED(result))
	{
		D3D11_SAMPLER_DESC desc = {};
		desc.Filter = D3D11_FILTER_MAXIMUM_ANISOTROPIC;

		desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.MinLOD = -FLT_MAX;
		desc.MaxLOD = FLT_MAX;
		desc.MipLODBias = 0.0f;
		desc.MaxAnisotropy = 16;
		desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 1.0f;
		result = device->CreateSamplerState(&desc, &m_sampler_max);
	}

	ID3D10Blob* vertexShaderBuffer = nullptr;
	ID3D10Blob* pixelShaderBuffer = nullptr;
	int flags = 0;
#ifdef _DEBUG // Включение/отключение отладочной информации для шейдеров в зависимости от конфигурации сборки
	flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	if (SUCCEEDED(result)) {
		result = D3DCompileFromFile(L"brightnessPS.hlsl", NULL, NULL, "main", "ps_5_0", flags, 0, &pixelShaderBuffer, NULL);
		if (SUCCEEDED(result)) {
			result = device->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), NULL, &m_brightnessPS);
		}
	}

	SAFE_RELEASE(pixelShaderBuffer);

	if (SUCCEEDED(result)) {
		result = D3DCompileFromFile(L"downsampleVS.hlsl", NULL, NULL, "main", "vs_5_0", flags, 0, &vertexShaderBuffer, NULL);
		if (SUCCEEDED(result)) {
			result = device->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), NULL, &m_downsampleVS);
		}
	}
	if (SUCCEEDED(result)) {
		result = D3DCompileFromFile(L"downsamplePS.hlsl", NULL, NULL, "main", "ps_5_0", flags, 0, &pixelShaderBuffer, NULL);
		if (SUCCEEDED(result)) {
			result = device->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), NULL, &m_downsamplePS);
		}
	}

	SAFE_RELEASE(vertexShaderBuffer);
	SAFE_RELEASE(pixelShaderBuffer);

	if (SUCCEEDED(result)) {
		result = D3DCompileFromFile(L"toneMapVS.hlsl", NULL, NULL, "main", "vs_5_0", flags, 0, &vertexShaderBuffer, NULL);
		if (SUCCEEDED(result)) {
			result = device->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), NULL, &m_toneMapVS);
		}
	}
	if (SUCCEEDED(result)) {
		result = D3DCompileFromFile(L"toneMapPS.hlsl", NULL, NULL, "main", "ps_5_0", flags, 0, &pixelShaderBuffer, NULL);
		if (SUCCEEDED(result)) {
			result = device->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), NULL, &m_toneMapPS);
		}
	}

	SAFE_RELEASE(vertexShaderBuffer);
	SAFE_RELEASE(pixelShaderBuffer);

	return result;
}

HRESULT ToneMapping::CreateTextures(ID3D11Device* device, int textureWidth, int textureHeight)
{
	D3D11_TEXTURE2D_DESC textureDesc = {};

	textureDesc.Width = textureWidth;
	textureDesc.Height = textureHeight;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.MiscFlags = 0;

	HRESULT result = device->CreateTexture2D(&textureDesc, NULL, &m_frameTexture);
	if (!SUCCEEDED(result))
		return result;

	D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
	renderTargetViewDesc.Format = textureDesc.Format;
	renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	renderTargetViewDesc.Texture2D.MipSlice = 0;

	result = device->CreateRenderTargetView(m_frameTexture, &renderTargetViewDesc, &m_frameRTV);
	if (!SUCCEEDED(result))
		return result;

	D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
	shaderResourceViewDesc.Format = textureDesc.Format;
	shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
	shaderResourceViewDesc.Texture2D.MipLevels = 1;

	result = device->CreateShaderResourceView(m_frameTexture, &shaderResourceViewDesc, &m_frameSRV);

	int minSide = min(textureWidth, textureHeight);

	while (minSide >>= 1) {
		n++;
	}

	for (int i = 0; i < n + 1; i++)
	{
		ScaledFrame scaledFrame;

		result = createTexture(device, scaledFrame.avg, i);
		if (!SUCCEEDED(result))
			break;

		result = createTexture(device, scaledFrame.min, i);
		if (!SUCCEEDED(result))
			break;

		result = createTexture(device, scaledFrame.max, i);
		if (!SUCCEEDED(result))
			break;

		m_scaledFrames.push_back(scaledFrame);
	}
}

HRESULT ToneMapping::createTexture(ID3D11Device* device, Texture& text, int num)
{
	D3D11_TEXTURE2D_DESC desc;
	desc.Format = DXGI_FORMAT_R16_FLOAT;
	desc.ArraySize = 1;
	desc.MipLevels = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Height = pow(2, num);
	desc.Width = pow(2, num);
	HRESULT result = device->CreateTexture2D(&desc, nullptr, &text.texture);
	if (SUCCEEDED(result))
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC descSRV = {};
		descSRV.Format = DXGI_FORMAT_R16_FLOAT;
		descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		descSRV.Texture2D.MipLevels = 1;
		descSRV.Texture2D.MostDetailedMip = 0;
		result = device->CreateShaderResourceView(text.texture, &descSRV, &text.SRV);
	}

	if (SUCCEEDED(result))
	{
		result = device->CreateRenderTargetView(text.texture, nullptr, &text.RTV);
	}

	return result;
}

void ToneMapping::SetRenderTarget(ID3D11DeviceContext* deviceContext)
{
	deviceContext->OMSetRenderTargets(1, &m_frameRTV, NULL);
}

void ToneMapping::ClearRenderTarget(ID3D11DeviceContext* deviceContext)
{
	ID3D11RenderTargetView* views[] = { m_frameRTV };
	deviceContext->OMSetRenderTargets(1, views, NULL);

	float color[4] = { 0.25f, 0.25f, 0.25f, 1.0f };

	deviceContext->ClearRenderTargetView(m_frameRTV, color);

	for (auto& scaledFrame : m_scaledFrames)
	{
		deviceContext->OMSetRenderTargets(1, &scaledFrame.avg.RTV, NULL);
		deviceContext->ClearRenderTargetView(scaledFrame.avg.RTV, color);
	}
}

void ToneMapping::RenderBrightness(ID3D11DeviceContext* deviceContext)
{
	for (int i = n; i >= 0; i--)
	{
		ID3D11RenderTargetView* views[] = { m_scaledFrames[i].avg.RTV, m_scaledFrames[i].min.RTV, m_scaledFrames[i].max.RTV };
		deviceContext->OMSetRenderTargets(3, views, nullptr);
		ID3D11SamplerState* samplers[] = { m_sampler_avg, m_sampler_min, m_sampler_max };
		deviceContext->PSSetSamplers(0, 3, samplers);

		D3D11_VIEWPORT viewport;
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = (FLOAT)pow(2, i);
		viewport.Height = (FLOAT)pow(2, i);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		deviceContext->RSSetViewports(1, &viewport);

		ID3D11ShaderResourceView* resources[] = { i == n ? m_frameSRV : m_scaledFrames[i + 1].avg.SRV,
			i == n ? m_frameSRV : m_scaledFrames[i + 1].min.SRV,
			i == n ? m_frameSRV : m_scaledFrames[i + 1].max.SRV 
		};
		deviceContext->PSSetShaderResources(0, 3, resources);
		deviceContext->OMSetDepthStencilState(nullptr, 0);
		deviceContext->RSSetState(nullptr);
		deviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
		deviceContext->IASetInputLayout(nullptr);
		deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		deviceContext->VSSetShader(m_downsampleVS, nullptr, 0);
		deviceContext->PSSetShader(i == n ? m_brightnessPS : m_downsamplePS, nullptr, 0);
		deviceContext->Draw(6, 0);
	}
}

void ToneMapping::RenderTonemap(ID3D11DeviceContext* deviceContext)
{
	ID3D11ShaderResourceView* resources[] = { m_frameSRV, 
		m_scaledFrames[0].avg.SRV,
			m_scaledFrames[0].min.SRV,
			m_scaledFrames[0].max.SRV
	};
	deviceContext->PSSetShaderResources(0, 4, resources);
	deviceContext->OMSetDepthStencilState(nullptr, 0);
	deviceContext->RSSetState(nullptr);
	deviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
	deviceContext->IASetInputLayout(nullptr);
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	deviceContext->VSSetShader(m_toneMapVS, nullptr, 0);
	deviceContext->PSSetShader(m_toneMapPS, nullptr, 0);
	deviceContext->Draw(6, 0);
}

HRESULT ToneMapping::Resize(ID3D11Device* pDevice_, int width, int height)
{
	CleanUpTexutes();
	return CreateTextures(pDevice_, width, height);
}
