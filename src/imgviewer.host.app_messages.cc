#include "imgviewer.host.internal.hpp"

#include "imgviewer.messages.hpp"
#include "ui.a11y.hpp"

#include <windows.h>
#include <shellapi.h>

#include <vector>

win32::WindowMessageResult HandleImgViewerAppMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case kImgViewerUiActionMessage: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        const UiAction action = UiAction(static_cast<int>(wparam));
        DispatchUiAction(hwnd, context, action);
        return win32::WindowMessageResult::Handled();
    }

    case kImgViewerOpenImageMessage: {
        HandleImgViewerOpenImageCommand(hwnd, GetImgViewerContext(hwnd));
        return win32::WindowMessageResult::Handled();
    }

    case kImgViewerSettingsDestroyedMessage: {
        CleanupImgViewerSettingsWindow(
            GetImgViewerContext(hwnd),
            reinterpret_cast<void*>(lparam));
        return win32::WindowMessageResult::Handled();
    }

    case kImgViewerAboutDestroyedMessage: {
        CleanupImgViewerAboutWindow(
            GetImgViewerContext(hwnd),
            reinterpret_cast<void*>(lparam));
        return win32::WindowMessageResult::Handled();
    }

    case WM_TIMER: {
        if (wparam == kImgViewerToastTimerId) {
            KillTimer(hwnd, kImgViewerToastTimerId);
            ImgViewerContext* context = GetImgViewerContext(hwnd);
            if (context != nullptr && context->ui.HideToast()) {
                RenderImgViewer(context);
            }
            return win32::WindowMessageResult::Handled();
        }

        if (wparam == kImgViewerAnimationTimerId) {
            ImgViewerContext* context = GetImgViewerContext(hwnd);
            if (context == nullptr) {
                return win32::WindowMessageResult::Handled();
            }

            const DWORD now = GetTickCount();
            const UINT elapsed_ms = context->animation_last_tick_ms == 0
                ? 0
                : static_cast<UINT>(now - context->animation_last_tick_ms);
            context->animation_last_tick_ms = now;
            if (context->viewer.AdvanceAnimation(elapsed_ms)) {
                InvalidateImgViewerInfoPanelAnalysis(context);
                RenderImgViewer(context);
            }
            SyncImgViewerAnimationTimer(hwnd, context);
            return win32::WindowMessageResult::Handled();
        }

        return win32::WindowMessageResult::Unhandled();
    }

    case WM_DROPFILES: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        const HDROP drop = reinterpret_cast<HDROP>(wparam);
        const UINT length = DragQueryFileW(drop, 0, nullptr, 0);
        if (length > 0) {
            std::vector<wchar_t> path(static_cast<size_t>(length) + 1);
            DragQueryFileW(drop, 0, path.data(), static_cast<UINT>(path.size()));
            LoadImgViewerImageFile(hwnd, context, path.data());
        }
        DragFinish(drop);
        return win32::WindowMessageResult::Handled();
    }

    case WM_GETOBJECT: {
        if (lparam == UiaRootObjectId) {
            ImgViewerContext* context = GetImgViewerContext(hwnd);
            if (context != nullptr) {
                return win32::WindowMessageResult::Handled(UiaReturnRawElementProvider(
                    hwnd,
                    wparam,
                    lparam,
                    context->accessibility_provider.get()));
            }
        }

        return win32::WindowMessageResult::Unhandled();
    }

    default:
        return win32::WindowMessageResult::Unhandled();
    }
}
