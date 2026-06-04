#include "ui.popup.hpp"

#include <algorithm>

#include <d2d1helper.h>
#include <windowsx.h>
#include <wil/result_macros.h>

#include "ui.draw.hpp"

LRESULT CALLBACK PopupWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

namespace {

constexpr wchar_t kPopupWindowClassName[] = L"UiPopupWindow";

PopupHost* GetPopupHost(HWND hwnd)
{
    return reinterpret_cast<PopupHost*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

HRESULT RegisterPopupWindowClass(HINSTANCE instance)
{
    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = PopupWindowProc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.lpszClassName = kPopupWindowClassName;
    const ATOM atom = RegisterClassExW(&window_class);
    if (atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        RETURN_LAST_ERROR();
    }
    return S_OK;
}

POINT ClientToScreenPoint(HWND hwnd, D2D1_POINT_2F point)
{
    POINT screen_point{static_cast<LONG>(point.x), static_cast<LONG>(point.y)};
    ClientToScreen(hwnd, &screen_point);
    return screen_point;
}

} // namespace

HRESULT PopupHost::Initialize(HWND owner, UINT action_message, ID2D1Factory* d2d_factory, IDWriteFactory* dwrite_factory)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, owner);
    RETURN_HR_IF(E_INVALIDARG, action_message == 0);
    RETURN_HR_IF_NULL(E_INVALIDARG, d2d_factory);
    RETURN_HR_IF_NULL(E_INVALIDARG, dwrite_factory);

    owner_ = owner;
    action_message_ = action_message;
    d2d_factory_ = d2d_factory;
    dwrite_factory_ = dwrite_factory;
    return RegisterPopupWindowClass(reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner_, GWLP_HINSTANCE)));
}

void PopupHost::SetTextFormats(IDWriteTextFormat* body_text_format, IDWriteTextFormat* icon_text_format)
{
    body_text_format_ = body_text_format;
    icon_text_format_ = icon_text_format;
}

bool PopupHost::IsOpen() const
{
    return native_open_ || menu_.IsOpen();
}

void PopupHost::Close()
{
    menu_.Close();
    native_open_ = false;
    native_render_target_.reset();
    if (popup_hwnd_ != nullptr) {
        DestroyWindow(popup_hwnd_);
        popup_hwnd_ = nullptr;
    }
}

HRESULT PopupHost::OpenMenu(D2D1_POINT_2F origin, std::vector<MenuItem> items)
{
    Close();

    MenuOverlay menu;
    menu.Open(origin, std::move(items));
    if (ShouldUseNativeWindow(menu.Bounds())) {
        return OpenNativePopup(menu);
    }

    menu_ = std::move(menu);
    return S_OK;
}

void PopupHost::Draw(const UiDrawContext& context) const
{
    if (!native_open_) {
        menu_.Draw(context, UiElementState{});
    }
}

UiEventResult PopupHost::OnInputEvent(const UiInputEvent& event)
{
    if (native_open_) {
        if (event.type == UiEventType::Cancel || event.type == UiEventType::OwnerDeactivated ||
            (event.type == UiEventType::KeyDown && event.key.virtual_key == VK_ESCAPE)) {
            Close();
            return UiEventResult{.handled = true, .needs_render = true};
        }
        return {};
    }
    return menu_.OnInputEvent(event);
}

UiEventResult PopupHost::OnPointerEvent(const UiPointerEvent& event)
{
    return OnInputEvent(UiInputEvent{.type = event.type, .pointer = event, .point = event.point});
}

UiEventResult PopupHost::OnKeyEvent(const UiKeyEvent& event)
{
    return OnInputEvent(UiInputEvent{.type = event.type, .key = event});
}

bool PopupHost::Contains(D2D1_POINT_2F point) const
{
    return !native_open_ && menu_.Contains(point);
}

bool PopupHost::ShouldUseNativeWindow(D2D1_RECT_F bounds) const
{
    RECT client = {};
    if (!GetClientRect(owner_, &client)) {
        return true;
    }
    return bounds.left < 0.0f || bounds.top < 0.0f ||
        bounds.right > static_cast<float>(client.right - client.left) ||
        bounds.bottom > static_cast<float>(client.bottom - client.top);
}

HRESULT PopupHost::OpenNativePopup(const MenuOverlay& menu)
{
    const D2D1_SIZE_F size = menu.DesiredSize();
    POINT screen_origin = ClientToScreenPoint(owner_, menu.Origin());

    RECT work_area = {};
    HMONITOR monitor = MonitorFromPoint(screen_origin, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info = {.cbSize = sizeof(monitor_info)};
    if (GetMonitorInfoW(monitor, &monitor_info)) {
        work_area = monitor_info.rcWork;
        screen_origin.x = (std::min)(screen_origin.x, work_area.right - static_cast<LONG>(size.width));
        screen_origin.y = (std::min)(screen_origin.y, work_area.bottom - static_cast<LONG>(size.height));
        screen_origin.x = (std::max)(screen_origin.x, work_area.left);
        screen_origin.y = (std::max)(screen_origin.y, work_area.top);
    }

    menu_.Open(D2D1::Point2F(0.0f, 0.0f), menu.Items());
    popup_hwnd_ = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        kPopupWindowClassName,
        L"",
        WS_POPUP,
        screen_origin.x,
        screen_origin.y,
        static_cast<int>(size.width),
        static_cast<int>(size.height),
        owner_,
        nullptr,
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner_, GWLP_HINSTANCE)),
        this);
    RETURN_LAST_ERROR_IF_NULL(popup_hwnd_);

    native_open_ = true;
    ShowWindow(popup_hwnd_, SW_SHOWNOACTIVATE);
    UpdateWindow(popup_hwnd_);
    return S_OK;
}

HRESULT PopupHost::EnsureNativeRenderTarget()
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, popup_hwnd_);
    if (native_render_target_ != nullptr) {
        return S_OK;
    }

    RECT rect = {};
    RETURN_IF_WIN32_BOOL_FALSE(GetClientRect(popup_hwnd_, &rect));
    RETURN_IF_FAILED(d2d_factory_->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(
            popup_hwnd_,
            D2D1::SizeU(
                static_cast<UINT32>((std::max)(1L, rect.right - rect.left)),
                static_cast<UINT32>((std::max)(1L, rect.bottom - rect.top)))),
        native_render_target_.put()));
    return S_OK;
}

void PopupHost::RenderNativePopup()
{
    if (FAILED(EnsureNativeRenderTarget())) {
        return;
    }
    const UiDrawContext draw_context{
        .d2d_context = native_render_target_.get(),
        .dwrite_factory = dwrite_factory_.get(),
        .body_text_format = body_text_format_,
        .icon_text_format = icon_text_format_,
    };

    native_render_target_->BeginDraw();
    native_render_target_->Clear(D2D1::ColorF(D2D1::ColorF::Black, 0.0f));

    menu_.Draw(draw_context, UiElementState{});

    if (native_render_target_->EndDraw() == D2DERR_RECREATE_TARGET) {
        native_render_target_.reset();
    }
}

void PopupHost::ForwardAction(UiAction action)
{
    if (action != kUiActionNone) {
        PostMessageW(owner_, action_message_, static_cast<WPARAM>(UiActionValue(action)), 0);
    }
}

LRESULT CALLBACK PopupWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_NCCREATE: {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return TRUE;
    }
    case WM_PAINT: {
        PAINTSTRUCT paint = {};
        BeginPaint(hwnd, &paint);
        if (PopupHost* host = GetPopupHost(hwnd)) {
            host->RenderNativePopup();
        }
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP: {
        PopupHost* host = GetPopupHost(hwnd);
        if (host == nullptr) {
            break;
        }
        const UiEventType type = message == WM_MOUSEMOVE
            ? UiEventType::PointerMove
            : message == WM_LBUTTONDOWN ? UiEventType::PointerDown : UiEventType::PointerUp;
        UiPointerEvent pointer{
            .type = type,
            .point = D2D1::Point2F(static_cast<float>(GET_X_LPARAM(lparam)), static_cast<float>(GET_Y_LPARAM(lparam))),
            .button = message == WM_MOUSEMOVE ? UiPointerButton::None : UiPointerButton::Left,
        };
        UiEventResult result = host->menu_.OnInputEvent(UiInputEvent{.type = type, .pointer = pointer, .point = pointer.point});
        if (result.needs_render) {
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        if (result.action != kUiActionNone) {
            host->ForwardAction(result.action);
            host->Close();
        }
        return 0;
    }
    case WM_KEYDOWN: {
        PopupHost* host = GetPopupHost(hwnd);
        if (host != nullptr) {
            const UiEventResult result = host->OnInputEvent(UiInputEvent{
                .type = UiEventType::KeyDown,
                .key = UiKeyEvent{.type = UiEventType::KeyDown, .virtual_key = static_cast<UINT>(wparam)},
            });
            if (result.needs_render) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            if (result.handled) {
                return 0;
            }
        }
        if (wparam == VK_ESCAPE) {
            return 0;
        }
        break;
    }
    case WM_ACTIVATE:
        if (LOWORD(wparam) == WA_INACTIVE) {
            if (PopupHost* host = GetPopupHost(hwnd)) {
                host->Close();
            }
        }
        return 0;
    case WM_DESTROY:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}
