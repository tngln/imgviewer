#include "ui.popup.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include <d2d1helper.h>
#include <windowsx.h>
#include <wil/result_macros.h>

#include "ui.draw.hpp"

LRESULT CALLBACK PopupWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

namespace {

constexpr wchar_t kPopupWindowClassName[] = L"UiPopupWindow";
constexpr float kMenuCornerRadius = 3.0f;
constexpr float kMenuBodyFontSize = 8.5f;
constexpr float kMenuIconFontSize = 10.0f;

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

class MenuPopupContent final : public UiPopupContent {
public:
    MenuPopupContent(
        std::vector<MenuItem> items,
        const UiDrawContext& context,
        IDWriteTextFormat* body_text_format,
        IDWriteTextFormat* icon_text_format) :
        body_text_format_(body_text_format),
        icon_text_format_(icon_text_format)
    {
        menu_.Open(D2D1::Point2F(0.0f, 0.0f), std::move(items));
        menu_.UpdatePreferredWidth(MenuTextContext(context));
    }

    float CornerRadius() const override
    {
        return kMenuCornerRadius;
    }

    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F) const override
    {
        return menu_.Measure(MenuTextContext(context), context.viewport_size);
    }

    void Render(const UiDrawContext& context) const override
    {
        menu_.Render(MenuTextContext(context), UiRootState{});
    }

    UiEventResult OnInputEvent(const UiInputEvent& event) override
    {
        UiEventResult result = menu_.OnInputEvent(event);
        if (result.action != kUiActionNone || !menu_.IsOpen()) {
            result.close_popup = true;
        }
        return result;
    }

private:
    UiDrawContext MenuTextContext(const UiDrawContext& context) const
    {
        UiDrawContext menu_context = context;
        menu_context.body_text_format = body_text_format_;
        menu_context.icon_text_format = icon_text_format_;
        return menu_context;
    }

    MenuOverlay menu_;
    IDWriteTextFormat* body_text_format_ = nullptr;
    IDWriteTextFormat* icon_text_format_ = nullptr;
};

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
    RETURN_IF_FAILED(dwrite_factory_->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        kMenuBodyFontSize,
        L"",
        menu_body_text_format_.put()));
    RETURN_IF_FAILED(dwrite_factory_->CreateTextFormat(
        L"Segoe MDL2 Assets",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        kMenuIconFontSize,
        L"",
        menu_icon_text_format_.put()));
    RETURN_IF_FAILED(menu_body_text_format_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
    RETURN_IF_FAILED(menu_icon_text_format_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
    return RegisterPopupWindowClass(reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner_, GWLP_HINSTANCE)));
}

void PopupHost::SetTextFormats(IDWriteTextFormat* body_text_format, IDWriteTextFormat* icon_text_format)
{
    body_text_format_ = body_text_format;
    icon_text_format_ = icon_text_format;
}

bool PopupHost::IsOpen() const
{
    return native_open_;
}

void PopupHost::Close()
{
    if (content_ != nullptr) {
        content_->OnClosed();
        content_.reset();
    }
    native_open_ = false;
    native_render_target_.reset();
    if (popup_hwnd_ != nullptr) {
        DestroyWindow(popup_hwnd_);
        popup_hwnd_ = nullptr;
    }
}

HRESULT PopupHost::Open(D2D1_POINT_2F origin, std::unique_ptr<UiPopupContent> content)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, content);
    Close();

    const UiDrawContext measure_context{
        .dwrite_factory = dwrite_factory_.get(),
        .body_text_format = body_text_format_,
        .icon_text_format = icon_text_format_,
    };
    const D2D1_SIZE_F size = content->Measure(measure_context, D2D1::SizeF());
    content_ = std::move(content);
    return OpenNativePopup(origin, size);
}

HRESULT PopupHost::OpenMenu(D2D1_POINT_2F origin, std::vector<MenuItem> items)
{
    const UiDrawContext measure_context{
        .dwrite_factory = dwrite_factory_.get(),
        .body_text_format = MenuBodyTextFormat(),
        .icon_text_format = MenuIconTextFormat(),
    };
    return Open(
        origin,
        std::make_unique<MenuPopupContent>(
            std::move(items),
            measure_context,
            MenuBodyTextFormat(),
            MenuIconTextFormat()));
}

void PopupHost::Render(const UiDrawContext& context) const
{
    UNREFERENCED_PARAMETER(context);
}

UiEventResult PopupHost::OnInputEvent(const UiInputEvent& event)
{
    if (!native_open_) {
        return {};
    }

    if (event.type == UiEventType::Cancel || event.type == UiEventType::OwnerDeactivated ||
        (event.type == UiEventType::KeyDown && event.key.virtual_key == VK_ESCAPE)) {
        Close();
        return UiEventResult{.handled = true, .needs_render = true};
    }

    if (event.type == UiEventType::PointerDown && event.hwnd == owner_) {
        Close();
        return UiEventResult{.needs_render = true};
    }

    if (content_ != nullptr) {
        UiEventResult result = content_->OnInputEvent(event);
        if (result.needs_render && popup_hwnd_ != nullptr) {
            InvalidateRect(popup_hwnd_, nullptr, FALSE);
        }
        return result;
    }

    return {};
}

UiEventResult PopupHost::OnPointerEvent(const UiPointerEvent& event)
{
    return OnInputEvent(UiInputEvent::Pointer(event, nullptr));
}

UiEventResult PopupHost::OnKeyEvent(const UiKeyEvent& event)
{
    return OnInputEvent(UiInputEvent::Key(event, nullptr));
}

bool PopupHost::Contains(D2D1_POINT_2F) const
{
    return false;
}

HRESULT PopupHost::OpenNativePopup(D2D1_POINT_2F origin, D2D1_SIZE_F size)
{
    const float dpi_scale = static_cast<float>(GetDpiForWindow(owner_)) / 96.0f;
    origin.x *= dpi_scale;
    origin.y *= dpi_scale;
    POINT screen_origin = ClientToScreenPoint(owner_, origin);
    const int width = static_cast<int>(std::ceil(size.width * dpi_scale));
    const int height = static_cast<int>(std::ceil(size.height * dpi_scale));

    RECT work_area = {};
    HMONITOR monitor = MonitorFromPoint(screen_origin, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info = {.cbSize = sizeof(monitor_info)};
    if (GetMonitorInfoW(monitor, &monitor_info)) {
        work_area = monitor_info.rcWork;
        screen_origin.x = (std::min)(screen_origin.x, work_area.right - width);
        screen_origin.y = (std::min)(screen_origin.y, work_area.bottom - height);
        screen_origin.x = (std::max)(screen_origin.x, work_area.left);
        screen_origin.y = (std::max)(screen_origin.y, work_area.top);
    }

    popup_hwnd_ = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        kPopupWindowClassName,
        L"",
        WS_POPUP,
        screen_origin.x,
        screen_origin.y,
        width,
        height,
        owner_,
        nullptr,
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner_, GWLP_HINSTANCE)),
        this);
    RETURN_LAST_ERROR_IF_NULL(popup_hwnd_);

    if (content_ != nullptr && content_->CornerRadius() > 0.0f) {
        const int diameter = static_cast<int>(std::ceil(content_->CornerRadius() * 2.0f * dpi_scale));
        HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, diameter, diameter);
        RETURN_LAST_ERROR_IF_NULL(region);
        if (SetWindowRgn(popup_hwnd_, region, FALSE) == 0) {
            DeleteObject(region);
            RETURN_LAST_ERROR();
        }
    }

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
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(),
            96.0f,
            96.0f),
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
    RECT rect = {};
    GetClientRect(popup_hwnd_, &rect);
    const float dpi_scale = static_cast<float>(GetDpiForWindow(popup_hwnd_)) / 96.0f;
    const UiDrawContext draw_context{
        .d2d_context = native_render_target_.get(),
        .dwrite_factory = dwrite_factory_.get(),
        .body_text_format = body_text_format_,
        .icon_text_format = icon_text_format_,
        .viewport_size = D2D1::SizeF(
            static_cast<float>((std::max)(1L, rect.right - rect.left)) / dpi_scale,
            static_cast<float>((std::max)(1L, rect.bottom - rect.top)) / dpi_scale),
        .dpi_scale = dpi_scale,
    };

    native_render_target_->BeginDraw();
    native_render_target_->SetTransform(D2D1::Matrix3x2F::Scale(dpi_scale, dpi_scale));
    native_render_target_->Clear(D2D1::ColorF(D2D1::ColorF::Black, 0.0f));

    if (content_ != nullptr) {
        content_->Render(draw_context);
    }

    if (native_render_target_->EndDraw() == D2DERR_RECREATE_TARGET) {
        native_render_target_.reset();
    }
}

void PopupHost::HandlePopupResult(UiEventResult result)
{
    if (result.needs_render && popup_hwnd_ != nullptr) {
        InvalidateRect(popup_hwnd_, nullptr, FALSE);
    }
    if (result.action != kUiActionNone) {
        ForwardAction(result.action, result.effect_target);
    } else if (result.effect_target != UiElementId::None) {
        PostMessageW(owner_, action_message_, 0, static_cast<LPARAM>(UiElementIdValue(result.effect_target)));
    }
    if (result.close_popup) {
        Close();
    }
}

void PopupHost::ForwardAction(UiAction action, UiElementId effect_target)
{
    if (action != kUiActionNone) {
        PostMessageW(
            owner_,
            action_message_,
            static_cast<WPARAM>(UiActionValue(action)),
            static_cast<LPARAM>(UiElementIdValue(effect_target)));
    }
}

IDWriteTextFormat* PopupHost::MenuBodyTextFormat() const
{
    return menu_body_text_format_ != nullptr ? menu_body_text_format_.get() : body_text_format_;
}

IDWriteTextFormat* PopupHost::MenuIconTextFormat() const
{
    return menu_icon_text_format_ != nullptr ? menu_icon_text_format_.get() : icon_text_format_;
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
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP: {
        PopupHost* host = GetPopupHost(hwnd);
        if (host == nullptr) {
            break;
        }
        const float dpi_scale2 = static_cast<float>(GetDpiForWindow(hwnd)) / 96.0f;
        const UiEventType type = message == WM_MOUSEMOVE
            ? UiEventType::PointerMove
            : message == WM_LBUTTONDOWN ? UiEventType::PointerDown : UiEventType::PointerUp;
        UiPointerEvent pointer{
            .type = type,
            .point = D2D1::Point2F(static_cast<float>(GET_X_LPARAM(lparam)) / dpi_scale2,
                                   static_cast<float>(GET_Y_LPARAM(lparam)) / dpi_scale2),
            .button = message == WM_MOUSEMOVE ? UiPointerButton::None : UiPointerButton::Left,
            .popup_host = host,
        };
        UiEventResult result = {};
        if (host->content_ != nullptr) {
            result = host->content_->OnInputEvent(UiInputEvent::Pointer(pointer, hwnd));
        }
        host->HandlePopupResult(result);
        return 0;
    }
    case WM_KEYDOWN: {
        PopupHost* host = GetPopupHost(hwnd);
        if (host != nullptr) {
            const UiKeyEvent key{
                .type = UiEventType::KeyDown,
                .virtual_key = static_cast<UINT>(wparam),
                .modifiers = UiModifiers::Current(),
                .popup_host = host,
            };
            const UiEventResult result = host->OnInputEvent(UiInputEvent::Key(key, hwnd));
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
