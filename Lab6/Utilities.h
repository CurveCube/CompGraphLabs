#pragma once


namespace utilities {
    // Функция удаления объекта DX11 для умных указателей.
    template<typename T>
    void DXPtrDeleter(T ptr) {
        if (ptr != nullptr) {
            ptr->Release();
        }
    }

    // Функция удаления объекта DX11 для умных указателей (не удаляет в дебажной сборке - специально для device).
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