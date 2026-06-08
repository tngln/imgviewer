#include "imgviewer.host.hpp"

#include "imgviewer.hpp"
#include "imgviewer.action.hpp"
#include "imgviewer.config.hpp"
#include "imgviewer.edit_geometry.hpp"
#include "imgviewer.keybindings.hpp"
#include "imgviewer.messages.hpp"
#include "imgviewer.ui.action.hpp"
#include "imgviewer.viewer.hpp"
#include "math.hpp"
#include "ui.a11y.hpp"
#include "ui.tooltip.hpp"
#include "win32.window.hpp"
#include "win32.util.hpp"

#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <commctrl.h>
#include <cmath>
#include <imm.h>
#include <shellapi.h>
#include <string>
#include <vector>

#include <wil/com.h>
#include <wil/resource.h>
#include <wil/result_macros.h>

namespace {

constexpr wchar_t kWindowClassName[] = L"ImgViewerWindow";

D2D1_POINT_2F GetPointerPoint(HWND hwnd, LPARAM lparam, bool screen_to_client = false)
{
    POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    if (screen_to_client) {
        ScreenToClient(hwnd, &point);
    }
    return D2D1::Point2F(static_cast<float>(point.x), static_cast<float>(point.y));
}

D2D1_POINT_2F GetScreenPointerPoint(HWND hwnd, LPARAM lparam)
{
    return GetPointerPoint(hwnd, lparam, true);
}

ImgViewerContext* GetImgViewerContext(HWND hwnd)
{
    return static_cast<ImgViewerContext*>(win32::NativeWindow::UserData(hwnd));
}

bool IsCursorInsideWindow(HWND hwnd)
{
    POINT cursor = {};
    RECT window_rect = {};
    return GetCursorPos(&cursor) &&
        GetWindowRect(hwnd, &window_rect) &&
        cursor.x >= window_rect.left &&
        cursor.x < window_rect.right &&
        cursor.y >= window_rect.top &&
        cursor.y < window_rect.bottom;
}

void TrackNonClientMouseLeave(HWND hwnd)
{
    TRACKMOUSEEVENT track_event = {};
    track_event.cbSize = sizeof(track_event);
    track_event.dwFlags = TME_LEAVE | TME_NONCLIENT;
    track_event.hwndTrack = hwnd;
    TrackMouseEvent(&track_event);
}

ImgViewerAction ActionForKeyboardMessage(const ImgViewerContext* context, WPARAM wparam)
{
    if (context == nullptr) {
        return ImgViewerAction::None;
    }

    return ActionForKey(
        context->config.action_bindings,
        static_cast<UINT>(wparam),
        util::IsKeyDown(VK_CONTROL),
        util::IsKeyDown(VK_SHIFT),
        util::IsKeyDown(VK_MENU));
}

size_t KeyActionIndex(WPARAM wparam)
{
    return static_cast<size_t>(static_cast<UINT>(wparam) & 0xFF);
}

std::wstring ImeCompositionString(HWND hwnd, LPARAM lparam)
{
    if ((lparam & GCS_COMPSTR) == 0) {
        return {};
    }

    HIMC ime = ImmGetContext(hwnd);
    if (ime == nullptr) {
        return {};
    }

    const LONG bytes = ImmGetCompositionStringW(ime, GCS_COMPSTR, nullptr, 0);
    std::wstring text(bytes > 0 ? static_cast<size_t>(bytes) / sizeof(wchar_t) : 0, L'\0');
    if (!text.empty()) {
        ImmGetCompositionStringW(ime, GCS_COMPSTR, text.data(), bytes);
    }
    ImmReleaseContext(hwnd, ime);
    return text;
}

bool EditingTextCaretPoint(ImgViewerContext* context, D2D1_POINT_2F* point)
{
    if (context == nullptr || point == nullptr) {
        return false;
    }

    const ImgViewerEditSnapshot edit = context->edit.Snapshot();
    if (!edit.active || !edit.editing_text || edit.editing_text_index >= edit.texts.size()) {
        return false;
    }

    constexpr float kPaddingX = 6.0f;
    constexpr float kPaddingY = 4.0f;
    const ImgViewerEditText& text = edit.texts[edit.editing_text_index];
    const float font_size = (std::max)(6.0f, text.style.font_size);
    wil::com_ptr<IDWriteTextFormat> format;
    if (FAILED(context->renderer.DWriteFactory()->CreateTextFormat(
            text.style.font_family.c_str(),
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            font_size,
            L"",
            format.put()))) {
        return false;
    }
    if (FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP))) {
        return false;
    }

    const TextEditState& edit_state = edit.editing_text_state;
    const std::wstring display_text = edit_state.DisplayText().empty() ? L" " : edit_state.DisplayText();
    const float width = (std::max)(48.0f, static_cast<float>(display_text.size()) * font_size * 0.55f + kPaddingX * 2.0f);
    wil::com_ptr<IDWriteTextLayout> layout;
    if (FAILED(context->renderer.DWriteFactory()->CreateTextLayout(
            display_text.c_str(),
            static_cast<UINT32>(display_text.size()),
            format.get(),
            width,
            4096.0f,
            layout.put()))) {
        return false;
    }

    const D2D1_POINT_2F origin = D2D1::Point2F(text.origin.x + kPaddingX, text.origin.y + kPaddingY);
    D2D1_POINT_2F caret_top = origin;
    D2D1_POINT_2F caret_bottom = origin;
    if (!edit_state.CaretMetrics(layout.get(), origin, &caret_top, &caret_bottom)) {
        return false;
    }
    *point = caret_bottom;
    return true;
}

D2D1_POINT_2F DocumentPointToViewportPoint(const ImgViewerSnapshot& image, const ImgViewerEditSnapshot& edit, D2D1_POINT_2F point, D2D1_SIZE_U viewport_size)
{
    const D2D1_SIZE_U preview_size = imgviewer_edit_geometry::EditPreviewSize(image.pixel_size, edit.rotation_quadrants);
    const float image_scale = math::FitScale(preview_size, viewport_size) * image.zoom_multiplier;
    const D2D1_POINT_2F viewport_center = D2D1::Point2F(
        static_cast<float>(viewport_size.width) * 0.5f,
        static_cast<float>(viewport_size.height) * 0.5f);
    const D2D1_POINT_2F preview_view_center = imgviewer_edit_geometry::SourcePointToEditPreviewPoint(
        image.view_center,
        image.pixel_size,
        edit.rotation_quadrants);
    const D2D1_MATRIX_3X2_F transform =
        imgviewer_edit_geometry::SourceToEditPreviewTransform(image.pixel_size, edit.rotation_quadrants) *
        D2D1::Matrix3x2F::Translation(-preview_view_center.x, -preview_view_center.y) *
        D2D1::Matrix3x2F::Scale(image_scale, image_scale) *
        D2D1::Matrix3x2F::Scale(
            image.flipped_horizontal ? -1.0f : 1.0f,
            image.flipped_vertical ? -1.0f : 1.0f,
            D2D1::Point2F(0.0f, 0.0f)) *
        D2D1::Matrix3x2F::Rotation(image.rotation_degrees, D2D1::Point2F(0.0f, 0.0f)) *
        D2D1::Matrix3x2F::Translation(viewport_center.x, viewport_center.y);
    return D2D1::Point2F(
        point.x * transform._11 + point.y * transform._21 + transform._31,
        point.x * transform._12 + point.y * transform._22 + transform._32);
}

void PositionMainWindowIme(HWND hwnd, ImgViewerContext* context)
{
    if (context == nullptr || !context->edit.IsEditingText()) {
        return;
    }

    D2D1_POINT_2F caret_document_point = {};
    if (!EditingTextCaretPoint(context, &caret_document_point)) {
        return;
    }

    const ImgViewerSnapshot image = context->viewer.Snapshot();
    const ImgViewerEditSnapshot edit = context->edit.Snapshot();
    const D2D1_SIZE_U viewport_size = context->renderer.ViewportPixelSize();
    if (image.bitmap == nullptr || viewport_size.width == 0 || viewport_size.height == 0) {
        return;
    }

    const D2D1_POINT_2F caret_viewport_point = DocumentPointToViewportPoint(image, edit, caret_document_point, viewport_size);
    HIMC ime = ImmGetContext(hwnd);
    if (ime == nullptr) {
        return;
    }
    COMPOSITIONFORM form = {};
    form.dwStyle = CFS_POINT;
    form.ptCurrentPos = POINT{
        static_cast<LONG>(std::floor(caret_viewport_point.x)),
        static_cast<LONG>(std::floor(caret_viewport_point.y)),
    };
    ImmSetCompositionWindow(ime, &form);
    ImmReleaseContext(hwnd, ime);
}

void SyncPopupModal(ImgViewerContext* context);

void DispatchUiAction(HWND hwnd, ImgViewerContext* context, UiAction action)
{
    if (context == nullptr || action == kUiActionNone) {
        return;
    }

    if (context->ui.Root() != nullptr && context->ui.Root()->HandleUiAction(action, &context->popup)) {
        SyncPopupModal(context);
        RenderImgViewer(context);
        SyncImgViewerMainWindowIme(hwnd, context);
        return;
    }

    const ImgViewerAction viewer_action = ImgViewerActionFromUiAction(action);
    if (viewer_action == ImgViewerAction::OpenImage) {
        HandleImgViewerOpenImageCommand(hwnd, context);
        return;
    }

    ExecuteImgViewerAction(hwnd, context, viewer_action);
    SyncPopupModal(context);
    SyncImgViewerMainWindowIme(hwnd, context);
}

void RenderIfNeeded(HWND hwnd, ImgViewerContext* context, UiEventResult result)
{
    if (context == nullptr) {
        return;
    }

    if (result.capture == UiCaptureRequest::Capture) {
        if (context != nullptr) {
            context->interaction.BeginPointerCapture(ImgViewerPointerCaptureOwner::Ui);
        }
        SetCapture(hwnd);
    } else if (result.capture == UiCaptureRequest::Release) {
        if (context != nullptr) {
            context->interaction.EndPointerCapture(ImgViewerPointerCaptureOwner::Ui);
        }
        ReleaseCapture();
    }

    if (result.needs_render) {
        RenderImgViewer(context);
        PositionMainWindowIme(hwnd, context);
    }

    DispatchUiAction(hwnd, context, result.action);
    SyncImgViewerMainWindowIme(hwnd, context);
}

void RenderIfNeeded(HWND hwnd, ImgViewerContext* context, ImgViewerEventResult result)
{
    if (context == nullptr) {
        return;
    }

    if (result.released_capture) {
        if (context != nullptr) {
            context->interaction.ClearPointerCapture();
        }
        ReleaseCapture();
    }

    if (result.needs_render) {
        RenderImgViewer(context);
        PositionMainWindowIme(hwnd, context);
    }

    SyncImgViewerMainWindowIme(hwnd, context);
}

void ClosePopup(ImgViewerContext* context)
{
    if (context != nullptr && context->popup.IsOpen()) {
        context->popup.Close();
        context->interaction.ClearModal(ImgViewerModalOwner::Popup);
    }
}

void SyncPopupModal(ImgViewerContext* context)
{
    if (context == nullptr) {
        return;
    }

    if (context->popup.IsOpen()) {
        if (context->interaction.Modal() != ImgViewerModalOwner::Popup) {
            context->interaction.SetModal(ImgViewerModalOwner::Popup);
        }
    } else {
        context->interaction.ClearModal(ImgViewerModalOwner::Popup);
    }
}

void SyncKeyboardOwner(ImgViewerContext* context)
{
    if (context == nullptr) {
        return;
    }

    if (context->popup.IsOpen()) {
        context->interaction.SetKeyboardOwner(ImgViewerKeyboardOwner::Popup);
    } else if (context->edit.IsEditingText()) {
        context->interaction.SetKeyboardOwner(ImgViewerKeyboardOwner::EditText);
    } else if (context->ui.FocusedElement() != UiElementId::None) {
        context->interaction.SetKeyboardOwner(ImgViewerKeyboardOwner::UiFocus);
    } else {
        context->interaction.SetKeyboardOwner(ImgViewerKeyboardOwner::ViewerShortcut);
    }
}

ImgViewerPointerCaptureOwner EditPointerCaptureOwner(const ImgViewerEditController& edit)
{
    if (edit.Tool() == ImgViewerEditTool::Crop) {
        return ImgViewerPointerCaptureOwner::EditCrop;
    }
    if (edit.Tool() == ImgViewerEditTool::PixelSelect) {
        return ImgViewerPointerCaptureOwner::EditPixelSelection;
    }
    return ImgViewerPointerCaptureOwner::EditStroke;
}

void ShowWindowSizeToast(HWND hwnd, ImgViewerContext* context)
{
    if (context == nullptr || IsIconic(hwnd)) {
        return;
    }

    RECT window_rect = {};
    if (!GetWindowRect(hwnd, &window_rect)) {
        return;
    }

    const int width = static_cast<int>(window_rect.right - window_rect.left);
    const int height = static_cast<int>(window_rect.bottom - window_rect.top);
    if (width <= 0 || height <= 0 ||
        (width == context->last_window_size_toast_width && height == context->last_window_size_toast_height)) {
        return;
    }

    context->last_window_size_toast_width = width;
    context->last_window_size_toast_height = height;

    wchar_t toast_text[64] = {};
    swprintf_s(toast_text, L"Window %dx%d", width, height);
    ShowImgViewerToast(hwnd, context, toast_text);
}

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

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case kImgViewerUiActionMessage: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        const UiAction action = UiAction(static_cast<int>(wparam));
        DispatchUiAction(hwnd, context, action);
        return 0;
    }

    case kImgViewerOpenImageMessage: {
        HandleImgViewerOpenImageCommand(hwnd, GetImgViewerContext(hwnd));
        return 0;
    }

    case kImgViewerSettingsDestroyedMessage: {
        CleanupImgViewerSettingsWindow(
            GetImgViewerContext(hwnd),
            reinterpret_cast<void*>(lparam));
        return 0;
    }

    case kImgViewerAboutDestroyedMessage: {
        CleanupImgViewerAboutWindow(
            GetImgViewerContext(hwnd),
            reinterpret_cast<void*>(lparam));
        return 0;
    }

    case WM_NCCALCSIZE:
        return CalculateClientArea(hwnd, wparam, lparam);

    case WM_NCCREATE: {
        const auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create_struct->lpCreateParams));
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

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
            return -1;
        }
        context->popup.SetTextFormats(context->renderer.BodyTextFormat(), context->renderer.IconTextFormat());
        context->viewer.SetPixelatedSampling(context->config.pixelated_sampling);
        context->renderer.SetCheckerboardBackground(context->config.checkerboard_background);
        ApplyWindowOpacity(hwnd, context->current_window_opacity_percent);
        if (FAILED(RenderImgViewer(context))) {
            return -1;
        }

        return 0;
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
        return 0;
    }

    case WM_EXITSIZEMOVE: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        if (context != nullptr) {
            context->interactive_size_move_active = false;
        }
        return 0;
    }

    case WM_SIZE: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        ClosePopup(context);
        if (context != nullptr && FAILED(context->renderer.Resize())) {
            return -1;
        }
        if (context != nullptr) {
            SyncWindowState(hwnd, &context->ui);
            if (FAILED(RenderImgViewer(context))) {
                return -1;
            }
            UpdateUiTooltipRects(hwnd, context->tooltip.get(), context->ui);
            if (context->interactive_size_move_active) {
                ShowWindowSizeToast(hwnd, context);
            }
        }

        return 0;
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
            return -1;
        }
        if (context != nullptr) {
            SyncWindowState(hwnd, &context->ui);
            if (FAILED(RenderImgViewer(context))) {
                return -1;
            }
            UpdateUiTooltipRects(hwnd, context->tooltip.get(), context->ui);
        }
        return 0;
    }

    case WM_MOVE:
        ClosePopup(GetImgViewerContext(hwnd));
        return DefWindowProcW(hwnd, message, wparam, lparam);

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
        return DefWindowProcW(hwnd, message, wparam, lparam);

    case WM_ACTIVATEAPP:
        if (wparam == FALSE) {
            ClosePopup(GetImgViewerContext(hwnd));
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);

    case WM_NCHITTEST:
        return HitTestFrame(hwnd, lparam);

    case WM_NCLBUTTONDBLCLK:
        if (wparam == HTCAPTION) {
            ExecuteImgViewerAction(hwnd, GetImgViewerContext(hwnd), ImgViewerAction::ToggleMaximize);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);

    case WM_NCMOUSEMOVE: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        if (context != nullptr && context->config.borderless_window) {
            context->renderer.SetUiOverlayVisible(true);
            TrackNonClientMouseLeave(hwnd);
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_NCMOUSELEAVE: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        if (context != nullptr &&
            context->config.borderless_window &&
            context->ui.CapturedElement() == UiElementId::None &&
            !IsCursorInsideWindow(hwnd)) {
            context->renderer.SetUiOverlayVisible(false);
        }
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_MOUSEMOVE: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        if (context != nullptr && context->config.borderless_window) {
            context->renderer.SetUiOverlayVisible(true);
        }
        const D2D1_POINT_2F point = GetPointerPoint(hwnd, lparam);
        util::TrackMouseLeave(hwnd);
        if (context != nullptr) {
            SyncPopupModal(context);
            const math::CoordinateSpace coordinates = math::CoordinateSpace::FromWindow(hwnd);
            const D2D1_POINT_2F ui_point = D2D1::Point2F(point.x / coordinates.scale(), point.y / coordinates.scale());
            UiPointerEvent pointer{
                .type = UiEventType::PointerMove,
                .point = ui_point,
                .modifiers = UiModifiers::Current(),
                .popup_host = &context->popup,
            };
            if (context->popup.IsOpen()) {
                UiEventResult popup_result = context->popup.OnInputEvent(UiInputEvent::Pointer(pointer, hwnd));
                RenderIfNeeded(hwnd, context, popup_result);
                SyncPopupModal(context);
                if (popup_result.handled) {
                    return 0;
                }
            }
            if (context->interaction.PointerCapture() == ImgViewerPointerCaptureOwner::Ui ||
                !context->interaction.HasPointerCapture()) {
                UiEventResult ui_result = context->ui.OnInputEvent(UiInputEvent::Pointer(pointer, hwnd));
                RenderIfNeeded(hwnd, context, ui_result);
                if (ui_result.handled) {
                    return 0;
                }
            }

            ImgViewerEventResult canvas_result = {};
            if (context->interaction.PointerCapture() == ImgViewerPointerCaptureOwner::ColorPicker) {
                if (UpdateImgViewerColorPickerSample(context, point)) {
                    RenderImgViewer(context);
                    return 0;
                }
            } else if (context->interaction.PointerCapture() == ImgViewerPointerCaptureOwner::EditStroke ||
                context->interaction.PointerCapture() == ImgViewerPointerCaptureOwner::EditCrop ||
                context->interaction.PointerCapture() == ImgViewerPointerCaptureOwner::EditPixelSelection) {
                canvas_result = context->edit.OnPointerMove(point, context->viewer.Snapshot(), context->renderer.ViewportPixelSize());
            } else if (context->interaction.PointerCapture() == ImgViewerPointerCaptureOwner::ViewerPan ||
                context->interaction.PointerCapture() == ImgViewerPointerCaptureOwner::ViewerRotate) {
                canvas_result = context->viewer.OnPointerMove(point.x, point.y, context->renderer.ViewportPixelSize());
            } else if (context->interaction.CanvasOwner() == ImgViewerCanvasOwner::EditTool && context->edit.Active()) {
                canvas_result = context->edit.OnPointerMove(point, context->viewer.Snapshot(), context->renderer.ViewportPixelSize());
            } else if (context->interaction.CanvasOwner() == ImgViewerCanvasOwner::Viewer) {
                canvas_result = context->viewer.OnPointerMove(point.x, point.y, context->renderer.ViewportPixelSize());
            }
            RenderIfNeeded(hwnd, context, canvas_result);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        const D2D1_POINT_2F point = GetPointerPoint(hwnd, lparam);
        const float dpi_scale = math::CoordinateSpace::FromWindow(hwnd).scale();
        const D2D1_POINT_2F ui_point = D2D1::Point2F(point.x / dpi_scale, point.y / dpi_scale);
        UiPointerEvent pointer{
            .type = UiEventType::PointerDown,
            .point = ui_point,
            .button = UiPointerButton::Left,
            .modifiers = UiModifiers::Current(),
            .popup_host = context != nullptr ? &context->popup : nullptr,
        };
        if (context != nullptr && context->popup.IsOpen()) {
            SyncPopupModal(context);
            UiEventResult popup_result = context->popup.OnInputEvent(UiInputEvent::Pointer(pointer, hwnd));
            RenderIfNeeded(hwnd, context, popup_result);
            SyncPopupModal(context);
            if (popup_result.handled) {
                return 0;
            }
        }
        UiEventResult ui_result = context != nullptr
            ? context->ui.OnInputEvent(UiInputEvent::Pointer(pointer, hwnd))
            : UiEventResult{};
        ImgViewerEventResult viewer_result = {};
        if (context != nullptr && !ui_result.handled) {
            if (context->interaction.CanvasOwner() == ImgViewerCanvasOwner::ColorPicker &&
                UpdateImgViewerColorPickerSample(context, point)) {
                context->interaction.BeginPointerCapture(ImgViewerPointerCaptureOwner::ColorPicker);
                SetCapture(hwnd);
                ui_result.handled = true;
                ui_result.needs_render = true;
            } else if (context->interaction.CanvasOwner() == ImgViewerCanvasOwner::EditTool && context->edit.Active()) {
                viewer_result = context->edit.OnPointerDown(point, context->viewer.Snapshot(), context->renderer.ViewportPixelSize());
            } else {
                viewer_result = context->viewer.OnPointerDown(point.x, point.y, context->renderer.ViewportPixelSize());
            }
        }
        if (viewer_result.captured) {
            if (context != nullptr && context->edit.HasTransientCapture()) {
                context->interaction.BeginPointerCapture(EditPointerCaptureOwner(context->edit));
            } else if (context != nullptr && context->viewer.HasTransientCapture()) {
                context->interaction.BeginPointerCapture(util::IsKeyDown('R')
                    ? ImgViewerPointerCaptureOwner::ViewerRotate
                    : ImgViewerPointerCaptureOwner::ViewerPan);
            }
            SetCapture(hwnd);
        }
        RenderIfNeeded(hwnd, context, ui_result);
        RenderIfNeeded(hwnd, context, viewer_result);
        return (ui_result.handled || viewer_result.handled) ? 0 : DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_LBUTTONDBLCLK: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        const D2D1_POINT_2F point = GetPointerPoint(hwnd, lparam);
        ImgViewerEventResult edit_result = {};
        if (context != nullptr &&
            context->interaction.CanvasOwner() == ImgViewerCanvasOwner::EditTool &&
            context->edit.Active()) {
            edit_result = context->edit.OnPointerDoubleClick(
                point,
                context->viewer.Snapshot(),
                context->renderer.ViewportPixelSize());
        }
        RenderIfNeeded(hwnd, context, edit_result);
        return edit_result.handled ? 0 : DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_LBUTTONUP: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        const D2D1_POINT_2F point = GetPointerPoint(hwnd, lparam);
        ImgViewerEventResult viewer_result = {};
        if (context != nullptr) {
            if (context->interaction.PointerCapture() == ImgViewerPointerCaptureOwner::ColorPicker) {
                context->interaction.EndPointerCapture(ImgViewerPointerCaptureOwner::ColorPicker);
                ReleaseCapture();
                viewer_result = ImgViewerEventResult{.handled = true};
            } else if (context->interaction.PointerCapture() == ImgViewerPointerCaptureOwner::EditStroke ||
                context->interaction.PointerCapture() == ImgViewerPointerCaptureOwner::EditCrop ||
                context->interaction.PointerCapture() == ImgViewerPointerCaptureOwner::EditPixelSelection) {
                viewer_result = context->edit.OnPointerUp(point, context->viewer.Snapshot(), context->renderer.ViewportPixelSize());
            } else if (context->interaction.PointerCapture() == ImgViewerPointerCaptureOwner::ViewerPan ||
                context->interaction.PointerCapture() == ImgViewerPointerCaptureOwner::ViewerRotate) {
                viewer_result = context->viewer.OnPointerUp(point.x, point.y, context->renderer.ViewportPixelSize());
            }
        }
        RenderIfNeeded(hwnd, context, viewer_result);
        UiEventResult ui_result = {};
        if (context != nullptr &&
            !viewer_result.handled &&
            (context->interaction.PointerCapture() == ImgViewerPointerCaptureOwner::Ui ||
                !context->interaction.HasPointerCapture())) {
            const float dpi_scale = math::CoordinateSpace::FromWindow(hwnd).scale();
            const D2D1_POINT_2F ui_point = D2D1::Point2F(point.x / dpi_scale, point.y / dpi_scale);
            UiPointerEvent pointer{
                .type = UiEventType::PointerUp,
                .point = ui_point,
                .button = UiPointerButton::Left,
                .modifiers = UiModifiers::Current(),
                .popup_host = &context->popup,
            };
            if (context->popup.IsOpen()) {
                SyncPopupModal(context);
                UiEventResult popup_result = context->popup.OnInputEvent(UiInputEvent::Pointer(pointer, hwnd));
                RenderIfNeeded(hwnd, context, popup_result);
                SyncPopupModal(context);
                if (popup_result.handled) {
                    return 0;
                }
            }
            ui_result = context->ui.OnInputEvent(UiInputEvent::Pointer(pointer, hwnd));
            RenderIfNeeded(hwnd, context, ui_result);
        }
        return (ui_result.handled || viewer_result.handled) ? 0 : DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_MOUSELEAVE: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        UiPointerEvent pointer{.type = UiEventType::PointerLeave, .modifiers = UiModifiers::Current()};
        RenderIfNeeded(
            hwnd,
            context,
            context != nullptr ? context->ui.OnInputEvent(UiInputEvent{
                .type = pointer.type,
                .pointer = pointer,
                .hwnd = hwnd,
                .popup_host = &context->popup,
            }) : UiEventResult{});
        if (context != nullptr &&
            context->config.borderless_window &&
            context->ui.CapturedElement() == UiElementId::None &&
            !IsCursorInsideWindow(hwnd)) {
            context->renderer.SetUiOverlayVisible(false);
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        const D2D1_POINT_2F point = GetScreenPointerPoint(hwnd, lparam);
        if (context != nullptr && util::IsKeyDown('O')) {
            constexpr int kOpacityWheelStep = 5;
            const int wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam);
            const int steps = wheel_delta / WHEEL_DELTA;
            if (steps != 0) {
                SetImgViewerWindowOpacity(
                    hwnd,
                    context,
                    context->current_window_opacity_percent + steps * kOpacityWheelStep);
            }
            return 0;
        }
        if (context != nullptr) {
            const float dpi_scale = math::CoordinateSpace::FromWindow(hwnd).scale();
            const D2D1_POINT_2F ui_point = D2D1::Point2F(point.x / dpi_scale, point.y / dpi_scale);
            UiPointerEvent pointer{
                .type = UiEventType::PointerWheel,
                .point = ui_point,
                .wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam),
                .modifiers = UiModifiers::Current(),
                .popup_host = &context->popup,
            };
            if (context->popup.IsOpen()) {
                SyncPopupModal(context);
                UiEventResult popup_result = context->popup.OnInputEvent(UiInputEvent::Pointer(pointer, hwnd));
                RenderIfNeeded(hwnd, context, popup_result);
                SyncPopupModal(context);
                if (popup_result.handled) {
                    return 0;
                }
            }
            const UiEventResult ui_result = context->ui.OnInputEvent(UiInputEvent::Pointer(pointer, hwnd));
            if (ui_result.handled) {
                RenderIfNeeded(hwnd, context, ui_result);
                return 0;
            }
        }
        if (context != nullptr &&
            context->interaction.CanvasOwner() == ImgViewerCanvasOwner::Viewer &&
            context->viewer.OnMouseWheel(
                point.x,
                point.y,
                GET_WHEEL_DELTA_WPARAM(wparam),
                context->renderer.ViewportPixelSize())) {
            RenderImgViewer(context);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        UiKeyEvent key{
            .type = UiEventType::KeyDown,
            .virtual_key = static_cast<UINT>(wparam),
            .modifiers = UiModifiers::Current(),
            .repeat = (lparam & 0x40000000) != 0,
            .popup_host = context != nullptr ? &context->popup : nullptr,
        };
        if (context != nullptr && context->popup.IsOpen()) {
            SyncPopupModal(context);
            UiEventResult popup_result = context->popup.OnInputEvent(UiInputEvent::Key(key, hwnd));
            RenderIfNeeded(hwnd, context, popup_result);
            SyncPopupModal(context);
            if (popup_result.handled) {
                return 0;
            }
        }

        SyncKeyboardOwner(context);
        if (context != nullptr && context->interaction.KeyboardOwner() == ImgViewerKeyboardOwner::EditText) {
            if (!key.modifiers.alt && key.modifiers.ctrl) {
                UiAction text_action = kUiActionNone;
                if (wparam == 'A') {
                    text_action = kUiActionTextSelectAll;
                } else if (wparam == 'C') {
                    text_action = kUiActionTextCopy;
                } else if (wparam == 'X') {
                    text_action = kUiActionTextCut;
                } else if (wparam == 'V') {
                    text_action = kUiActionTextPaste;
                }
                if (text_action != kUiActionNone) {
                    if (context->edit.ExecuteTextEditAction(text_action, hwnd)) {
                        RenderImgViewer(context);
                        PositionMainWindowIme(hwnd, context);
                    }
                    return 0;
                }
            }
            if (!key.modifiers.ctrl && !key.modifiers.alt) {
                if (context->edit.OnTextKeyDown(static_cast<UINT>(wparam), key.modifiers.shift)) {
                    RenderImgViewer(context);
                    PositionMainWindowIme(hwnd, context);
                }
                return 0;
            }
        }

        if (context != nullptr &&
            wparam == VK_ESCAPE &&
            !key.modifiers.ctrl &&
            !key.modifiers.shift &&
            !key.modifiers.alt &&
            context->edit.Active() &&
            context->edit.Tool() == ImgViewerEditTool::Crop) {
            ExecuteImgViewerAction(hwnd, context, ImgViewerAction::EditCancelCrop);
            return 0;
        }
        if (context != nullptr &&
            wparam == VK_ESCAPE &&
            !key.modifiers.ctrl &&
            !key.modifiers.shift &&
            !key.modifiers.alt &&
            context->edit.Active() &&
            context->edit.Tool() == ImgViewerEditTool::Select &&
            context->edit.HasSelection()) {
            context->edit.CancelSelection();
            RenderImgViewer(context);
            SyncImgViewerMainWindowIme(hwnd, context);
            return 0;
        }

        const UiEventResult ui_result = context != nullptr
            ? context->ui.OnInputEvent(UiInputEvent::Key(key, hwnd))
            : UiEventResult{};
        if (ui_result.handled) {
            RenderIfNeeded(hwnd, context, ui_result);
            return 0;
        }

        if (message == WM_KEYDOWN && wparam == 'V' && key.modifiers.ctrl && !key.modifiers.shift && !key.modifiers.alt) {
            HandleImgViewerPasteClipboard(hwnd, context);
            return 0;
        }
        if (message == WM_KEYDOWN && wparam == 'S' && key.modifiers.ctrl && !key.modifiers.shift && !key.modifiers.alt) {
            ExecuteImgViewerAction(hwnd, context, ImgViewerAction::SaveImageAs);
            return 0;
        }

        const ImgViewerAction action = ActionForKeyboardMessage(context, wparam);
        if (context != nullptr) {
            context->pressed_key_actions[KeyActionIndex(wparam)] = action;
        }
        if (context != nullptr &&
            context->interaction.CanvasOwner() == ImgViewerCanvasOwner::Viewer &&
            context->viewer.OnActionDown(action)) {
            return 0;
        }
        if (action == ImgViewerAction::OpenImage) {
            HandleImgViewerOpenImageCommand(hwnd, context);
            return 0;
        }
        if (action != ImgViewerAction::None) {
            ExecuteImgViewerAction(hwnd, context, action);
            SyncImgViewerMainWindowIme(hwnd, context);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_CHAR: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        SyncKeyboardOwner(context);
        if (context != nullptr &&
            context->interaction.KeyboardOwner() == ImgViewerKeyboardOwner::EditText &&
            context->edit.OnTextInput(static_cast<wchar_t>(wparam))) {
            RenderImgViewer(context);
            PositionMainWindowIme(hwnd, context);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_IME_STARTCOMPOSITION: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        SyncKeyboardOwner(context);
        if (context != nullptr && context->interaction.KeyboardOwner() == ImgViewerKeyboardOwner::EditText) {
            PositionMainWindowIme(hwnd, context);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_IME_COMPOSITION: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        SyncKeyboardOwner(context);
        if (context != nullptr && context->interaction.KeyboardOwner() == ImgViewerKeyboardOwner::EditText) {
            if (context->edit.UpdateTextImeComposition(ImeCompositionString(hwnd, lparam))) {
                RenderImgViewer(context);
                PositionMainWindowIme(hwnd, context);
            }
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_IME_ENDCOMPOSITION: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        SyncKeyboardOwner(context);
        if (context != nullptr && context->interaction.KeyboardOwner() == ImgViewerKeyboardOwner::EditText) {
            if (context->edit.EndTextImeComposition()) {
                RenderImgViewer(context);
            }
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_KEYUP:
    case WM_SYSKEYUP: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        UiKeyEvent key{
            .type = UiEventType::KeyUp,
            .virtual_key = static_cast<UINT>(wparam),
            .modifiers = UiModifiers::Current(),
            .popup_host = context != nullptr ? &context->popup : nullptr,
        };
        if (context != nullptr && context->popup.IsOpen()) {
            SyncPopupModal(context);
            UiEventResult popup_result = context->popup.OnInputEvent(UiInputEvent::Key(key, hwnd));
            RenderIfNeeded(hwnd, context, popup_result);
            SyncPopupModal(context);
            if (popup_result.handled) {
                return 0;
            }
        }
        const UiEventResult ui_result = context != nullptr
            ? context->ui.OnInputEvent(UiInputEvent::Key(key, hwnd))
            : UiEventResult{};
        if (ui_result.handled) {
            RenderIfNeeded(hwnd, context, ui_result);
            return 0;
        }
        const ImgViewerAction action =
            context != nullptr ? context->pressed_key_actions[KeyActionIndex(wparam)] : ImgViewerAction::None;
        if (context != nullptr) {
            context->pressed_key_actions[KeyActionIndex(wparam)] = ImgViewerAction::None;
        }
        const ImgViewerEventResult viewer_result =
            context != nullptr && context->interaction.CanvasOwner() == ImgViewerCanvasOwner::Viewer
                ? context->viewer.OnActionUp(action)
                : ImgViewerEventResult{};
        if (viewer_result.handled) {
            RenderIfNeeded(hwnd, context, viewer_result);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_TIMER: {
        if (wparam == kImgViewerToastTimerId) {
            KillTimer(hwnd, kImgViewerToastTimerId);
            ImgViewerContext* context = GetImgViewerContext(hwnd);
            if (context != nullptr && context->ui.HideToast()) {
                RenderImgViewer(context);
            }
            return 0;
        }

        if (wparam == kImgViewerAnimationTimerId) {
            ImgViewerContext* context = GetImgViewerContext(hwnd);
            if (context == nullptr) {
                return 0;
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
            return 0;
        }

        return DefWindowProcW(hwnd, message, wparam, lparam);
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
        return 0;
    }

    case WM_GETOBJECT: {
        if (lparam == UiaRootObjectId) {
            ImgViewerContext* context = GetImgViewerContext(hwnd);
            if (context != nullptr) {
                return UiaReturnRawElementProvider(
                    hwnd,
                    wparam,
                    lparam,
                    context->accessibility_provider.get());
            }
        }

        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

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
        return 0;

    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

class ImgViewerWindowDelegate final : public win32::NativeWindowDelegate {
public:
    win32::WindowMessageResult OnWindowMessage(
        win32::NativeWindow& window,
        UINT message,
        WPARAM wparam,
        LPARAM lparam) override
    {
        return win32::WindowMessageResult::Handled(WindowProc(window.Hwnd(), message, wparam, lparam));
    }
};

HRESULT RunImgViewerApplicationAsHresult()
{
    INITCOMMONCONTROLSEX common_controls = {
        .dwSize = sizeof(common_controls),
        .dwICC = ICC_WIN95_CLASSES,
    };
    InitCommonControlsEx(&common_controls);

    RETURN_IF_FAILED(util::InitializeDpiAwareness());
    const HRESULT co_initialize_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    RETURN_IF_FAILED(co_initialize_result);
    auto co_uninitialize = wil::scope_exit([] { CoUninitialize(); });

    HINSTANCE instance = GetModuleHandleW(nullptr);
    RETURN_LAST_ERROR_IF_NULL(instance);

    ImgViewerContext context;
    RETURN_IF_FAILED(LoadImgViewerConfig(&context.config));
    context.current_window_opacity_percent = context.config.window_opacity_percent;
    context.current_toolbar_scale_percent = context.config.toolbar_scale_percent;
    context.ui.SetToolbarScalePercent(context.current_toolbar_scale_percent);
    const WindowSizeConfig initial_window_size =
        context.config.remember_window_size ? context.config.window_size : WindowSizeConfig{};
    ImgViewerWindowDelegate window_delegate;
    win32::NativeWindow window;
    RETURN_IF_FAILED(window.Create(
        win32::NativeWindowOptions{
            .instance = instance,
            .class_name = kWindowClassName,
            .title = kImgViewerWindowTitle,
            .style = ImgViewerWindowStyle(context.config.borderless_window),
            .width = initial_window_size.width,
            .height = initial_window_size.height,
            .user_data = &context,
        },
        &window_delegate));
    DragAcceptFiles(window.Hwnd(), TRUE);
    context.main_window_ime_context = util::DisableIme(window.Hwnd());
    context.main_window_ime_enabled = false;
    RETURN_IF_FAILED(ApplyImgViewerWindowFrame(window.Hwnd(), &context, false));

    window.Show(SW_SHOWDEFAULT);
    HWND tooltip = context.tooltip.get();
    RETURN_IF_FAILED(InitializeUiTooltips(window.Hwnd(), &tooltip, context.ui));
    if (context.tooltip.get() != tooltip) {
        context.tooltip.reset(tooltip);
    }

    int argc = 0;
    wil::unique_hlocal command_line_args{reinterpret_cast<HLOCAL>(CommandLineToArgvW(GetCommandLineW(), &argc))};
    RETURN_LAST_ERROR_IF_NULL(command_line_args.get());
    auto** argv = reinterpret_cast<wchar_t**>(command_line_args.get());
    if (argc > 1) {
        LoadImgViewerImageFile(window.Hwnd(), &context, argv[1]);
    }

    MSG message = {};
    while (true) {
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result == -1) {
            RETURN_LAST_ERROR();
        }

        if (result == 0) {
            break;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return S_OK;
}

} // namespace

void SyncImgViewerMainWindowIme(HWND hwnd, ImgViewerContext* context)
{
    if (hwnd == nullptr || context == nullptr) {
        return;
    }

    const bool should_enable = context->edit.IsEditingText() && context->main_window_ime_context != nullptr;
    if (context->main_window_ime_enabled == should_enable) {
        if (should_enable) {
            PositionMainWindowIme(hwnd, context);
        }
        return;
    }

    if (!should_enable) {
        if (HIMC ime = ImmGetContext(hwnd)) {
            ImmNotifyIME(ime, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
            ImmReleaseContext(hwnd, ime);
        }
    }

    util::AssociateImeContext(hwnd, should_enable ? context->main_window_ime_context : nullptr);
    context->main_window_ime_enabled = should_enable;
    if (should_enable) {
        PositionMainWindowIme(hwnd, context);
    }
}

int RunImgViewerApplication()
{
    const HRESULT hr = RunImgViewerApplicationAsHresult();
    return SUCCEEDED(hr) ? 0 : 1;
}
