#pragma once

#include <windows.h>

#include <string>

namespace util {

bool IsWindowTopMost(HWND hwnd);
int ResizeBorderThicknessForDpi(UINT dpi);
std::wstring FileNameFromPath(const wchar_t* path, const wchar_t* fallback);
HRESULT ApplyDwmFrame(HWND hwnd, bool borderless);
void DisableIme(HWND hwnd);
void TrackMouseLeave(HWND hwnd);
HRESULT InitializeDpiAwareness();

} // namespace util
