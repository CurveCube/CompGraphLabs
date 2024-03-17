#include "D3DInclude.h"

HRESULT D3DInclude::Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes) {
    FILE* pFile = nullptr;
    fopen_s(&pFile, pFileName, "rb");
    if (pFile == nullptr) {
        return E_FAIL;
    }

    fseek(pFile, 0, SEEK_END);
    long size = ftell(pFile);
    fseek(pFile, 0, SEEK_SET);

    char* buffer = new char[size];
    fread(buffer, 1, size, pFile);
    fclose(pFile);

    *ppData = buffer;
    *pBytes = size;

    return S_OK;
}

HRESULT D3DInclude::Close(LPCVOID pData) {
    free(const_cast<void*>(pData));
    return S_OK;
}
