#include "imgviewer.host.internal.hpp"

#include "imgviewer.messages.hpp"
#include "imgviewer.ui.hpp"
#include "ui.host_effects.hpp"
#include "ui.window.hpp"

#include <windows.h>
#include <shellapi.h>

#include <vector>

// Owned child windows (Settings/About/Developer) post a single
// kImgViewerOwnedWindowDestroyedMessage with their UiWindowDelegate* on destroy.
// One cleanup clears the matching slot/modal and deletes via the virtual base
// dtor (replaces three near-identical per-window handshakes; refactor.md L2).
void CleanupImgViewerOwnedWindow(ImgViewerContext* context, UiWindowDelegate* window)
{
    if (window == nullptr) {
        return;
    }
    if (context != nullptr) {
        if (context->settings_context == window) {
            context->settings_context = nullptr;
            context->interaction.ClearModal(ImgViewerModalOwner::Settings);
        } else if (context->about_context == window) {
            context->about_context = nullptr;
            context->interaction.ClearModal(ImgViewerModalOwner::About);
        } else if (context->developer_context == window) {
            context->developer_context = nullptr;
            context->interaction.ClearModal(ImgViewerModalOwner::Developer);
        }
    }
    delete window;
}

win32::WindowMessageResult HandleImgViewerAppMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case kImgViewerUiActionMessage: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        const UiPostedActionMessage posted = DecodeUiPostedActionMessage(wparam, lparam);
        ImgViewerHostEffects effects;
        effects.action = posted.action;
        ApplyHostEffects(hwnd, context, effects);
        return win32::WindowMessageResult::Handled();
    }

    case kImgViewerOpenImageMessage: {
        HandleImgViewerOpenImageCommand(hwnd, GetImgViewerContext(hwnd));
        return win32::WindowMessageResult::Handled();
    }

    case kImgViewerOwnedWindowDestroyedMessage: {
        CleanupImgViewerOwnedWindow(
            GetImgViewerContext(hwnd),
            reinterpret_cast<UiWindowDelegate*>(lparam));
        return win32::WindowMessageResult::Handled();
    }

    case WM_TIMER: {
        if (wparam == kImgViewerToastTimerId) {
            KillTimer(hwnd, kImgViewerToastTimerId);
            ImgViewerContext* context = GetImgViewerContext(hwnd);
            if (context != nullptr && context->main_ui->HideToast()) {
                RequestWindowRender(hwnd);
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
                RequestWindowRender(hwnd);
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

    default:
        return win32::WindowMessageResult::Unhandled();
    }
}
