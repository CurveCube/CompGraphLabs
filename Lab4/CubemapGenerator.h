#pragma once

#include "framework.h"
#include <memory>
#include "SimpleManager.h"
#include <vector>

class CubemapGenerator
{
	static const UINT sideSize = 512;

	struct SimpleVertex {
		XMFLOAT3 pos;
	};

	struct CameraBuffer {
		XMMATRIX viewProjMatrix;
	};

public:
	HRESULT generate(
		std::shared_ptr<ID3D11Device>, std::shared_ptr <ID3D11DeviceContext>,
		SimpleSamplerManager&, SimpleTextureManager&,
		SimpleILManager&, SimplePSManager&, 
		SimpleVSManager&, SimpleGeometryManager&);

	HRESULT createBuffer(std::shared_ptr<ID3D11Device>);

	void Cleanup();

	void getCubemapTexture(std::shared_ptr<SimpleTexture>&);

private:
	HRESULT createHdrMappedTexture(std::shared_ptr<ID3D11Device>, std::shared_ptr <ID3D11DeviceContext>);
	HRESULT createCubemapTexture(std::shared_ptr<ID3D11Device>);
	void renderToCubeMap(std::shared_ptr <ID3D11DeviceContext>, SimplePSManager&, SimpleVSManager&, SimpleSamplerManager&, int);
	HRESULT createGgeometry(SimpleGeometryManager&);
	HRESULT renderSubHdr(std::shared_ptr <ID3D11DeviceContext>, 
		SimplePSManager&, SimpleVSManager&, 
		SimpleSamplerManager&, SimpleTextureManager&, 
		SimpleILManager&, SimpleGeometryManager&, int);

private:
	ID3D11Texture2D* subHdrMappedTexture = NULL;
	ID3D11RenderTargetView* subHdrMappedRTV = NULL;
	ID3D11ShaderResourceView* subHdrMappedSRV = NULL;
	ID3D11Texture2D* cubemapTexture = NULL;
	ID3D11RenderTargetView* cubemapRTV[6];
	ID3D11ShaderResourceView* cubemapSRV = NULL;
	ID3D11Buffer* pCameraBuffer = NULL;
	DirectX::XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(XM_PI / 2, 1.0f, 0.1f, 10.0f);
	std::vector<DirectX::XMMATRIX> viewMatrices;
	std::vector<std::string> sides;
};

