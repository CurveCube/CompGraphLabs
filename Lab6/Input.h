#pragma once

#include "framework.h"

class Input {
public:
    Input();

    HRESULT Init(HINSTANCE hinstance, HWND hwnd);
    void Release();
    XMFLOAT3 ReadMouse();
    XMINT3 ReadKeyboard();

    ~Input();
private:
    IDirectInput8* directInput_;
    IDirectInputDevice8* mouse_;
    IDirectInputDevice8* keyboard_;

    DIMOUSESTATE mouseState_ = {};
    unsigned char keyboardState_[256];
};