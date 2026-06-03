#include "ui.tooltip.hpp"

#include <commctrl.h>

#include <d2d1_1.h>

#include <wil/resource.h>

#include "imgviewer.hpp"

namespace {

constexpr UINT kTooltipToolInfoSize = TTTOOLINFOW_V2_SIZE;

RECT UiElementRectToWin32Rect(D2D1_RECT_F rect)
{
    return RECT{
        static_cast<LONG>(rect.left),
        static_cast<LONG>(rect.top),
        static_cast<LONG>(rect.right),
        static_cast<LONG>(rect.bottom),
    };
}

} // namespace

void UpdateUiTooltipRects(HWND hwnd, ImgViewerContext* context)
{
    if (context == nullptr || context->tooltip.get() == nullptr) {
        return;
    }

    for (size_t index = 0; index < context->ui.ElementCount(); ++index) {
        const UiElementMetadata* metadata = context->ui.ElementMetadataAt(index);
        if (metadata == nullptr || metadata->tooltip[0] == L'\0') {
            continue;
        }

        TTTOOLINFOW tool_info = {};
        tool_info.cbSize = kTooltipToolInfoSize;
        tool_info.hwnd = hwnd;
        tool_info.uId = static_cast<UINT_PTR>(UiElementRuntimeId(metadata->id));
        tool_info.rect = UiElementRectToWin32Rect(context->ui.ElementRect(metadata->id));
        SendMessageW(context->tooltip.get(), TTM_NEWTOOLRECTW, 0, reinterpret_cast<LPARAM>(&tool_info));
    }
}

HRESULT InitializeUiTooltips(HWND hwnd, ImgViewerContext* context)
{
    if (context == nullptr || context->tooltip.get() != nullptr) {
        return S_OK;
    }

    context->tooltip.reset(CreateWindowExW(
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
        nullptr));
    if (context->tooltip.get() == nullptr) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    SetWindowPos(
        context->tooltip.get(),
        HWND_TOPMOST,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    for (size_t index = 0; index < context->ui.ElementCount(); ++index) {
        const UiElementMetadata* metadata = context->ui.ElementMetadataAt(index);
        if (metadata == nullptr || metadata->tooltip[0] == L'\0') {
            continue;
        }

        TTTOOLINFOW tool_info = {};
        tool_info.cbSize = kTooltipToolInfoSize;
        tool_info.uFlags = TTF_SUBCLASS;
        tool_info.hwnd = hwnd;
        tool_info.uId = static_cast<UINT_PTR>(UiElementRuntimeId(metadata->id));
        tool_info.rect = UiElementRectToWin32Rect(context->ui.ElementRect(metadata->id));
        tool_info.lpszText = const_cast<LPWSTR>(metadata->tooltip);
        if (SendMessageW(context->tooltip.get(), TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool_info)) == FALSE) {
            context->tooltip.reset();
            return E_FAIL;
        }
    }

    return S_OK;
}
