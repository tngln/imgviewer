#pragma once

#include <windows.h>

namespace util {

bool IsWindowTopMost(HWND hwnd);
int ResizeBorderThicknessForDpi(UINT dpi);
HRESULT ApplyDwmFrame(HWND hwnd);
void DisableIme(HWND hwnd);
void TrackMouseLeave(HWND hwnd);
HRESULT InitializeDpiAwareness();

} // namespace util
