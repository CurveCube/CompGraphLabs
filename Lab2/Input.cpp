#include "input.h"

Input::Input() :
    directInput_(NULL),
    mouse_(NULL),
    keyboard_(NULL) {}

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

    if (SUCCEEDED(result)) {
        result = directInput_->CreateDevice(GUID_SysKeyboard, &keyboard_, NULL);
    }

    if (SUCCEEDED(result)) {
        result = keyboard_->SetDataFormat(&c_dfDIKeyboard);
    }

    if (SUCCEEDED(result)) {
        result = keyboard_->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
    }

    if (SUCCEEDED(result)) {
        result = keyboard_->Acquire();
    }

    return result;
}

void Input::Realese() {
    if (mouse_) {
        mouse_->Unacquire();
        mouse_->Release();
        mouse_ = NULL;
    }

    if (keyboard_) {
        keyboard_->Unacquire();
        keyboard_->Release();
        keyboard_ = NULL;
    }

    if (directInput_) {
        directInput_->Release();
        directInput_ = NULL;
    }
}

XMFLOAT3 Input::ReadMouse() {
    HRESULT result;

    result = mouse_->GetDeviceState(sizeof(DIMOUSESTATE), (LPVOID)&mouseState_);
    if (FAILED(result)) {
        if ((result == DIERR_INPUTLOST) || (result == DIERR_NOTACQUIRED))
            mouse_->Acquire();
    }
    else {
        if (mouseState_.rgbButtons[1] || mouseState_.lZ != 0)
            return XMFLOAT3((float)mouseState_.lX, (float)mouseState_.lY, (float)mouseState_.lZ);
    }

    return XMFLOAT3(0.0f, 0.0f, 0.0f);
}

XMINT3 Input::ReadKeyboard() {
    HRESULT result;
    XMINT3 keys = XMINT3(0, 0, 0);

    result = keyboard_->GetDeviceState(sizeof(keyboardState_), (LPVOID)&keyboardState_);
    if (FAILED(result)) {
        if ((result == DIERR_INPUTLOST) || (result == DIERR_NOTACQUIRED))
            keyboard_->Acquire();
    }
    else {
        if (keyboardState_[DIK_UP] || keyboardState_[DIK_W])
            keys.x = 1.0f;
        if (keyboardState_[DIK_DOWN] || keyboardState_[DIK_S])
            keys.x = -1.0f;
        if (keyboardState_[DIK_RIGHT] || keyboardState_[DIK_D])
            keys.z = -1.0f;
        if (keyboardState_[DIK_LEFT] || keyboardState_[DIK_A])
            keys.z = 1.0f;
        if (keyboardState_[DIK_RSHIFT] || keyboardState_[DIK_Q])
            keys.y = 1.0f;
        if (keyboardState_[DIK_RCONTROL] || keyboardState_[DIK_E])
            keys.y = -1.0f;
    }

    return keys;
}

Input::~Input() {
    Realese();
}