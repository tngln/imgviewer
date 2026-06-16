#include "ui.host_effects.hpp"

UiPostedActionMessage DecodeUiPostedActionMessage(WPARAM wparam, LPARAM lparam)
{
    UNREFERENCED_PARAMETER(lparam);
    return UiPostedActionMessage{
        .action = UiAction(static_cast<int>(wparam)),
    };
}

void ApplyUiCaptureRequest(HWND hwnd, UiCaptureRequest capture)
{
    if (capture == UiCaptureRequest::Capture) {
        SetCapture(hwnd);
    } else if (capture == UiCaptureRequest::Release) {
        ReleaseCapture();
    }
}

void RequestWindowRender(HWND hwnd, bool render)
{
    if (hwnd != nullptr && render) {
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}
