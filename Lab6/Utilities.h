#pragma once


namespace utilities {
    // ������� �������� ������� DX11 ��� ����� ����������.
    template<typename T>
    void DXPtrDeleter(T ptr) {
        if (ptr != nullptr) {
            ptr->Release();
        }
    }

    // ������� �������� ������� DX11 ��� ����� ���������� (�� ������� � �������� ������ - ���������� ��� device).
    template<typename T>
    void DXRelPtrDeleter(T ptr) {
        if (ptr != nullptr) {
#ifndef _DEBUG
            ptr->Release();
#else
            ;
#endif
        }
    }
};