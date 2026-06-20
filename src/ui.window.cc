#include "ui.window.hpp"

#include <memory>

#include <d2d1helper.h>
#include <windowsx.h>
#include <wil/com.h>
#include <wil/result_macros.h>

#include "ui.common_window.hpp"
#include "math.hpp"
#include "ui.host_effects.hpp"
#include "ui.host_ime.hpp"
#include "ui.host_input.hpp"
#include "ui.host_popup.hpp"

namespace {

using ui_host_input::ClientPixelSize;
using ui_host_input::PhysicalClientPointToUi;
using ui_host_input::ScreenPointToUi;
using ui_host_input::UiPointToPhysicalClient;

} // namespace

HRESULT UiWindowHost::Create(
    UiWindowOptions options,
    std::unique_ptr<ScriptView> root,
    UiWindowDelegate* delegate,
    GraphicsDevice* graphics)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, root);
    RETURN_HR_IF_NULL(E_INVALIDARG, delegate);
    RETURN_HR_IF_NULL(E_INVALIDARG, graphics);
    RETURN_HR_IF(E_INVALIDARG, options.action_message == 0);

    options_ = options;
    delegate_ = delegate;
    graphics_ = graphics;
    root_ = std::move(root);
    return window_.Create(options_.native, this);
}

void UiWindowHost::ResetRoot(std::unique_ptr<ScriptView> root)
{
    if (options_.enable_popup) {
        ClosePopupIfOpen(&popup_);
    }
    root_ = std::move(root);
    Invalidate();
}

void UiWindowHost::Invalidate()
{
    RequestWindowRender(window_.Hwnd());
}

void UiWindowHost::Close()
{
    window_.Destroy();
}

HWND UiWindowHost::Hwnd() const
{
    return window_.Hwnd();
}

win32::NativeWindow& UiWindowHost::Window()
{
    return window_;
}

PopupHost& UiWindowHost::Popup()
{
    return popup_;
}

IDWriteFactory* UiWindowHost::DWriteFactory() const
{
    return dwrite_factory_.get();
}

win32::WindowMessageResult UiWindowHost::OnWindowMessage(
    win32::NativeWindow&,
    UINT message,
    WPARAM wparam,
    LPARAM lparam)
{
    if (message == options_.action_message) {
        const UiPostedActionMessage posted = DecodeUiPostedActionMessage(wparam, lparam);
        ExecuteAction(posted.action);
        return win32::WindowMessageResult::Handled();
    }

    switch (message) {
    case WM_CREATE:
        if (FAILED(InitializeRenderResources()) ||
            (options_.enable_popup &&
                FAILED(popup_.Initialize(
                    window_.Hwnd(),
                    options_.action_message,
                    graphics_,
                    options_.script_engine))) ||
            FAILED(delegate_->OnCreate(*this))) {
            return win32::WindowMessageResult::Handled(-1);
        }
        return win32::WindowMessageResult::Handled();
    case WM_ENTERSIZEMOVE:
    case WM_MOVE:
        if (options_.enable_popup) {
            ClosePopupIfOpen(&popup_);
        }
        break;
    case WM_SIZE:
        if (options_.enable_popup) {
            ClosePopupIfOpen(&popup_);
        }
        if (wparam != SIZE_MINIMIZED) {
            Render();
        }
        Invalidate();
        return win32::WindowMessageResult::Handled();
    case WM_DPICHANGED: {
        if (options_.enable_popup) {
            ClosePopupIfOpen(&popup_);
        }
        const auto* suggested_rect = reinterpret_cast<const RECT*>(lparam);
        if (suggested_rect != nullptr) {
            SetWindowPos(
                window_.Hwnd(),
                nullptr,
                suggested_rect->left,
                suggested_rect->top,
                suggested_rect->right - suggested_rect->left,
                suggested_rect->bottom - suggested_rect->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
        Invalidate();
        return win32::WindowMessageResult::Handled();
    }
    case WM_PAINT: {
        PAINTSTRUCT paint = {};
        BeginPaint(window_.Hwnd(), &paint);
        Render();
        EndPaint(window_.Hwnd(), &paint);
        return win32::WindowMessageResult::Handled();
    }
    case WM_ERASEBKGND:
        return win32::WindowMessageResult::Handled(1);
    case WM_MOUSEMOVE: {
        TRACKMOUSEEVENT track = {.cbSize = sizeof(track), .dwFlags = TME_LEAVE, .hwndTrack = window_.Hwnd()};
        TrackMouseEvent(&track);
        const D2D1_POINT_2F point = PhysicalClientPointToUi(window_.Hwnd(), lparam);
        UiPointerEvent pointer{
            .type = UiEventType::PointerMove,
            .point = point,
            .modifiers = CurrentModifiers(),
            .popup_host = options_.enable_popup ? &popup_ : nullptr,
        };
        DispatchInputEvent(UiInputEvent::Pointer(pointer, window_.Hwnd()));
        PositionIme();
        return win32::WindowMessageResult::Handled();
    }
    case WM_MOUSELEAVE: {
        UiPointerEvent pointer{.type = UiEventType::PointerLeave, .modifiers = CurrentModifiers()};
        HandleUiResult(root_ != nullptr ? root_->OnInputEvent(UiInputEvent::Pointer(pointer, window_.Hwnd())) : UiEventResult{});
        return win32::WindowMessageResult::Handled();
    }
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP: {
        SetFocus(window_.Hwnd());
        const UiEventType type = message == WM_LBUTTONDOWN ? UiEventType::PointerDown : UiEventType::PointerUp;
        const D2D1_POINT_2F point = PhysicalClientPointToUi(window_.Hwnd(), lparam);
        UiPointerEvent pointer{
            .type = type,
            .point = point,
            .button = UiPointerButton::Left,
            .modifiers = CurrentModifiers(),
            .popup_host = options_.enable_popup ? &popup_ : nullptr,
        };
        DispatchInputEvent(UiInputEvent::Pointer(pointer, window_.Hwnd()));
        PositionIme();
        return win32::WindowMessageResult::Handled();
    }
    case WM_MOUSEWHEEL: {
        POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        UiPointerEvent pointer{
            .type = UiEventType::PointerWheel,
            .point = ScreenPointToUi(window_.Hwnd(), point),
            .wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam),
            .modifiers = CurrentModifiers(),
            .popup_host = options_.enable_popup ? &popup_ : nullptr,
        };
        UiEventResult result = DispatchInputEvent(UiInputEvent::Pointer(pointer, window_.Hwnd()));
        return result.handled ? win32::WindowMessageResult::Handled() : win32::WindowMessageResult::Unhandled();
    }
    case WM_KEYDOWN: {
        UiKeyEvent key{
            .type = UiEventType::KeyDown,
            .virtual_key = static_cast<UINT>(wparam),
            .modifiers = CurrentModifiers(),
            .popup_host = options_.enable_popup ? &popup_ : nullptr,
        };
        UiEventResult result = DispatchInputEvent(UiInputEvent::Key(key, window_.Hwnd()));
        PositionIme();
        return result.handled ? win32::WindowMessageResult::Handled() : win32::WindowMessageResult::Unhandled();
    }
    case WM_CHAR: {
        UiEventResult result = root_ != nullptr ? root_->OnInputEvent(UiInputEvent{
            .type = UiEventType::TextChar,
            .character = static_cast<wchar_t>(wparam),
            .hwnd = window_.Hwnd(),
        }) : UiEventResult{};
        HandleUiResult(result);
        PositionIme();
        return result.handled ? win32::WindowMessageResult::Handled() : win32::WindowMessageResult::Unhandled();
    }
    case WM_IME_STARTCOMPOSITION:
        if (options_.enable_ime) {
            HandleUiResult(root_ != nullptr ? root_->OnInputEvent(UiInputEvent{.type = UiEventType::ImeStartComposition, .hwnd = window_.Hwnd()}) : UiEventResult{});
            PositionIme();
            return win32::WindowMessageResult::Handled();
        }
        break;
    case WM_IME_COMPOSITION:
        if (options_.enable_ime) {
            HandleUiResult(root_ != nullptr ? root_->OnInputEvent(UiInputEvent{
                .type = UiEventType::ImeComposition,
                .text = ImeCompositionString(lparam),
                .hwnd = window_.Hwnd(),
            }) : UiEventResult{});
            PositionIme();
        }
        break;
    case WM_IME_ENDCOMPOSITION:
        if (options_.enable_ime) {
            HandleUiResult(root_ != nullptr ? root_->OnInputEvent(UiInputEvent{.type = UiEventType::ImeEndComposition, .hwnd = window_.Hwnd()}) : UiEventResult{});
            return win32::WindowMessageResult::Handled();
        }
        break;
    case WM_CONTEXTMENU: {
        POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        D2D1_POINT_2F client_point = {};
        if (point.x == -1 && point.y == -1) {
            client_point = D2D1::Point2F(16.0f, 112.0f);
        } else {
            ScreenToClient(window_.Hwnd(), &point);
            client_point = PhysicalClientPointToUi(window_.Hwnd(), point);
        }
        UiEventResult result = root_ != nullptr ? root_->OnInputEvent(UiInputEvent{
            .type = UiEventType::ContextMenu,
            .point = client_point,
            .screen_point = POINT{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)},
            .hwnd = window_.Hwnd(),
            .popup_host = options_.enable_popup ? &popup_ : nullptr,
        }) : UiEventResult{};
        HandleUiResult(result);
        return result.handled ? win32::WindowMessageResult::Handled() : win32::WindowMessageResult::Unhandled();
    }
    case WM_TIMER: {
        UiEventResult result = root_ != nullptr ? root_->OnInputEvent(UiInputEvent{
            .type = UiEventType::Timer,
            .timer_id = wparam,
            .hwnd = window_.Hwnd(),
        }) : UiEventResult{};
        if (result.value_changed ||
            result.action != kUiActionNone ||
            result.capture != UiCaptureRequest::None ||
            result.popup.has_value() ||
            result.close_popup) {
            HandleUiResult(result);
        }
        return result.handled ? win32::WindowMessageResult::Handled() : win32::WindowMessageResult::Unhandled();
    }
    case WM_ACTIVATE:
        if (LOWORD(wparam) == WA_INACTIVE) {
            if (options_.enable_popup) {
                UiEventResult result = {};
                if (DispatchOwnerDeactivatedToPopup(&popup_, window_.Hwnd(), &result)) {
                    HandleUiResult(result);
                }
            }
            HandleUiResult(root_ != nullptr ? root_->OnInputEvent(UiInputEvent{.type = UiEventType::OwnerDeactivated, .hwnd = window_.Hwnd()}) : UiEventResult{});
            Invalidate();
        }
        break;
    case WM_ACTIVATEAPP:
        if (wparam == FALSE && options_.enable_popup) {
            ClosePopupIfOpen(&popup_);
        }
        break;
    case WM_CLOSE:
        if (options_.enable_popup) {
            ClosePopupIfOpen(&popup_);
        }
        window_.Destroy();
        return win32::WindowMessageResult::Handled();
    case WM_DESTROY:
        if (options_.enable_popup) {
            ClosePopupIfOpen(&popup_);
        }
        delegate_->OnDestroy(*this);
        return win32::WindowMessageResult::Handled();
    default:
        break;
    }

    return delegate_->OnUnhandledMessage(*this, message, wparam, lparam);
}

HRESULT UiWindowHost::InitializeRenderResources()
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, graphics_);
    d2d_context_ = graphics_->D2DContext();
    dcomp_device_ = graphics_->DCompDevice();
    dwrite_factory_ = graphics_->DWriteFactory();
    return S_OK;
}

void UiWindowHost::Render()
{
    const D2D1_SIZE_U client_pixel = ClientPixelSize(window_.Hwnd());
    const float dpi_scale = math::CoordinateSpace::FromWindow(window_.Hwnd()).scale();

    struct RenderState final {
        UiWindowHost* host;
        D2D1_SIZE_U client_pixel;
        float dpi_scale;
    } state{this, client_pixel, dpi_scale};
    if (FAILED(ui_common_window::RenderUiCompositionSurface(
        graphics_,
        window_.Hwnd(),
        dcomp_target_,
        dcomp_visual_,
        dcomp_surface_,
        client_pixel.width,
        client_pixel.height,
        &surface_width_,
        &surface_height_,
        dwrite_factory_.get(),
        D2D1::SizeF(static_cast<float>(client_pixel.width) / dpi_scale, static_cast<float>(client_pixel.height) / dpi_scale),
        dpi_scale,
        D2D1::Point2F(0.0f, 0.0f),
        D2D1::ColorF(D2D1::ColorF::Black, 0.0f),
        [](const UiDrawContext& draw_context, void* user_data) -> HRESULT {
            const auto* state = static_cast<const RenderState*>(user_data);
            RETURN_HR_IF_NULL(E_INVALIDARG, state);
            UiWindowHost* host = state->host;
            RETURN_HR_IF_NULL(E_INVALIDARG, host);
            if (host->root_ != nullptr) {
                host->root_->Render(draw_context);
            }
            return S_OK;
        },
        &state))) {
        return;
    }
}

void UiWindowHost::HandleUiResult(UiEventResult result)
{
    if (result.ime_caret_point.has_value()) {
        ime_caret_point_ = result.ime_caret_point;
    }
    ApplyUiCaptureRequest(window_.Hwnd(), result.capture);
    // Full-repaint doctrine: any dispatched event repaints the layer (refactor.md 3.4).
    RequestWindowRender(window_.Hwnd());
    if (result.popup.has_value() && options_.enable_popup) {
        const UiPopupRequest& popup = result.popup.value();
        if (SUCCEEDED(popup_.OpenPopup(popup.origin, MakeJsonPopupContent(popup.state_json)))) {
            result.value_changed = true;
        }
    }
    if (result.value_changed) {
        delegate_->OnUiValueChanged(*this, result);
    }
    if (result.close_popup && options_.enable_popup) {
        ClosePopupIfOpen(&popup_);
    }
    ExecuteAction(result.action);
}

bool UiWindowHost::ExecuteAction(UiAction action)
{
    if (action == kUiActionNone) {
        return false;
    }
    return delegate_->OnUiAction(*this, action);
}

UiEventResult UiWindowHost::DispatchInputEvent(const UiInputEvent& event)
{
    UiEventResult result = {};
    if (options_.enable_popup && DispatchInputEventToPopup(&popup_, event, &result)) {
        HandleUiResult(result);
        if (result.handled) {
            return result;
        }
    }
    result = root_ != nullptr ? root_->OnInputEvent(event) : UiEventResult{};
    HandleUiResult(result);
    return result;
}

UiModifiers UiWindowHost::CurrentModifiers() const
{
    return UiModifiers::Current();
}

void UiWindowHost::PositionIme()
{
    if (!options_.enable_ime) {
        return;
    }
    if (ime_caret_point_.has_value()) {
        SetImeCompositionWindowClientPoint(window_.Hwnd(), UiPointToPhysicalClient(window_.Hwnd(), *ime_caret_point_));
        return;
    }
}

D2D1_POINT_2F UiWindowHost::CaretPoint() const
{
    return {};
}

std::wstring UiWindowHost::ImeCompositionString(LPARAM lparam) const
{
    return ReadImeCompositionString(window_.Hwnd(), lparam, GCS_COMPSTR);
}
