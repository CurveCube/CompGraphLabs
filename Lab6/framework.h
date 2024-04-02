﻿// header.h: включаемый файл для стандартных системных включаемых файлов
// или включаемые файлы для конкретного проекта
//

#pragma once

#include "targetver.h"
#define WIN32_LEAN_AND_MEAN             // Исключите редко используемые компоненты из заголовков Windows
// Файлы заголовков Windows
#include <windows.h>
// Файлы заголовков среды выполнения C
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#define SAFE_RELEASE(A) if ((A) != NULL) { (A)->Release(); (A) = NULL; }
#define MAX_LIGHT 3

#include <d3dcompiler.h>
#include <dxgi.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <directxmath.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"

using namespace DirectX;