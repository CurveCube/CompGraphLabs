#pragma once

#include "ShaderManagers.hpp"
#include "TextureManager.h"
#include "StateManager.hpp"


class ManagerStorage {
public:
    ManagerStorage() = default;

    HRESULT Init(const std::shared_ptr<Device>& device) {
        if (!device->IsInit()) {
            return E_FAIL;
        }
        VSManager_ = std::make_shared<VSManager>(device);
        PSManager_ = std::make_shared<PSManager>(device);
        textureManager_ = std::make_shared<TextureManager>(device);
        stateManager_ = std::make_shared<StateManager>(device);
        return S_OK;
    };

    bool IsInit() const {
        return !!VSManager_;
    };

    std::shared_ptr<VSManager> GetVSManager() const {
        return VSManager_;
    };

    std::shared_ptr<PSManager> GetPSManager() const {
        return PSManager_;
    };

    std::shared_ptr<TextureManager> GetTextureManager() const {
        return textureManager_;
    };

    std::shared_ptr<StateManager> GetStateManager() const {
        return stateManager_;
    };

    void Cleanup() {
        VSManager_->Cleanup();
        PSManager_->Cleanup();
        textureManager_->Cleanup();
        stateManager_->Cleanup();
    };

    ~ManagerStorage() = default;

private:
    std::shared_ptr<VSManager> VSManager_; // transmitted outward ->
    std::shared_ptr<PSManager> PSManager_; // transmitted outward ->
    std::shared_ptr<TextureManager> textureManager_; // transmitted outward ->
    std::shared_ptr<StateManager> stateManager_; // transmitted outward ->
};