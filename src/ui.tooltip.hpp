#pragma once

#include <windows.h>

struct AppContext;

void UpdateUiTooltipRects(HWND hwnd, AppContext* context);
HRESULT InitializeUiTooltips(HWND hwnd, AppContext* context);
