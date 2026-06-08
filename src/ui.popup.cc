#include "ui.popup.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include <d2d1helper.h>
#include <d3d11.h>
#include <windowsx.h>
#include <wil/result_macros.h>

#include "ui.draw.hpp"

LRESULT CALLBACK PopupWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

namespace {

constexpr wchar_t kPopupWindowClassName[] = L"UiPopupWindow";
constexpr float kMenuBodyFontSize = 8.5f;
constexpr float kMenuIconFontSize = 10.0f;
constexpr float kPopupContentInset = 1.0f;

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

D2D1_SIZE_F PopupWindowSize(D2D1_SIZE_F content_size)
{
    return D2D1::SizeF(
        content_size.width + kPopupContentInset * 2.0f,
        content_size.height + kPopupContentInset * 2.0f);
}

D2D1_POINT_2F PopupWindowOrigin(D2D1_POINT_2F content_origin)
{
    return D2D1::Point2F(content_origin.x - kPopupContentInset, content_origin.y - kPopupContentInset);
}

UiInputEvent OffsetPopupEvent(UiInputEvent event)
{
    if (event.type == UiEventType::PointerMove ||
        event.type == UiEventType::PointerDown ||
        event.type == UiEventType::PointerUp ||
        event.type == UiEventType::PointerLeave ||
        event.type == UiEventType::PointerWheel) {
        event.point.x -= kPopupContentInset;
        event.point.y -= kPopupContentInset;
        event.pointer.point = event.point;
    }
    return event;
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

    D2D1_FACTORY_OPTIONS factory_options = {};
#if defined(_DEBUG)
    factory_options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    RETURN_IF_FAILED(D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        factory_options,
        dcomp_d2d_factory_.put()));
    constexpr UINT device_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    constexpr D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    RETURN_IF_FAILED(D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        device_flags,
        feature_levels,
        ARRAYSIZE(feature_levels),
        D3D11_SDK_VERSION,
        d3d_device_.put(),
        nullptr,
        d3d_context_.put()));
    RETURN_IF_FAILED(d3d_device_->QueryInterface(IID_PPV_ARGS(dxgi_device_.put())));
    RETURN_IF_FAILED(dcomp_d2d_factory_->CreateDevice(dxgi_device_.get(), d2d_device_.put()));
    RETURN_IF_FAILED(d2d_device_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, d2d_context_.put()));
    RETURN_IF_FAILED(DCompositionCreateDevice(
        dxgi_device_.get(),
        __uuidof(IDCompositionDevice),
        reinterpret_cast<void**>(dcomp_device_.put())));

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
    ResetDCompPopupResources();
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
        UiEventResult result = content_->OnInputEvent(OffsetPopupEvent(event));
        if (result.needs_render && !result.close_popup && popup_hwnd_ != nullptr) {
            bool resized = false;
            ResizeNativePopupToContent(&resized);
            if (!resized) {
                RenderNativePopup();
            }
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
    D2D1_POINT_2F window_origin = PopupWindowOrigin(origin);
    window_origin.x *= dpi_scale;
    window_origin.y *= dpi_scale;
    POINT screen_origin = ClientToScreenPoint(owner_, window_origin);
    const D2D1_SIZE_F window_size = PopupWindowSize(size);
    const int width = static_cast<int>(std::ceil(window_size.width * dpi_scale));
    const int height = static_cast<int>(std::ceil(window_size.height * dpi_scale));

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
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_NOREDIRECTIONBITMAP,
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

    RETURN_IF_FAILED(EnsureDCompResources());
    RETURN_IF_FAILED(EnsureDCompSurface(
        static_cast<UINT>((std::max)(1, width)),
        static_cast<UINT>((std::max)(1, height))));

    native_open_ = true;
    RenderNativePopup();
    ShowWindow(popup_hwnd_, SW_SHOWNOACTIVATE);
    return S_OK;
}

HRESULT PopupHost::ResizeNativePopupToContent(bool* resized)
{
    RETURN_HR_IF_NULL(E_POINTER, resized);
    *resized = false;
    RETURN_HR_IF_NULL(E_UNEXPECTED, popup_hwnd_);
    RETURN_HR_IF_NULL(E_UNEXPECTED, content_);

    const UiDrawContext measure_context{
        .dwrite_factory = dwrite_factory_.get(),
        .body_text_format = body_text_format_,
        .icon_text_format = icon_text_format_,
    };
    const D2D1_SIZE_F size = PopupWindowSize(content_->Measure(measure_context, D2D1::SizeF()));
    const float dpi_scale = static_cast<float>(GetDpiForWindow(popup_hwnd_)) / 96.0f;
    const int width = (std::max)(1, static_cast<int>(std::ceil(size.width * dpi_scale)));
    const int height = (std::max)(1, static_cast<int>(std::ceil(size.height * dpi_scale)));

    RECT rect = {};
    RETURN_IF_WIN32_BOOL_FALSE(GetWindowRect(popup_hwnd_, &rect));
    if (rect.right - rect.left == width && rect.bottom - rect.top == height) {
        return S_OK;
    }

    POINT origin{rect.left, rect.top};
    HMONITOR monitor = MonitorFromPoint(origin, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info = {.cbSize = sizeof(monitor_info)};
    if (GetMonitorInfoW(monitor, &monitor_info)) {
        const RECT work_area = monitor_info.rcWork;
        origin.x = (std::min)(origin.x, work_area.right - width);
        origin.y = (std::min)(origin.y, work_area.bottom - height);
        origin.x = (std::max)(origin.x, work_area.left);
        origin.y = (std::max)(origin.y, work_area.top);
    }

    RETURN_IF_FAILED(EnsureDCompSurface(static_cast<UINT>(width), static_cast<UINT>(height)));
    RETURN_IF_FAILED(RenderDCompPopup(static_cast<UINT>(width), static_cast<UINT>(height)));
    RETURN_IF_WIN32_BOOL_FALSE(SetWindowPos(
        popup_hwnd_,
        HWND_TOPMOST,
        origin.x,
        origin.y,
        width,
        height,
        SWP_NOACTIVATE));
    *resized = true;
    return S_OK;
}

HRESULT PopupHost::EnsureDCompResources()
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, popup_hwnd_);
    RETURN_HR_IF_NULL(E_UNEXPECTED, dcomp_device_);
    if (dcomp_target_ != nullptr && dcomp_visual_ != nullptr) {
        return S_OK;
    }

    RETURN_IF_FAILED(dcomp_device_->CreateTargetForHwnd(popup_hwnd_, TRUE, dcomp_target_.put()));
    RETURN_IF_FAILED(dcomp_device_->CreateVisual(dcomp_visual_.put()));
    RETURN_IF_FAILED(dcomp_target_->SetRoot(dcomp_visual_.get()));
    return dcomp_device_->Commit();
}

HRESULT PopupHost::EnsureDCompSurface(UINT width, UINT height)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, dcomp_device_);
    RETURN_HR_IF_NULL(E_UNEXPECTED, dcomp_visual_);

    width = (std::max)(1U, width);
    height = (std::max)(1U, height);
    if (dcomp_surface_ != nullptr && dcomp_surface_width_ == width && dcomp_surface_height_ == height) {
        return S_OK;
    }

    dcomp_surface_.reset();
    RETURN_IF_FAILED(dcomp_device_->CreateSurface(
        width,
        height,
        DXGI_FORMAT_B8G8R8A8_UNORM,
        DXGI_ALPHA_MODE_PREMULTIPLIED,
        dcomp_surface_.put()));
    RETURN_IF_FAILED(dcomp_visual_->SetContent(dcomp_surface_.get()));
    RETURN_IF_FAILED(dcomp_visual_->SetOffsetX(0.0f));
    RETURN_IF_FAILED(dcomp_visual_->SetOffsetY(0.0f));
    dcomp_surface_width_ = width;
    dcomp_surface_height_ = height;
    return dcomp_device_->Commit();
}

void PopupHost::ResetDCompPopupResources()
{
    dcomp_surface_.reset();
    dcomp_visual_.reset();
    dcomp_target_.reset();
    dcomp_surface_width_ = 0;
    dcomp_surface_height_ = 0;
}

void PopupHost::RenderNativePopup()
{
    FAIL_FAST_IF_FAILED(RenderDCompPopup());
}

HRESULT PopupHost::RenderDCompPopup()
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, popup_hwnd_);
    RECT rect = {};
    RETURN_IF_WIN32_BOOL_FALSE(GetClientRect(popup_hwnd_, &rect));
    const UINT width = static_cast<UINT>((std::max)(1L, rect.right - rect.left));
    const UINT height = static_cast<UINT>((std::max)(1L, rect.bottom - rect.top));
    return RenderDCompPopup(width, height);
}

HRESULT PopupHost::RenderDCompPopup(UINT width, UINT height)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, popup_hwnd_);
    RETURN_HR_IF_NULL(E_UNEXPECTED, dcomp_surface_);
    RETURN_HR_IF_NULL(E_UNEXPECTED, d2d_context_);
    RETURN_HR_IF_NULL(E_UNEXPECTED, dcomp_device_);

    width = (std::max)(1U, width);
    height = (std::max)(1U, height);
    RETURN_IF_FAILED(EnsureDCompSurface(width, height));

    wil::com_ptr<IDXGISurface> dxgi_surface;
    POINT offset = {};
    const RECT update_rect = {
        0,
        0,
        static_cast<LONG>(width),
        static_cast<LONG>(height),
    };
    RETURN_IF_FAILED(dcomp_surface_->BeginDraw(
        &update_rect,
        __uuidof(IDXGISurface),
        reinterpret_cast<void**>(dxgi_surface.put()),
        &offset));

    wil::com_ptr<ID2D1Bitmap1> target_bitmap;
    const D2D1_BITMAP_PROPERTIES1 bitmap_properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f,
        96.0f);
    HRESULT hr = d2d_context_->CreateBitmapFromDxgiSurface(dxgi_surface.get(), bitmap_properties, target_bitmap.put());
    if (SUCCEEDED(hr)) {
        const float dpi_scale = static_cast<float>(GetDpiForWindow(popup_hwnd_)) / 96.0f;
        const UiDrawContext draw_context{
            .d2d_context = d2d_context_.get(),
            .dwrite_factory = dwrite_factory_.get(),
            .body_text_format = body_text_format_,
            .icon_text_format = icon_text_format_,
            .viewport_size = D2D1::SizeF(static_cast<float>(width) / dpi_scale, static_cast<float>(height) / dpi_scale),
            .dpi_scale = dpi_scale,
        };

        d2d_context_->SetTarget(target_bitmap.get());
        d2d_context_->BeginDraw();
        d2d_context_->SetTransform(
            D2D1::Matrix3x2F::Scale(dpi_scale, dpi_scale) *
            D2D1::Matrix3x2F::Translation(
                static_cast<float>(offset.x) + kPopupContentInset * dpi_scale,
                static_cast<float>(offset.y) + kPopupContentInset * dpi_scale));
        d2d_context_->Clear(D2D1::ColorF(D2D1::ColorF::Black, 0.0f));
        if (content_ != nullptr) {
            content_->Render(draw_context);
        }
        hr = d2d_context_->EndDraw();
        d2d_context_->SetTarget(nullptr);
    }

    const HRESULT end_surface_result = dcomp_surface_->EndDraw();
    RETURN_IF_FAILED(hr);
    RETURN_IF_FAILED(end_surface_result);
    return dcomp_device_->Commit();
}

void PopupHost::HandlePopupResult(UiEventResult result)
{
    if (result.needs_render && !result.close_popup && popup_hwnd_ != nullptr) {
        bool resized = false;
        ResizeNativePopupToContent(&resized);
        if (!resized) {
            RenderNativePopup();
        }
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
            result = host->content_->OnInputEvent(OffsetPopupEvent(UiInputEvent::Pointer(pointer, hwnd)));
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
