#pragma once
#include "framework.h"
#include <memory>
#include "Camera.h"
#include "ToneMapping.h"
#include <vector>

class CubeMap
{
public:
    CubeMap()
    {

    }

    HRESULT init(ID3D11Device* device, ID3D11DeviceContext* m_deviceContext, SimpleSamplerManager& samplerManager, SimpleTextureManager& textureManager,
        SimpleILManager& ILManager, SimplePSManager& PSManager, SimpleVSManager& VSManager, SimpleGeometryManager& GManager)
    {
        createRenderTarget(device, 512, 512);
        Camera camera;
        const auto& viewMatrix = camera.GetViewMatrix();
        const auto projectionMatrix = XMMatrixPerspectiveFovLH(XM_PI / 2, 1.0f, 0.4f, 0.6f);

        ID3D11RenderTargetView* views = {
            renderTarget.RTV
        };
        m_deviceContext->OMSetRenderTargets(1, &views, nullptr);

        samplerManager.get("default", SS);
        ID3D11SamplerState* samplers[] = {
            SS.get()
        };
        m_deviceContext->PSSetSamplers(0, 3, samplers);

        D3D11_VIEWPORT viewport;
        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;
        viewport.Width = (FLOAT)512;
        viewport.Height = (FLOAT)512;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        m_deviceContext->RSSetViewports(1, &viewport);

        std::shared_ptr<SimpleTexture> simpleTexture;
        textureManager.get("hdr", simpleTexture);
        ID3D11ShaderResourceView* resources[] = {
            simpleTexture->getSRV()
        };

        SimpleVertex geometry[] = { XMFLOAT3(0.5f, -0.5f, 0.5f),
                                    XMFLOAT3(0.5f, -0.5f, -0.5f),
                                    XMFLOAT3(0.5f, 0.5f, -0.5f),
                                    XMFLOAT3(0.5f, 0.5f, 0.5f) };

        UINT indices[] = { 0, 1, 2, 2, 1, 3 };

        GManager.loadGeometry(geometry, sizeof(XMFLOAT3) * 4, indices, sizeof(UINT) * 6, "X+");

        m_deviceContext->PSSetShaderResources(0, 1, resources);
        m_deviceContext->OMSetDepthStencilState(nullptr, 0);
        m_deviceContext->RSSetState(nullptr);
        m_deviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
       
        m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        VSManager.get("environmentMapVS", VS);
        PSManager.get("environmentMapPS", PS);
        ILManager.get("environmentMapVS", IL);



        m_deviceContext->VSSetShader(VS.get(), nullptr, 0);
        m_deviceContext->PSSetShader(PS.get(), nullptr, 0);
        m_deviceContext->IASetInputLayout(IL.get());
        m_deviceContext->Draw(6, 0);
    }

    HRESULT createRenderTarget(ID3D11Device* device, int textureWidth, int textureHeight)
    {
        HRESULT result;
        {
            D3D11_TEXTURE2D_DESC textureDesc = {};

            textureDesc.Width = textureWidth;
            textureDesc.Height = textureHeight;
            textureDesc.MipLevels = 1;
            textureDesc.ArraySize = 1;
            textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            textureDesc.SampleDesc.Count = 1;
            textureDesc.Usage = D3D11_USAGE_DEFAULT;
            textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            textureDesc.CPUAccessFlags = 0;
            textureDesc.MiscFlags = 0;

            result = device->CreateTexture2D(&textureDesc, NULL, &renderTarget.texture);
        }

        if (SUCCEEDED(result))
        {
            D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
            renderTargetViewDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            renderTargetViewDesc.Texture2D.MipSlice = 0;

            result = device->CreateRenderTargetView(renderTarget.texture, &renderTargetViewDesc, &renderTarget.RTV);
        }

        if (SUCCEEDED(result))
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
            shaderResourceViewDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
            shaderResourceViewDesc.Texture2D.MipLevels = 1;

            result = device->CreateShaderResourceView(renderTarget.texture, &shaderResourceViewDesc, &renderTarget.SRV);
        }
    }

    /*HRESULT CreateTexture2D(ID3D11Texture2D** texture, int textureWidth, int textureHeight, DXGI_FORMAT format, bool CPUAccess) {
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
    }*/

    HRESULT CreateCubemapTexture(ID3D11Device* device, ID3D11Texture2D** texture, int textureWidth, int textureHeight, DXGI_FORMAT format) {
        D3D11_TEXTURE2D_DESC textureDesc = {};

        textureDesc.Width = textureWidth;
        textureDesc.Height = textureHeight;
        textureDesc.MipLevels = 1;
        textureDesc.ArraySize = 6;
        textureDesc.Format = format;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
        textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        textureDesc.CPUAccessFlags = 0;
        textureDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

        return device->CreateTexture2D(&textureDesc, nullptr, texture);
    }

    struct Texture {
        ID3D11Texture2D* texture = nullptr;
        ID3D11RenderTargetView* RTV = nullptr;
        ID3D11ShaderResourceView* SRV = nullptr;
    };

private:
    Texture renderTarget;
    std::shared_ptr<ID3D11VertexShader> VS;
    std::shared_ptr<ID3D11InputLayout> IL;
    std::shared_ptr<ID3D11PixelShader> PS;
    std::shared_ptr<ID3D11SamplerState> SS;

    struct SimpleVertex {
        XMFLOAT3 pos;
    };
};