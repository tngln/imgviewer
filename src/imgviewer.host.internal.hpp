#pragma once

#include "imgviewer.hpp"
#include "imgviewer.action.hpp"
#include "imgviewer.viewer.hpp"
#include "ui.events.hpp"
#include "win32.window.hpp"

#include <windows.h>

#include <string>

ImgViewerContext* GetImgViewerContext(HWND hwnd);

D2D1_POINT_2F GetPointerPoint(HWND hwnd, LPARAM lparam, bool screen_to_client = false);
D2D1_POINT_2F GetScreenPointerPoint(HWND hwnd, LPARAM lparam);
bool IsCursorInsideWindow(HWND hwnd);
void TrackNonClientMouseLeave(HWND hwnd);

struct ImgViewerHostEffects final {
    bool handled = false;
    UiCaptureRequest capture = UiCaptureRequest::None;
    bool released_capture = false;
    ImgViewerPointerCaptureOwner begin_pointer_capture = ImgViewerPointerCaptureOwner::None;
    ImgViewerPointerCaptureOwner end_pointer_capture = ImgViewerPointerCaptureOwner::None;
    UiAction action = kUiActionNone;
    UiElementId effect_target = UiElementId::None;
    bool sync_popup_modal = false;
    bool sync_ime = false;

    void Merge(UiEventResult result, bool request_popup_modal_sync = false);
    void Merge(ImgViewerEventResult result);
    bool HasFollowUpWork() const
    {
        return capture != UiCaptureRequest::None ||
            released_capture ||
            begin_pointer_capture != ImgViewerPointerCaptureOwner::None ||
            end_pointer_capture != ImgViewerPointerCaptureOwner::None ||
            action != kUiActionNone ||
            effect_target != UiElementId::None ||
            sync_popup_modal ||
            sync_ime;
    }
};

void ApplyHostEffects(HWND hwnd, ImgViewerContext* context, ImgViewerHostEffects effects);
void ApplyMerged(HWND hwnd, ImgViewerContext* context, UiEventResult result);
void ApplyMerged(HWND hwnd, ImgViewerContext* context, ImgViewerEventResult result);
void ApplyRender(HWND hwnd, ImgViewerContext* context);
void ApplyRenderAndIme(HWND hwnd, ImgViewerContext* context);
void ApplyImeSync(HWND hwnd, ImgViewerContext* context);
ImgViewerHostEffects DispatchUiAction(HWND hwnd, ImgViewerContext* context, UiAction action);
bool DispatchToPopup(HWND hwnd, ImgViewerContext* context, const UiInputEvent& event);
void ClosePopup(ImgViewerContext* context);
void SyncPopupModal(ImgViewerContext* context);
void SyncKeyboardOwner(ImgViewerContext* context);
ImgViewerAction ActionForKeyboardMessage(const ImgViewerContext* context, WPARAM wparam);
size_t KeyActionIndex(WPARAM wparam);
void PositionMainWindowIme(HWND hwnd, ImgViewerContext* context);
ImgViewerPointerCaptureOwner EditPointerCaptureOwner(const ImgViewerEditController& edit);
bool CancelPendingEdgeClickIfDragged(HWND hwnd, ImgViewerContext* context, D2D1_POINT_2F point);
bool CommitPendingEdgeClick(HWND hwnd, ImgViewerContext* context, D2D1_POINT_2F point);
ImgViewerAction EdgeClickActionAtPoint(const ImgViewerContext* context, D2D1_POINT_2F point, bool require_no_capture = true);
void ShowWindowSizeToast(HWND hwnd, ImgViewerContext* context);

win32::WindowMessageResult HandleImgViewerAppMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
win32::WindowMessageResult HandleImgViewerChromeMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
win32::WindowMessageResult HandleImgViewerLifecycleMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
win32::WindowMessageResult HandleImgViewerPointerMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
win32::WindowMessageResult HandleImgViewerKeyboardMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
