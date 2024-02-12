#include "input.h"

Input::Input():
    directInput_(NULL),
    mouse_(NULL) {}

HRESULT Input::Init(HINSTANCE hinstance, HWND hwnd) {
    HRESULT result;

    result = DirectInput8Create(hinstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput_, NULL);

    if (SUCCEEDED(result)) {
        result = directInput_->CreateDevice(GUID_SysMouse, &mouse_, NULL);
    }

    if (SUCCEEDED(result)) {
        result = mouse_->SetDataFormat(&c_dfDIMouse);
    }

    if (SUCCEEDED(result)) {
        result = mouse_->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
    }

    if (SUCCEEDED(result)) {
        result = mouse_->Acquire();
    }

    return result;
}

void Input::Realese() {
    if (mouse_) {
        mouse_->Unacquire();
        mouse_->Release();
        mouse_ = NULL;
    }

    if (directInput_) {
        directInput_->Release();
        directInput_ = NULL;
    }
}

XMFLOAT3 Input::ReadInput() {
    HRESULT result;

    result = mouse_->GetDeviceState(sizeof(DIMOUSESTATE), (LPVOID)&mouseState_);
    if (FAILED(result)) {
        if ((result == DIERR_INPUTLOST) || (result == DIERR_NOTACQUIRED))
            mouse_->Acquire();
    }
    else {
        if (mouseState_.rgbButtons[0] || mouseState_.rgbButtons[1] || mouseState_.rgbButtons[2] & 0x80)
            return XMFLOAT3((float)mouseState_.lX, (float)mouseState_.lY, (float)mouseState_.lZ);
    }

    return XMFLOAT3(0.0f, 0.0f, 0.0f);
}

Input::~Input() {
    Realese();
}