#pragma once

#include <windows.h>

#include <d2d1_1.h>

#include <string>

namespace util {

RECT UiElementRectToWin32Rect(D2D1_RECT_F rect);
bool IsWindowTopMost(HWND hwnd);
int ResizeBorderThicknessForDpi(UINT dpi);
HRESULT ApplyDwmFrame(HWND hwnd);
void DisableIme(HWND hwnd);
std::wstring FileNameFromPath(const wchar_t* path, const wchar_t* fallback);
void TrackMouseLeave(HWND hwnd);
HRESULT InitializeDpiAwareness();

} // namespace util
