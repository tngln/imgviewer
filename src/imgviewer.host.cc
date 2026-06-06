#include "imgviewer.host.hpp"

#include "imgviewer.hpp"
#include "imgviewer.action.hpp"
#include "imgviewer.config.hpp"
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

#include <commctrl.h>
#include <shellapi.h>
#include <vector>

#include <wil/resource.h>
#include <wil/result_macros.h>

namespace {

constexpr wchar_t kWindowClassName[] = L"ImgViewerWindow";

D2D1_POINT_2F GetPointerPoint(HWND hwnd, LPARAM lparam)
{
    const POINT point{
        GET_X_LPARAM(lparam),
        GET_Y_LPARAM(lparam),
    };
    return math::CoordinateSpace::FromWindow(hwnd).PhysicalToRender(point);
}

D2D1_POINT_2F GetScreenPointerPoint(HWND hwnd, LPARAM lparam)
{
    POINT point{
        GET_X_LPARAM(lparam),
        GET_Y_LPARAM(lparam),
    };
    ScreenToClient(hwnd, &point);
    return math::CoordinateSpace::FromWindow(hwnd).PhysicalToRender(point);
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

void DispatchUiAction(HWND hwnd, ImgViewerContext* context, UiAction action)
{
    if (context == nullptr || action == kUiActionNone) {
        return;
    }

    if (context->ui.Root() != nullptr && context->ui.Root()->HandleUiAction(action, &context->popup)) {
        RenderImgViewer(context);
        return;
    }

    const ImgViewerAction viewer_action = ImgViewerActionFromUiAction(action);
    if (viewer_action == ImgViewerAction::OpenImage) {
        HandleImgViewerOpenImageCommand(hwnd, context);
        return;
    }

    ExecuteImgViewerAction(hwnd, context, viewer_action);
}

void RenderIfNeeded(HWND hwnd, ImgViewerContext* context, UiEventResult result)
{
    if (context == nullptr) {
        return;
    }

    if (result.capture == UiCaptureRequest::Capture) {
        SetCapture(hwnd);
    } else if (result.capture == UiCaptureRequest::Release) {
        ReleaseCapture();
    }

    if (result.needs_render) {
        RenderImgViewer(context);
    }

    DispatchUiAction(hwnd, context, result.action);
}

void RenderIfNeeded(ImgViewerContext* context, ImgViewerEventResult result)
{
    if (context == nullptr) {
        return;
    }

    if (result.released_capture) {
        ReleaseCapture();
    }

    if (result.needs_render) {
        RenderImgViewer(context);
    }
}

void ClosePopup(ImgViewerContext* context)
{
    if (context != nullptr && context->popup.IsOpen()) {
        context->popup.Close();
    }
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
    const D2D1_POINT_2F render_point = math::CoordinateSpace::FromWindow(hwnd).PhysicalToRender(client_point);
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

    case WM_MOVE:
        ClosePopup(GetImgViewerContext(hwnd));
        return DefWindowProcW(hwnd, message, wparam, lparam);

    case WM_ACTIVATE:
        if (LOWORD(wparam) == WA_INACTIVE) {
            ImgViewerContext* context = GetImgViewerContext(hwnd);
            ClosePopup(context);
            if (context != nullptr) {
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
        ImgViewerEventResult viewer_result = {};
        if (context != nullptr) {
            viewer_result = context->viewer.OnPointerMove(point.x, point.y, context->renderer.ViewportPixelSize());
        }
        RenderIfNeeded(context, viewer_result);
        if (context != nullptr && !viewer_result.handled) {
            UiPointerEvent pointer{
                .type = UiEventType::PointerMove,
                .point = point,
                .modifiers = UiModifiers::Current(),
                .popup_host = &context->popup,
            };
            if (context->popup.IsOpen()) {
                UiEventResult popup_result = context->popup.OnInputEvent(UiInputEvent{
                    .type = pointer.type,
                    .pointer = pointer,
                    .point = point,
                    .hwnd = hwnd,
                    .popup_host = &context->popup,
                });
                RenderIfNeeded(hwnd, context, popup_result);
                if (popup_result.handled) {
                    return 0;
                }
            }
            RenderIfNeeded(hwnd, context, context->ui.OnInputEvent(UiInputEvent{
                .type = pointer.type,
                .pointer = pointer,
                .point = point,
                .hwnd = hwnd,
                .popup_host = &context->popup,
            }));
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        const D2D1_POINT_2F point = GetPointerPoint(hwnd, lparam);
        UiPointerEvent pointer{
            .type = UiEventType::PointerDown,
            .point = point,
            .button = UiPointerButton::Left,
            .modifiers = UiModifiers::Current(),
            .popup_host = context != nullptr ? &context->popup : nullptr,
        };
        if (context != nullptr && context->popup.IsOpen()) {
            UiEventResult popup_result = context->popup.OnInputEvent(UiInputEvent{
                .type = pointer.type,
                .pointer = pointer,
                .point = point,
                .hwnd = hwnd,
                .popup_host = &context->popup,
            });
            RenderIfNeeded(hwnd, context, popup_result);
            if (popup_result.handled) {
                return 0;
            }
        }
        UiEventResult ui_result = context != nullptr
            ? context->ui.OnInputEvent(UiInputEvent{
                .type = pointer.type,
                .pointer = pointer,
                .point = point,
                .hwnd = hwnd,
                .popup_host = &context->popup,
            })
            : UiEventResult{};
        ImgViewerEventResult viewer_result = {};
        if (context != nullptr && !ui_result.handled) {
            if (HandleImgViewerColorPick(hwnd, context, point)) {
                ui_result.handled = true;
            } else {
                viewer_result = context->viewer.OnPointerDown(point.x, point.y, context->renderer.ViewportPixelSize());
            }
        }
        if (viewer_result.captured) {
            SetCapture(hwnd);
        }
        RenderIfNeeded(hwnd, context, ui_result);
        RenderIfNeeded(context, viewer_result);
        return (ui_result.handled || viewer_result.handled) ? 0 : DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_LBUTTONUP: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        const D2D1_POINT_2F point = GetPointerPoint(hwnd, lparam);
        ImgViewerEventResult viewer_result = {};
        if (context != nullptr) {
            viewer_result = context->viewer.OnPointerUp(point.x, point.y, context->renderer.ViewportPixelSize());
        }
        RenderIfNeeded(context, viewer_result);
        UiEventResult ui_result = {};
        if (context != nullptr && !viewer_result.handled) {
            UiPointerEvent pointer{
                .type = UiEventType::PointerUp,
                .point = point,
                .button = UiPointerButton::Left,
                .modifiers = UiModifiers::Current(),
                .popup_host = &context->popup,
            };
            if (context->popup.IsOpen()) {
                UiEventResult popup_result = context->popup.OnInputEvent(UiInputEvent{
                    .type = pointer.type,
                    .pointer = pointer,
                    .point = point,
                    .hwnd = hwnd,
                    .popup_host = &context->popup,
                });
                RenderIfNeeded(hwnd, context, popup_result);
                if (popup_result.handled) {
                    return 0;
                }
            }
            ui_result = context->ui.OnInputEvent(UiInputEvent{
                .type = pointer.type,
                .pointer = pointer,
                .point = point,
                .hwnd = hwnd,
                .popup_host = &context->popup,
            });
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
            UiPointerEvent pointer{
                .type = UiEventType::PointerWheel,
                .point = point,
                .wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam),
                .modifiers = UiModifiers::Current(),
                .popup_host = &context->popup,
            };
            const UiEventResult ui_result = context->ui.OnInputEvent(UiInputEvent{
                .type = pointer.type,
                .pointer = pointer,
                .point = point,
                .hwnd = hwnd,
                .popup_host = &context->popup,
            });
            if (ui_result.handled) {
                RenderIfNeeded(hwnd, context, ui_result);
                return 0;
            }
        }
        if (context != nullptr &&
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
            UiEventResult popup_result = context->popup.OnInputEvent(UiInputEvent{
                .type = key.type,
                .key = key,
                .hwnd = hwnd,
                .popup_host = &context->popup,
            });
            RenderIfNeeded(hwnd, context, popup_result);
            if (popup_result.handled) {
                return 0;
            }
        }
        const UiEventResult ui_result = context != nullptr
            ? context->ui.OnInputEvent(UiInputEvent{
                .type = key.type,
                .key = key,
                .hwnd = hwnd,
                .popup_host = &context->popup,
            })
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
        if (context != nullptr && context->viewer.OnActionDown(action)) {
            return 0;
        }
        if (action == ImgViewerAction::OpenImage) {
            HandleImgViewerOpenImageCommand(hwnd, context);
            return 0;
        }
        if (action != ImgViewerAction::None) {
            ExecuteImgViewerAction(hwnd, context, action);
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
        const UiEventResult ui_result = context != nullptr
            ? context->ui.OnInputEvent(UiInputEvent{
                .type = key.type,
                .key = key,
                .hwnd = hwnd,
                .popup_host = &context->popup,
            })
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
            context != nullptr ? context->viewer.OnActionUp(action) : ImgViewerEventResult{};
        if (viewer_result.handled) {
            RenderIfNeeded(context, viewer_result);
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
        if (ImgViewerContext* context = GetImgViewerContext(hwnd)) {
            context->popup.Close();
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
    util::DisableIme(window.Hwnd());
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

int RunImgViewerApplication()
{
    const HRESULT hr = RunImgViewerApplicationAsHresult();
    return SUCCEEDED(hr) ? 0 : 1;
}
