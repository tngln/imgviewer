#pragma once

#include <windows.h>

#include <string>

namespace util {

bool IsKeyDown(int virtual_key);
bool IsWindowTopMost(HWND hwnd);
int ResizeBorderThicknessForDpi(UINT dpi);
std::wstring FileNameFromPath(const wchar_t* path, const wchar_t* fallback);
HRESULT ApplyDwmFrame(HWND hwnd, bool borderless);
void DisableIme(HWND hwnd);
void TrackMouseLeave(HWND hwnd);
HRESULT InitializeDpiAwareness();
bool CaptureWindowSize(HWND hwnd, int* width, int* height);
void ApplyMinTrackSize(HWND hwnd, LPARAM lparam, int min_client_width, int min_client_height);

} // namespace util
