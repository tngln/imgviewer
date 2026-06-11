#pragma once

#include <windows.h>

#include <d2d1_1.h>

#include <string>

std::wstring ReadImeCompositionString(HWND hwnd, LPARAM lparam, DWORD string_type);
void SetImeCompositionWindowClientPoint(HWND hwnd, POINT point);
void SetImeCompositionWindowClientPoint(HWND hwnd, D2D1_POINT_2F point);
