#pragma once

#include <windows.h>

#include "ui.a11y.hpp"

void UpdateUiTooltipRects(HWND hwnd, HWND tooltip, const UiAccessibilitySource& ui);
HRESULT InitializeUiTooltips(HWND hwnd, HWND* tooltip, const UiAccessibilitySource& ui);
