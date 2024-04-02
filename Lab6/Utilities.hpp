#pragma once


namespace utilities {
    template<typename T>
    void DXPtrDeleter(T ptr) {
        if (ptr != nullptr) {
            ptr->Release();
        }
    }

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