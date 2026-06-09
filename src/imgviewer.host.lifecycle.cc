#include "imgviewer.host.internal.hpp"

#include "imgviewer.messages.hpp"
#include "ui.a11y.hpp"
#include "ui.tooltip.hpp"
#include "win32.util.hpp"

#include <windows.h>
#include <windowsx.h>

win32::WindowMessageResult HandleImgViewerLifecycleMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        if (context == nullptr ||
            FAILED(context->renderer.Initialize(hwnd)) ||
            FAILED(context->popup.Initialize(
                hwnd,
                kImgViewerUiActionMessage,
                context->renderer.D2DFactory(),
                context->renderer.DWriteFactory())) ||
            FAILED(context->viewer.Initialize()) ||
            FAILED(CreateUiAccessibilityProvider(
                hwnd,
                kImgViewerUiActionMessage,
                &context->ui,
                context->accessibility_provider.put()))) {
            return win32::WindowMessageResult::Handled(-1);
        }
        context->popup.SetTextFormats(context->renderer.BodyTextFormat(), context->renderer.IconTextFormat());
        context->viewer.SetPixelatedSampling(context->config.pixelated_sampling);
        context->renderer.SetCheckerboardBackground(context->config.checkerboard_background);
        ApplyWindowOpacity(hwnd, context->current_window_opacity_percent);
        if (FAILED(RenderImgViewer(context))) {
            return win32::WindowMessageResult::Handled(-1);
        }

        return win32::WindowMessageResult::Handled();
    }

    case WM_ENTERSIZEMOVE: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        if (context != nullptr) {
            ClosePopup(context);
            ResetImgViewerTransientInput(hwnd, context);
            context->interactive_size_move_active = true;
            context->last_window_size_toast_width = 0;
            context->last_window_size_toast_height = 0;
        }
        return win32::WindowMessageResult::Handled();
    }

    case WM_EXITSIZEMOVE: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        if (context != nullptr) {
            context->interactive_size_move_active = false;
        }
        return win32::WindowMessageResult::Handled();
    }

    case WM_SIZE: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        ClosePopup(context);
        if (context != nullptr && FAILED(context->renderer.Resize())) {
            return win32::WindowMessageResult::Handled(-1);
        }
        if (context != nullptr) {
            SyncWindowState(hwnd, &context->ui);
            if (FAILED(RenderImgViewer(context))) {
                return win32::WindowMessageResult::Handled(-1);
            }
            UpdateUiTooltipRects(hwnd, context->tooltip.get(), context->ui);
            if (context->interactive_size_move_active) {
                ShowWindowSizeToast(hwnd, context);
            }
        }

        return win32::WindowMessageResult::Handled();
    }

    case WM_DPICHANGED: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        ClosePopup(context);
        const auto* suggested_rect = reinterpret_cast<const RECT*>(lparam);
        if (suggested_rect != nullptr) {
            SetWindowPos(
                hwnd,
                nullptr,
                suggested_rect->left,
                suggested_rect->top,
                suggested_rect->right - suggested_rect->left,
                suggested_rect->bottom - suggested_rect->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
        if (context != nullptr && FAILED(context->renderer.Resize())) {
            return win32::WindowMessageResult::Handled(-1);
        }
        if (context != nullptr) {
            SyncWindowState(hwnd, &context->ui);
            if (FAILED(RenderImgViewer(context))) {
                return win32::WindowMessageResult::Handled(-1);
            }
            UpdateUiTooltipRects(hwnd, context->tooltip.get(), context->ui);
        }
        return win32::WindowMessageResult::Handled();
    }

    case WM_MOVE:
        ClosePopup(GetImgViewerContext(hwnd));
        return win32::WindowMessageResult::Unhandled();

    case WM_ACTIVATE:
        if (LOWORD(wparam) == WA_INACTIVE) {
            ImgViewerContext* context = GetImgViewerContext(hwnd);
            ClosePopup(context);
            if (context != nullptr) {
                ResetImgViewerTransientInput(hwnd, context);
                RenderIfNeeded(
                    hwnd,
                    context,
                    context->ui.OnInputEvent(UiInputEvent{.type = UiEventType::OwnerDeactivated, .hwnd = hwnd}));
            }
        }
        return win32::WindowMessageResult::Unhandled();

    case WM_ACTIVATEAPP:
        if (wparam == FALSE) {
            ClosePopup(GetImgViewerContext(hwnd));
        }
        return win32::WindowMessageResult::Unhandled();

    case WM_ERASEBKGND:
        return win32::WindowMessageResult::Handled(1);

    case WM_DESTROY:
        KillTimer(hwnd, kImgViewerToastTimerId);
        KillTimer(hwnd, kImgViewerAnimationTimerId);
        if (ImgViewerContext* context = GetImgViewerContext(hwnd)) {
            context->popup.Close();
            if (context->main_window_ime_context != nullptr) {
                util::AssociateImeContext(hwnd, context->main_window_ime_context);
                context->main_window_ime_enabled = true;
            }
            SaveWindowSize(hwnd, context);
        }
        PostQuitMessage(0);
        return win32::WindowMessageResult::Handled();

    default:
        return win32::WindowMessageResult::Unhandled();
    }
}
