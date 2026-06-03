#pragma once

#include <windows.h>

struct ImgViewerContext;

void UpdateUiTooltipRects(HWND hwnd, ImgViewerContext* context);
HRESULT InitializeUiTooltips(HWND hwnd, ImgViewerContext* context);
