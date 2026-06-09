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

void DispatchUiAction(HWND hwnd, ImgViewerContext* context, UiAction action);
void RenderIfNeeded(HWND hwnd, ImgViewerContext* context, UiEventResult result);
void RenderIfNeeded(HWND hwnd, ImgViewerContext* context, ImgViewerEventResult result);
void ClosePopup(ImgViewerContext* context);
void SyncPopupModal(ImgViewerContext* context);
void SyncKeyboardOwner(ImgViewerContext* context);
ImgViewerAction ActionForKeyboardMessage(const ImgViewerContext* context, WPARAM wparam);
size_t KeyActionIndex(WPARAM wparam);
std::wstring ImeCompositionString(HWND hwnd, LPARAM lparam, DWORD string_type);
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
