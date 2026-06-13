#pragma once

#include <windows.h>

#include <d2d1_1.h>

// Shared window-coordinate helpers used by both window hosts (UiWindowHost and
// the main ImgViewer host). Centralises the physical-pixel <-> DPI-independent
// UI coordinate model so the two hosts don't each re-derive it.
namespace ui_host_input {

D2D1_SIZE_U ClientPixelSize(HWND hwnd);
D2D1_SIZE_F ClientRenderSize(HWND hwnd);
float DpiScale(HWND hwnd);
D2D1_POINT_2F PhysicalClientPointToUi(HWND hwnd, POINT point);
D2D1_POINT_2F PhysicalClientPointToUi(HWND hwnd, LPARAM lparam);
D2D1_POINT_2F ScreenPointToUi(HWND hwnd, POINT point);
POINT UiPointToPhysicalClient(HWND hwnd, D2D1_POINT_2F point);

} // namespace ui_host_input
