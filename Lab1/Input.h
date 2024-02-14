#pragma once

#include "framework.h"

class Input {
public:
    Input();

    HRESULT Init(HINSTANCE hinstance, HWND hwnd);
    void Realese();
    XMFLOAT3 ReadInput();

    ~Input();
private:
    IDirectInput8* directInput_;
    IDirectInputDevice8* mouse_;

    DIMOUSESTATE mouseState_ = {};
};