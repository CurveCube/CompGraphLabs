#pragma once

#include "Device.hpp"

class SwapChain {
public:
    SwapChain() = default;

    HRESULT Init(const std::shared_ptr<IDXGIFactory>& factory, const std::shared_ptr<Device>& device, HWND hWnd, UINT width, UINT height) {
        if (!device->IsInit()) {
            return E_FAIL;
        }

        device_ = device;

        DXGI_SWAP_CHAIN_DESC swapChainDesc = { };
        swapChainDesc.BufferCount = 2;
        swapChainDesc.BufferDesc.Width = width;
        swapChainDesc.BufferDesc.Height = height;
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
        swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
        swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.OutputWindow = hWnd;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.Windowed = true;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.Flags = 0;

        HRESULT result = factory->CreateSwapChain(device_->GetDevice().get(), &swapChainDesc, &swapChain_);

        ID3D11Texture2D* backBuffer = nullptr;
        if (SUCCEEDED(result)) {
            result = swapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
        }

        ID3D11RenderTargetView* renderTargetView = nullptr;
        if (SUCCEEDED(result)) {
            D3D11_RENDER_TARGET_VIEW_DESC desc = {};
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            desc.Texture2D.MipSlice = 0;

            result = device_->GetDevice()->CreateRenderTargetView(backBuffer, &desc, &renderTargetView);
        }
        SAFE_RELEASE(backBuffer);

        if (SUCCEEDED(result)) {
            renderTarget_ = std::shared_ptr<ID3D11RenderTargetView>(renderTargetView, utilities::DXPtrDeleter<ID3D11RenderTargetView*>);
        }

        return result;
    };

    bool IsInit() const {
        return !!swapChain_;
    };

    std::shared_ptr<ID3D11RenderTargetView> GetRenderTarget() const {
        return renderTarget_;
    };

    bool Resize(UINT width, UINT height) {
        if (!IsInit()) {
            return false;
        }

        renderTarget_.reset();

        HRESULT result = swapChain_->ResizeBuffers(2, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
        if (FAILED(result)) {
            return false;
        }

        ID3D11Texture2D* backBuffer;
        result = swapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
        if (FAILED(result)) {
            return false;
        }

        D3D11_RENDER_TARGET_VIEW_DESC desc = {};
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipSlice = 0;

        ID3D11RenderTargetView* renderTargetView;
        result = device_->GetDevice()->CreateRenderTargetView(backBuffer, &desc, &renderTargetView);
        SAFE_RELEASE(backBuffer);
        if (FAILED(result)) {
            return false;
        }
        renderTarget_ = std::shared_ptr<ID3D11RenderTargetView>(renderTargetView, utilities::DXPtrDeleter<ID3D11RenderTargetView*>);

        return true;
    };

    HRESULT Present() const {
        if (IsInit()) {
            return swapChain_->Present(0, 0);
        }
        else {
            return E_FAIL;
        }
    };

    void Cleanup() {
        SAFE_RELEASE(swapChain_);
        device_.reset();
        renderTarget_.reset();
    };

    ~SwapChain() {
        Cleanup();
    };

private:
    std::shared_ptr<Device> device_; // provided externally <-
    std::shared_ptr<ID3D11RenderTargetView> renderTarget_; // transmitted outward ->

    IDXGISwapChain* swapChain_ = nullptr; // always remains only inside the class #
};