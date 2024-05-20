#pragma once

#include "Device.hpp"
#include <map>
#include <vector>
#include <string>


namespace {
    std::string GenerateKey(D3D11_BLEND srcBlend, D3D11_BLEND destBlend, D3D11_BLEND_OP op) {
        return std::to_string(srcBlend) + "_" + std::to_string(destBlend) + "_" + std::to_string(op);
    };

    std::string GenerateKey(D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE modeU,
        D3D11_TEXTURE_ADDRESS_MODE modeV, D3D11_TEXTURE_ADDRESS_MODE modeW, D3D11_COMPARISON_FUNC compFunc) {
        return std::to_string(filter) + "_" + std::to_string(modeU) + "_" + std::to_string(modeV) + "_" + std::to_string(modeW) + "_" + std::to_string(compFunc);
    };

    std::string GenerateKey(D3D11_COMPARISON_FUNC compFunc, D3D11_DEPTH_WRITE_MASK mask) {
        return std::to_string(compFunc) + "_" + std::to_string(mask);
    };

    std::string GenerateKey(D3D11_FILL_MODE fillMode, D3D11_CULL_MODE cullMode, INT depthBias, FLOAT slopeScaleDepthBias) {
        return std::to_string(fillMode) + "_" + std::to_string(cullMode) + "_" + std::to_string(depthBias) + "_"
            + std::to_string(slopeScaleDepthBias);
    };
}; // anonymous namespace


class StateManager {
public:
    StateManager(const std::shared_ptr<Device>& devicePtr) : device_(devicePtr) {};

    bool CheckBlendState(D3D11_BLEND srcBlend = D3D11_BLEND_SRC_ALPHA, D3D11_BLEND destBlend = D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP op = D3D11_BLEND_OP_ADD) const {
        return blendStates_.find(GenerateKey(srcBlend, destBlend, op)) != blendStates_.end();
    };

    HRESULT CreateBlendState(std::shared_ptr<ID3D11BlendState>& object, D3D11_BLEND srcBlend = D3D11_BLEND_SRC_ALPHA, D3D11_BLEND destBlend = D3D11_BLEND_INV_SRC_ALPHA,
        D3D11_BLEND_OP op = D3D11_BLEND_OP_ADD) {
        if (SUCCEEDED(GetBlendState(object, srcBlend, destBlend, op))) {
            return S_OK;
        }

        D3D11_BLEND_DESC desc = { };
        desc.AlphaToCoverageEnable = false;
        desc.IndependentBlendEnable = false;
        desc.RenderTarget[0].BlendEnable = true;
        desc.RenderTarget[0].BlendOp = op;
        desc.RenderTarget[0].DestBlend = destBlend;
        desc.RenderTarget[0].SrcBlend = srcBlend;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED |
            D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;

        ID3D11BlendState* blendState;
        HRESULT result = device_->GetDevice()->CreateBlendState(&desc, &blendState);
        if (SUCCEEDED(result)) {
            object = std::shared_ptr<ID3D11BlendState>(blendState, utilities::DXPtrDeleter<ID3D11BlendState*>);
            blendStates_.emplace(GenerateKey(srcBlend, destBlend, op), object);
        }
        return result;
    };

    HRESULT CreateBlendState(D3D11_BLEND srcBlend = D3D11_BLEND_SRC_ALPHA, D3D11_BLEND destBlend = D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP op = D3D11_BLEND_OP_ADD) {
        std::shared_ptr<ID3D11BlendState> object;
        return CreateBlendState(object, srcBlend, destBlend, op);
    };

    HRESULT GetBlendState(std::shared_ptr<ID3D11BlendState>& object, D3D11_BLEND srcBlend = D3D11_BLEND_SRC_ALPHA, D3D11_BLEND destBlend = D3D11_BLEND_INV_SRC_ALPHA,
        D3D11_BLEND_OP op = D3D11_BLEND_OP_ADD) const {
        auto result = blendStates_.find(GenerateKey(srcBlend, destBlend, op));
        if (result == blendStates_.end()) {
            return E_FAIL;
        }
        else {
            object = result->second;
            return S_OK;
        }
    };

    void ClearBlendState() {
        blendStates_.clear();
    };

    bool CheckSamplerState(D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE modeU = D3D11_TEXTURE_ADDRESS_WRAP,
        D3D11_TEXTURE_ADDRESS_MODE modeV = D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_MODE modeW = D3D11_TEXTURE_ADDRESS_WRAP,
        D3D11_COMPARISON_FUNC compFunc = D3D11_COMPARISON_NEVER) const {
        return samplerStates_.find(GenerateKey(filter, modeU, modeV, modeW, compFunc)) != samplerStates_.end();
    };

    HRESULT CreateSamplerState(std::shared_ptr<ID3D11SamplerState>& object, D3D11_FILTER filter,
        D3D11_TEXTURE_ADDRESS_MODE modeU = D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_MODE modeV = D3D11_TEXTURE_ADDRESS_WRAP,
        D3D11_TEXTURE_ADDRESS_MODE modeW = D3D11_TEXTURE_ADDRESS_WRAP, D3D11_COMPARISON_FUNC compFunc = D3D11_COMPARISON_NEVER) {
        if (SUCCEEDED(GetSamplerState(object, filter, modeU, modeV, modeW, compFunc))) {
            return S_OK;
        }

        D3D11_SAMPLER_DESC desc = {};
        desc.Filter = filter;
        desc.AddressU = modeU;
        desc.AddressV = modeV;
        desc.AddressW = modeW;
        desc.MinLOD = -FLT_MAX;
        desc.MaxLOD = FLT_MAX;
        desc.MipLODBias = 0.0f;
        desc.MaxAnisotropy = 16;
        desc.ComparisonFunc = compFunc;
        desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 1.0f;

        ID3D11SamplerState* sampler;
        HRESULT result = device_->GetDevice()->CreateSamplerState(&desc, &sampler);
        if (SUCCEEDED(result)) {
            object = std::shared_ptr<ID3D11SamplerState>(sampler, utilities::DXPtrDeleter<ID3D11SamplerState*>);
            samplerStates_.emplace(GenerateKey(filter, modeU, modeV, modeW, compFunc), object);
        }
        return result;
    };

    HRESULT CreateSamplerState(D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE modeU = D3D11_TEXTURE_ADDRESS_WRAP,
        D3D11_TEXTURE_ADDRESS_MODE modeV = D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_MODE modeW = D3D11_TEXTURE_ADDRESS_WRAP,
        D3D11_COMPARISON_FUNC compFunc = D3D11_COMPARISON_NEVER) {
        std::shared_ptr<ID3D11SamplerState> object;
        return CreateSamplerState(object, filter, modeU, modeV, modeW, compFunc);
    };

    HRESULT GetSamplerState(std::shared_ptr<ID3D11SamplerState>& object, D3D11_FILTER filter,
        D3D11_TEXTURE_ADDRESS_MODE modeU = D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_MODE modeV = D3D11_TEXTURE_ADDRESS_WRAP,
        D3D11_TEXTURE_ADDRESS_MODE modeW = D3D11_TEXTURE_ADDRESS_WRAP, D3D11_COMPARISON_FUNC compFunc = D3D11_COMPARISON_NEVER) const {
        auto result = samplerStates_.find(GenerateKey(filter, modeU, modeV, modeW, compFunc));
        if (result == samplerStates_.end()) {
            return E_FAIL;
        }
        else {
            object = result->second;
            return S_OK;
        }
    };

    void ClearSamplerStates() {
        samplerStates_.clear();
    };

    bool CheckDepthStencilState(D3D11_COMPARISON_FUNC compFunc = D3D11_COMPARISON_GREATER,
        D3D11_DEPTH_WRITE_MASK mask = D3D11_DEPTH_WRITE_MASK_ALL) const {
        return depthStencilStates_.find(GenerateKey(compFunc, mask)) != depthStencilStates_.end();
    };

    HRESULT CreateDepthStencilState(std::shared_ptr<ID3D11DepthStencilState>& object, D3D11_COMPARISON_FUNC compFunc = D3D11_COMPARISON_GREATER,
        D3D11_DEPTH_WRITE_MASK mask = D3D11_DEPTH_WRITE_MASK_ALL) {
        if (SUCCEEDED(GetDepthStencilState(object, compFunc, mask))) {
            return S_OK;
        }

        D3D11_DEPTH_STENCIL_DESC desc = { };
        desc.DepthEnable = TRUE;
        desc.DepthWriteMask = mask;
        desc.DepthFunc = compFunc;
        desc.StencilEnable = FALSE;

        ID3D11DepthStencilState* DSState;
        HRESULT result = device_->GetDevice()->CreateDepthStencilState(&desc, &DSState);
        if (SUCCEEDED(result)) {
            object = std::shared_ptr<ID3D11DepthStencilState>(DSState, utilities::DXPtrDeleter<ID3D11DepthStencilState*>);
            depthStencilStates_.emplace(GenerateKey(compFunc, mask), object);
        }
        return result;
    };

    HRESULT CreateDepthStencilState(D3D11_COMPARISON_FUNC compFunc = D3D11_COMPARISON_GREATER,
        D3D11_DEPTH_WRITE_MASK mask = D3D11_DEPTH_WRITE_MASK_ALL) {
        std::shared_ptr<ID3D11DepthStencilState> object;
        return CreateDepthStencilState(object, compFunc, mask);
    };

    HRESULT GetDepthStencilState(std::shared_ptr<ID3D11DepthStencilState>& object, D3D11_COMPARISON_FUNC compFunc = D3D11_COMPARISON_GREATER,
        D3D11_DEPTH_WRITE_MASK mask = D3D11_DEPTH_WRITE_MASK_ALL) const {
        auto result = depthStencilStates_.find(GenerateKey(compFunc, mask));
        if (result == depthStencilStates_.end()) {
            return E_FAIL;
        }
        else {
            object = result->second;
            return S_OK;
        }
    };

    void ClearDepthStencilStates() {
        depthStencilStates_.clear();
    };

    bool CheckRasterizerState(D3D11_FILL_MODE fillMode = D3D11_FILL_SOLID, D3D11_CULL_MODE cullMode = D3D11_CULL_NONE,
        INT depthBias = 0, FLOAT slopeScaleDepthBias = 0.0f) const {
        return rasterizerStates_.find(GenerateKey(fillMode, cullMode, depthBias, slopeScaleDepthBias)) != rasterizerStates_.end();
    };

    HRESULT CreateRasterizerState(std::shared_ptr<ID3D11RasterizerState>& object, D3D11_FILL_MODE fillMode = D3D11_FILL_SOLID,
        D3D11_CULL_MODE cullMode = D3D11_CULL_NONE, INT depthBias = 0, FLOAT slopeScaleDepthBias = 0.0f) {
        if (SUCCEEDED(GetRasterizerState(object, fillMode, cullMode, depthBias, slopeScaleDepthBias))) {
            return S_OK;
        }

        D3D11_RASTERIZER_DESC desc = {};
        desc.AntialiasedLineEnable = false;
        desc.FillMode = fillMode;
        desc.CullMode = cullMode;
        desc.DepthBias = depthBias;
        desc.DepthBiasClamp = 0.0f;
        desc.FrontCounterClockwise = false;
        desc.DepthClipEnable = true;
        desc.ScissorEnable = false;
        desc.MultisampleEnable = false;
        desc.SlopeScaledDepthBias = slopeScaleDepthBias;

        ID3D11RasterizerState* rasterizerState;
        HRESULT result = device_->GetDevice()->CreateRasterizerState(&desc, &rasterizerState);
        if (SUCCEEDED(result)) {
            object = std::shared_ptr<ID3D11RasterizerState>(rasterizerState, utilities::DXPtrDeleter<ID3D11RasterizerState*>);
            rasterizerStates_.emplace(GenerateKey(fillMode, cullMode, depthBias, slopeScaleDepthBias), object);
        }
        return result;
    };

    HRESULT CreateRasterizerState(D3D11_FILL_MODE fillMode = D3D11_FILL_SOLID, D3D11_CULL_MODE cullMode = D3D11_CULL_NONE,
        INT depthBias = 0, FLOAT slopeScaleDepthBias = 0.0f) {
        std::shared_ptr<ID3D11RasterizerState> object;
        return CreateRasterizerState(object, fillMode, cullMode, depthBias, slopeScaleDepthBias);
    };

    HRESULT GetRasterizerState(std::shared_ptr<ID3D11RasterizerState>& object, D3D11_FILL_MODE fillMode = D3D11_FILL_SOLID,
        D3D11_CULL_MODE cullMode = D3D11_CULL_NONE, INT depthBias = 0, FLOAT slopeScaleDepthBias = 0.0f) const {
        auto result = rasterizerStates_.find(GenerateKey(fillMode, cullMode, depthBias, slopeScaleDepthBias));
        if (result == rasterizerStates_.end()) {
            return E_FAIL;
        }
        else {
            object = result->second;
            return S_OK;
        }
    };

    void ClearRasterizerStates() {
        rasterizerStates_.clear();
    };

    void Cleanup() {
        device_.reset();
        ClearBlendState();
        ClearSamplerStates();
        ClearDepthStencilStates();
        ClearRasterizerStates();
    };

    ~StateManager() = default;

private:
    std::shared_ptr<Device> device_; // provided externally <-
    std::map<std::string, std::shared_ptr<ID3D11BlendState>> blendStates_; // states are transmitted outward ->
    std::map<std::string, std::shared_ptr<ID3D11SamplerState>> samplerStates_; // states are transmitted outward ->
    std::map<std::string, std::shared_ptr<ID3D11DepthStencilState>> depthStencilStates_; // states are transmitted outward ->
    std::map<std::string, std::shared_ptr<ID3D11RasterizerState>> rasterizerStates_; // states are transmitted outward ->
};