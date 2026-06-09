#include "imgviewer.host.internal.hpp"

#include "math.hpp"
#include "win32.util.hpp"

#include <windows.h>
#include <windowsx.h>

namespace {

LRESULT HitTestFrame(HWND hwnd, LPARAM lparam)
{
    POINT screen_point{
        GET_X_LPARAM(lparam),
        GET_Y_LPARAM(lparam),
    };

    RECT window_rect = {};
    GetWindowRect(hwnd, &window_rect);
    const UINT dpi = GetDpiForWindow(hwnd);
    const int resize_border = util::ResizeBorderThicknessForDpi(dpi);

    if (!IsZoomed(hwnd)) {
        const bool left = screen_point.x >= window_rect.left && screen_point.x < window_rect.left + resize_border;
        const bool right = screen_point.x < window_rect.right && screen_point.x >= window_rect.right - resize_border;
        const bool top = screen_point.y >= window_rect.top && screen_point.y < window_rect.top + resize_border;
        const bool bottom = screen_point.y < window_rect.bottom && screen_point.y >= window_rect.bottom - resize_border;

        if (top && left) {
            return HTTOPLEFT;
        }
        if (top && right) {
            return HTTOPRIGHT;
        }
        if (bottom && left) {
            return HTBOTTOMLEFT;
        }
        if (bottom && right) {
            return HTBOTTOMRIGHT;
        }
        if (left) {
            return HTLEFT;
        }
        if (right) {
            return HTRIGHT;
        }
        if (top) {
            return HTTOP;
        }
        if (bottom) {
            return HTBOTTOM;
        }
    }

    POINT client_point = screen_point;
    ScreenToClient(hwnd, &client_point);
    const math::CoordinateSpace coordinates = math::CoordinateSpace::FromWindow(hwnd);
    const D2D1_POINT_2F render_point = D2D1::Point2F(
        static_cast<float>(client_point.x) / coordinates.scale(),
        static_cast<float>(client_point.y) / coordinates.scale());
    ImgViewerContext* context = GetImgViewerContext(hwnd);
    if (context != nullptr && context->ui.IsPointInCaptionDragArea(render_point)) {
        return HTCAPTION;
    }

    return HTCLIENT;
}

LRESULT CalculateClientArea(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
    if (wparam != TRUE) {
        return DefWindowProcW(hwnd, WM_NCCALCSIZE, wparam, lparam);
    }

    auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lparam);
    if (IsZoomed(hwnd)) {
        const int resize_border = util::ResizeBorderThicknessForDpi(GetDpiForWindow(hwnd));
        params->rgrc[0].left += resize_border;
        params->rgrc[0].top += resize_border;
        params->rgrc[0].right -= resize_border;
        params->rgrc[0].bottom -= resize_border;
    }

    return 0;
}

} // namespace

win32::WindowMessageResult HandleImgViewerChromeMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_NCCALCSIZE:
        return win32::WindowMessageResult::Handled(CalculateClientArea(hwnd, wparam, lparam));

    case WM_NCHITTEST:
        return win32::WindowMessageResult::Handled(HitTestFrame(hwnd, lparam));

    case WM_NCLBUTTONDBLCLK:
        if (wparam == HTCAPTION) {
            ExecuteImgViewerAction(hwnd, GetImgViewerContext(hwnd), ImgViewerAction::ToggleMaximize);
            return win32::WindowMessageResult::Handled();
        }
        return win32::WindowMessageResult::Unhandled();

    case WM_NCMOUSEMOVE: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        if (context != nullptr && context->config.borderless_window) {
            context->renderer.SetUiOverlayVisible(true);
            TrackNonClientMouseLeave(hwnd);
        }
        return win32::WindowMessageResult::Unhandled();
    }

    case WM_NCMOUSELEAVE: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        if (context != nullptr &&
            context->config.borderless_window &&
            context->ui.CapturedElement() == UiElementId::None &&
            !IsCursorInsideWindow(hwnd)) {
            context->renderer.SetUiOverlayVisible(false);
        }
        return win32::WindowMessageResult::Handled();
    }

    default:
        return win32::WindowMessageResult::Unhandled();
    }
}
