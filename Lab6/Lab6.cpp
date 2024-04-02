#include "Lab6.h"
#include "Renderer.h"
#include <windowsx.h>

#define MAX_LOADSTRING 100

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

ATOM                 MyRegisterClass(HINSTANCE hInstance);
BOOL                 InitInstance(HINSTANCE, int);
LRESULT CALLBACK     WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE     hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR        lpCmdLine,
                      _In_ int           nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDI_LAB6, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    std::wstring dir;
    dir.resize(MAX_PATH + 1);
    GetCurrentDirectory(MAX_PATH + 1, &dir[0]);
    size_t configPos = dir.find(L"x64");
    if (configPos != std::wstring::npos) {
        dir.resize(configPos);
        dir += szTitle;
        SetCurrentDirectory(dir.c_str());
    }

    if (!InitInstance(hInstance, nCmdShow)) {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, L"");

    MSG msg;
    Renderer& renderer = Renderer::GetInstance();

    bool exit = false;
    while (!exit) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0) {
            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            if (WM_QUIT == msg.message)
                exit = true;
        }
        if (!renderer.Render())
            return FALSE;
    }

    return (int) msg.wParam;
}


ATOM MyRegisterClass(HINSTANCE hInstance) {
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style           = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc     = WndProc;
    wcex.cbClsExtra      = 0;
    wcex.cbWndExtra      = 0;
    wcex.hInstance       = hInstance;
    wcex.hIcon           = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LAB6));
    wcex.hCursor         = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground   = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName    = nullptr;
    wcex.lpszClassName   = szWindowClass;
    wcex.hIconSm         = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
    hInst = hInstance;

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd) {
       return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    SetForegroundWindow(hWnd);
    SetFocus(hWnd);

    Renderer& renderer = Renderer::GetInstance();
    if (!renderer.Init(hInstance, hWnd)) {
        return FALSE;
    }

    {
        RECT rc;
        rc.left = 0;
        rc.right = Renderer::defaultWidth; // 1280
        rc.top = 0;
        rc.bottom = Renderer::defaultHeight; // 720

        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

        MoveWindow(hWnd, 0, 0, rc.right - rc.left, rc.bottom - rc.top, TRUE);
    }

    UpdateWindow(hWnd);

    return TRUE;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam)) {
        return TRUE;
    }

    switch (message) {
    case WM_SIZE:
        {
            RECT rc;
            GetClientRect(hWnd, &rc);
            Renderer::GetInstance().Resize(rc.right - rc.left, rc.bottom - rc.top);
        }
        break;
    case WM_KEYDOWN:
        {
            int upDown = 0, rightLeft = 0, forwardBack = 0;
            if (GetKeyState('W') & 0x8000 || GetKeyState(VK_UP) & 0x8000) {
                ++forwardBack;
            }
            if (GetKeyState('S') & 0x8000 || GetKeyState(VK_DOWN) & 0x8000) {
                --forwardBack;
            }
            if (GetKeyState('D') & 0x8000 || GetKeyState(VK_RIGHT) & 0x8000) {
                --rightLeft;
            }
            if (GetKeyState('A') & 0x8000 || GetKeyState(VK_LEFT) & 0x8000) {
                ++rightLeft;
            }
            if (GetKeyState('Q') & 0x8000 || GetKeyState(VK_RSHIFT) & 0x8000) {
                ++upDown;
            }
            if (GetKeyState('E') & 0x8000 || GetKeyState(VK_RCONTROL) & 0x8000) {
                --upDown;
            }
            Renderer::GetInstance().MoveCamera(upDown, rightLeft, forwardBack);
        }
        break;
    case WM_MOUSEWHEEL:
        Renderer::GetInstance().OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        break;
    case WM_RBUTTONDOWN:
        Renderer::GetInstance().OnMouseRButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        break;
    case WM_MOUSEMOVE:
        Renderer::GetInstance().OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        break;
    case WM_RBUTTONUP:
        Renderer::GetInstance().OnMouseRButtonUp();
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return TRUE;
}