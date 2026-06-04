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
    return reinterpret_cast<ImgViewerContext*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

bool IsKeyDown(int virtual_key)
{
    return (GetKeyState(virtual_key) & 0x8000) != 0;
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

UiModifiers CurrentUiModifiers()
{
    return UiModifiers{
        .ctrl = IsKeyDown(VK_CONTROL),
        .shift = IsKeyDown(VK_SHIFT),
        .alt = IsKeyDown(VK_MENU),
    };
}

ImgViewerAction ActionForKeyboardMessage(const ImgViewerContext* context, WPARAM wparam)
{
    if (context == nullptr) {
        return ImgViewerAction::None;
    }

    return ActionForKey(
        context->config.action_bindings,
        static_cast<UINT>(wparam),
        IsKeyDown(VK_CONTROL),
        IsKeyDown(VK_SHIFT),
        IsKeyDown(VK_MENU));
}

size_t KeyActionIndex(WPARAM wparam)
{
    return static_cast<size_t>(static_cast<UINT>(wparam) & 0xFF);
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

    if (ImgViewerActionFromUiAction(result.action) == ImgViewerAction::OpenImage) {
        HandleImgViewerOpenImageCommand(hwnd, context);
    } else if (result.action != kUiActionNone) {
        ExecuteImgViewerAction(hwnd, context, ImgViewerActionFromUiAction(result.action));
    }
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
        ExecuteImgViewerAction(hwnd, GetImgViewerContext(hwnd), ImgViewerActionFromUiAction(UiAction(static_cast<int>(wparam))));
        return 0;
    }

    case kImgViewerOpenImageMessage: {
        HandleImgViewerOpenImageCommand(hwnd, GetImgViewerContext(hwnd));
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
            FAILED(context->viewer.Initialize()) ||
            FAILED(CreateUiAccessibilityProvider(
                hwnd,
                kImgViewerUiActionMessage,
                &context->ui,
                context->accessibility_provider.put()))) {
            return -1;
        }
        context->viewer.SetPixelatedSampling(context->config.pixelated_sampling);
        ApplyWindowOpacity(hwnd, context->current_window_opacity_percent);
        if (FAILED(RenderImgViewer(context))) {
            return -1;
        }

        return 0;
    }

    case WM_SIZE: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
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
            UiPointerEvent pointer{.type = UiEventType::PointerMove, .point = point, .modifiers = CurrentUiModifiers()};
            RenderIfNeeded(hwnd, context, context->ui.OnInputEvent(UiInputEvent{.type = pointer.type, .pointer = pointer, .point = point, .hwnd = hwnd}));
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        const D2D1_POINT_2F point = GetPointerPoint(hwnd, lparam);
        UiPointerEvent pointer{.type = UiEventType::PointerDown, .point = point, .button = UiPointerButton::Left, .modifiers = CurrentUiModifiers()};
        UiEventResult ui_result = context != nullptr
            ? context->ui.OnInputEvent(UiInputEvent{.type = pointer.type, .pointer = pointer, .point = point, .hwnd = hwnd})
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
            UiPointerEvent pointer{.type = UiEventType::PointerUp, .point = point, .button = UiPointerButton::Left, .modifiers = CurrentUiModifiers()};
            ui_result = context->ui.OnInputEvent(UiInputEvent{.type = pointer.type, .pointer = pointer, .point = point, .hwnd = hwnd});
            RenderIfNeeded(hwnd, context, ui_result);
        }
        return (ui_result.handled || viewer_result.handled) ? 0 : DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_MOUSELEAVE: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        UiPointerEvent pointer{.type = UiEventType::PointerLeave, .modifiers = CurrentUiModifiers()};
        RenderIfNeeded(
            hwnd,
            context,
            context != nullptr ? context->ui.OnInputEvent(UiInputEvent{.type = pointer.type, .pointer = pointer, .hwnd = hwnd}) : UiEventResult{});
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
        if (context != nullptr && IsKeyDown('O')) {
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
            .modifiers = CurrentUiModifiers(),
            .repeat = (lparam & 0x40000000) != 0,
        };
        const UiEventResult ui_result = context != nullptr
            ? context->ui.OnInputEvent(UiInputEvent{.type = key.type, .key = key, .hwnd = hwnd})
            : UiEventResult{};
        if (ui_result.handled) {
            RenderIfNeeded(hwnd, context, ui_result);
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
            .modifiers = CurrentUiModifiers(),
        };
        const UiEventResult ui_result = context != nullptr
            ? context->ui.OnInputEvent(UiInputEvent{.type = key.type, .key = key, .hwnd = hwnd})
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
        SaveWindowSize(hwnd, GetImgViewerContext(hwnd));
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

HRESULT RegisterMainWindowClass(HINSTANCE instance)
{
    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.lpszClassName = kWindowClassName;

    const ATOM window_class_atom = RegisterClassExW(&window_class);
    RETURN_LAST_ERROR_IF(window_class_atom == 0);

    return S_OK;
}

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

    RETURN_IF_FAILED(RegisterMainWindowClass(instance));

    ImgViewerContext context;
    RETURN_IF_FAILED(LoadImgViewerConfig(&context.config));
    context.current_window_opacity_percent = context.config.window_opacity_percent;
    const WindowSizeConfig initial_window_size =
        context.config.remember_window_size ? context.config.window_size : WindowSizeConfig{};
    wil::unique_hwnd window{CreateWindowExW(
        0,
        kWindowClassName,
        kImgViewerWindowTitle,
        ImgViewerWindowStyle(context.config.borderless_window),
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        initial_window_size.width,
        initial_window_size.height,
        nullptr,
        nullptr,
        instance,
        &context)};
    RETURN_LAST_ERROR_IF_NULL(window.get());
    DragAcceptFiles(window.get(), TRUE);
    util::DisableIme(window.get());
    RETURN_IF_FAILED(ApplyImgViewerWindowFrame(window.get(), &context, false));

    ShowWindow(window.get(), SW_SHOWDEFAULT);
    RETURN_IF_WIN32_BOOL_FALSE(UpdateWindow(window.get()));
    HWND tooltip = context.tooltip.get();
    RETURN_IF_FAILED(InitializeUiTooltips(window.get(), &tooltip, context.ui));
    if (context.tooltip.get() != tooltip) {
        context.tooltip.reset(tooltip);
    }

    int argc = 0;
    wil::unique_hlocal command_line_args{reinterpret_cast<HLOCAL>(CommandLineToArgvW(GetCommandLineW(), &argc))};
    RETURN_LAST_ERROR_IF_NULL(command_line_args.get());
    auto** argv = reinterpret_cast<wchar_t**>(command_line_args.get());
    if (argc > 1) {
        LoadImgViewerImageFile(window.get(), &context, argv[1]);
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
