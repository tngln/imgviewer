#include "ui.tooltip.hpp"

#include <commctrl.h>

#include <cmath>

#include <d2d1_1.h>

#include "math.hpp"

namespace {

constexpr UINT kTooltipToolInfoSize = TTTOOLINFOW_V2_SIZE;

RECT UiElementRectToWin32Rect(HWND hwnd, D2D1_RECT_F rect)
{
    const float dpi_scale = math::CoordinateSpace::FromWindow(hwnd).scale();
    return RECT{
        static_cast<LONG>(std::floor(rect.left * dpi_scale)),
        static_cast<LONG>(std::floor(rect.top * dpi_scale)),
        static_cast<LONG>(std::ceil(rect.right * dpi_scale)),
        static_cast<LONG>(std::ceil(rect.bottom * dpi_scale)),
    };
}

} // namespace

void UpdateUiTooltipRects(HWND hwnd, HWND tooltip, const UiAccessibilitySource& ui)
{
    if (tooltip == nullptr) {
        return;
    }

    for (size_t index = 0; index < ui.ElementCount(); ++index) {
        const UiElementMetadata* metadata = ui.ElementMetadataAt(index);
        if (metadata == nullptr || metadata->tooltip[0] == L'\0') {
            continue;
        }

        TTTOOLINFOW tool_info = {};
        tool_info.cbSize = kTooltipToolInfoSize;
        tool_info.hwnd = hwnd;
        tool_info.uId = static_cast<UINT_PTR>(UiElementRuntimeId(metadata->id));
        tool_info.rect = UiElementRectToWin32Rect(hwnd, ui.ElementRect(metadata->id));
        SendMessageW(tooltip, TTM_NEWTOOLRECTW, 0, reinterpret_cast<LPARAM>(&tool_info));
    }
}

HRESULT InitializeUiTooltips(HWND hwnd, HWND* tooltip, const UiAccessibilitySource& ui)
{
    if (tooltip == nullptr) {
        return E_POINTER;
    }

    if (*tooltip != nullptr) {
        return S_OK;
    }

    *tooltip = CreateWindowExW(
        WS_EX_TOPMOST,
        TOOLTIPS_CLASSW,
        nullptr,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        hwnd,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
    if (*tooltip == nullptr) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    SetWindowPos(
        *tooltip,
        HWND_TOPMOST,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    for (size_t index = 0; index < ui.ElementCount(); ++index) {
        const UiElementMetadata* metadata = ui.ElementMetadataAt(index);
        if (metadata == nullptr || metadata->tooltip[0] == L'\0') {
            continue;
        }

        TTTOOLINFOW tool_info = {};
        tool_info.cbSize = kTooltipToolInfoSize;
        tool_info.uFlags = TTF_SUBCLASS;
        tool_info.hwnd = hwnd;
        tool_info.uId = static_cast<UINT_PTR>(UiElementRuntimeId(metadata->id));
        tool_info.rect = UiElementRectToWin32Rect(hwnd, ui.ElementRect(metadata->id));
        tool_info.lpszText = const_cast<LPWSTR>(metadata->tooltip);
        if (SendMessageW(*tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool_info)) == FALSE) {
            DestroyWindow(*tooltip);
            *tooltip = nullptr;
            return E_FAIL;
        }
    }

    return S_OK;
}
