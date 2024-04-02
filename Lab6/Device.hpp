#pragma once

#include "framework.h"
#include "Utilities.hpp"
#include <memory>


class Device {
public:
    Device() = default;

    HRESULT Init(const std::shared_ptr<IDXGIAdapter>& adapter) {
        D3D_FEATURE_LEVEL level;
        ID3D11Device* device;
        ID3D11DeviceContext* deviceContext;
        D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
        UINT flags = 0;
#ifdef _DEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif // _DEBUG
        HRESULT result = D3D11CreateDevice(
            adapter.get(),
            D3D_DRIVER_TYPE_UNKNOWN,
            NULL,
            flags,
            levels,
            1,
            D3D11_SDK_VERSION,
            &device,
            &level,
            &deviceContext
        );
        if (D3D_FEATURE_LEVEL_11_0 != level || FAILED(result)) {
            SAFE_RELEASE(device);
            SAFE_RELEASE(deviceContext);
            return E_FAIL;
        }
        device_ = std::shared_ptr<ID3D11Device>(device, utilities::DXRelPtrDeleter<ID3D11Device*>);
        deviceContext_ = std::shared_ptr<ID3D11DeviceContext>(deviceContext, utilities::DXPtrDeleter<ID3D11DeviceContext*>);
        return S_OK;
    };

    bool IsInit() const {
        return !!device_;
    };

    std::shared_ptr<ID3D11Device> GetDevice() const {
        return device_;
    };

    std::shared_ptr<ID3D11DeviceContext> GetDeviceContext() const {
        return deviceContext_;
    };

    void Cleanup() {
        if (deviceContext_) {
            deviceContext_->ClearState();
            deviceContext_.reset();
        }
        if (device_) {
#ifdef _DEBUG
            ID3D11Debug* d3dDebug = nullptr;
            device_->QueryInterface(IID_PPV_ARGS(&d3dDebug));
            UINT references = device_->Release();
            device_.reset();
            if (references > 1) {
                d3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
            }
            SAFE_RELEASE(d3dDebug);
#else
            device_.reset();
#endif
        }
    };

    ~Device() {
        Cleanup();
    };

private:
    std::shared_ptr<ID3D11Device> device_; // transmitted outward ->
    std::shared_ptr<ID3D11DeviceContext> deviceContext_; // transmitted outward ->
};