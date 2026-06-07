#include "ui.window.hpp"

#include <algorithm>

#include <d2d1helper.h>
#include <windowsx.h>
#include <wil/result_macros.h>

#include "ui.a11y.hpp"
#include "ui.textbox.hpp"

namespace {

D2D1_SIZE_U ClientPixelSize(HWND hwnd)
{
    RECT rect = {};
    GetClientRect(hwnd, &rect);
    return D2D1::SizeU(
        static_cast<UINT32>((std::max)(1L, rect.right - rect.left)),
        static_cast<UINT32>((std::max)(1L, rect.bottom - rect.top)));
}

D2D1_SIZE_F ClientRenderSize(HWND hwnd)
{
    const D2D1_SIZE_U size = ClientPixelSize(hwnd);
    return D2D1::SizeF(static_cast<float>(size.width), static_cast<float>(size.height));
}

} // namespace

HRESULT UiWindowHost::Create(UiWindowOptions options, std::unique_ptr<UiRoot> root, UiWindowDelegate* delegate)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, root);
    RETURN_HR_IF_NULL(E_INVALIDARG, delegate);
    RETURN_HR_IF(E_INVALIDARG, options.action_message == 0);

    options_ = options;
    delegate_ = delegate;
    ui_.ResetRoot(std::move(root));
    return window_.Create(options_.native, this);
}

void UiWindowHost::ResetRoot(std::unique_ptr<UiRoot> root)
{
    if (options_.enable_popup) {
        popup_.Close();
    }
    ui_.ResetRoot(std::move(root));
    if (accessibility_provider_ != nullptr && window_.Hwnd() != nullptr && options_.enable_accessibility) {
        accessibility_provider_.reset();
        CreateUiAccessibilityProvider(
            window_.Hwnd(),
            options_.action_message,
            &ui_,
            accessibility_provider_.put());
    }
    Invalidate();
}

void UiWindowHost::Invalidate()
{
    if (window_.Hwnd() != nullptr) {
        InvalidateRect(window_.Hwnd(), nullptr, FALSE);
    }
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

UiController& UiWindowHost::Ui()
{
    return ui_;
}

PopupHost& UiWindowHost::Popup()
{
    return popup_;
}

IDWriteFactory* UiWindowHost::DWriteFactory() const
{
    return dwrite_factory_.get();
}

IDWriteTextFormat* UiWindowHost::BodyTextFormat() const
{
    return body_text_format_.get();
}

IDWriteTextFormat* UiWindowHost::IconTextFormat() const
{
    return icon_text_format_.get();
}

win32::WindowMessageResult UiWindowHost::OnWindowMessage(
    win32::NativeWindow&,
    UINT message,
    WPARAM wparam,
    LPARAM lparam)
{
    if (message == options_.action_message) {
        const UiElementId effect_target = static_cast<UiElementId>(static_cast<int>(lparam));
        if (effect_target != UiElementId::None && ui_.Root() != nullptr) {
            ui_.Root()->ApplyElementEffect(effect_target);
            Invalidate();
        }
        ExecuteAction(UiAction(static_cast<int>(wparam)));
        return win32::WindowMessageResult::Handled();
    }

    switch (message) {
    case WM_CREATE:
        if (FAILED(InitializeRenderResources()) ||
            (options_.enable_popup &&
                FAILED(popup_.Initialize(
                    window_.Hwnd(),
                    options_.action_message,
                    d2d_factory_.get(),
                    dwrite_factory_.get()))) ||
            (options_.enable_accessibility &&
                FAILED(CreateUiAccessibilityProvider(
                    window_.Hwnd(),
                    options_.action_message,
                    &ui_,
                    accessibility_provider_.put()))) ||
            FAILED(delegate_->OnCreate(*this))) {
            return win32::WindowMessageResult::Handled(-1);
        }
        return win32::WindowMessageResult::Handled();
    case WM_ENTERSIZEMOVE:
    case WM_MOVE:
        if (options_.enable_popup && popup_.IsOpen()) {
            popup_.Close();
        }
        break;
    case WM_SIZE:
        if (options_.enable_popup && popup_.IsOpen()) {
            popup_.Close();
        }
        if (render_target_ != nullptr) {
            render_target_->Resize(ClientPixelSize(window_.Hwnd()));
        }
        Invalidate();
        return win32::WindowMessageResult::Handled();
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
        const D2D1_POINT_2F point =
            D2D1::Point2F(static_cast<float>(GET_X_LPARAM(lparam)), static_cast<float>(GET_Y_LPARAM(lparam)));
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
        HandleUiResult(ui_.OnInputEvent(UiInputEvent::Pointer(pointer, window_.Hwnd())));
        return win32::WindowMessageResult::Handled();
    }
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP: {
        SetFocus(window_.Hwnd());
        const UiEventType type = message == WM_LBUTTONDOWN ? UiEventType::PointerDown : UiEventType::PointerUp;
        const D2D1_POINT_2F point =
            D2D1::Point2F(static_cast<float>(GET_X_LPARAM(lparam)), static_cast<float>(GET_Y_LPARAM(lparam)));
        UiPointerEvent pointer{
            .type = type,
            .point = point,
            .button = UiPointerButton::Left,
            .modifiers = CurrentModifiers(),
            .popup_host = options_.enable_popup ? &popup_ : nullptr,
        };
        DispatchInputEvent(UiInputEvent::Pointer(pointer, window_.Hwnd()));
        SyncCaretTimer();
        PositionIme();
        return win32::WindowMessageResult::Handled();
    }
    case WM_KEYDOWN: {
        UiKeyEvent key{
            .type = UiEventType::KeyDown,
            .virtual_key = static_cast<UINT>(wparam),
            .modifiers = CurrentModifiers(),
            .popup_host = options_.enable_popup ? &popup_ : nullptr,
        };
        UiEventResult result = DispatchInputEvent(UiInputEvent::Key(key, window_.Hwnd()));
        SyncCaretTimer();
        PositionIme();
        return result.handled ? win32::WindowMessageResult::Handled() : win32::WindowMessageResult::Unhandled();
    }
    case WM_CHAR: {
        UiEventResult result = ui_.OnInputEvent(UiInputEvent{
            .type = UiEventType::TextChar,
            .character = static_cast<wchar_t>(wparam),
            .hwnd = window_.Hwnd(),
        });
        HandleUiResult(result);
        PositionIme();
        return result.handled ? win32::WindowMessageResult::Handled() : win32::WindowMessageResult::Unhandled();
    }
    case WM_IME_STARTCOMPOSITION:
        if (options_.enable_ime) {
            HandleUiResult(ui_.OnInputEvent(UiInputEvent{.type = UiEventType::ImeStartComposition, .hwnd = window_.Hwnd()}));
            PositionIme();
            return win32::WindowMessageResult::Handled();
        }
        break;
    case WM_IME_COMPOSITION:
        if (options_.enable_ime) {
            HandleUiResult(ui_.OnInputEvent(UiInputEvent{
                .type = UiEventType::ImeComposition,
                .text = ImeCompositionString(lparam),
                .hwnd = window_.Hwnd(),
            }));
            PositionIme();
        }
        break;
    case WM_IME_ENDCOMPOSITION:
        if (options_.enable_ime) {
            HandleUiResult(ui_.OnInputEvent(UiInputEvent{.type = UiEventType::ImeEndComposition, .hwnd = window_.Hwnd()}));
            return win32::WindowMessageResult::Handled();
        }
        break;
    case WM_CONTEXTMENU: {
        POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        if (point.x == -1 && point.y == -1) {
            point = POINT{32, 224};
        } else {
            ScreenToClient(window_.Hwnd(), &point);
        }
        const D2D1_POINT_2F client_point = D2D1::Point2F(static_cast<float>(point.x), static_cast<float>(point.y));
        UiEventResult result = ui_.OnInputEvent(UiInputEvent{
            .type = UiEventType::ContextMenu,
            .point = client_point,
            .screen_point = POINT{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)},
            .hwnd = window_.Hwnd(),
            .popup_host = options_.enable_popup ? &popup_ : nullptr,
        });
        HandleUiResult(result);
        SyncCaretTimer();
        return result.handled ? win32::WindowMessageResult::Handled() : win32::WindowMessageResult::Unhandled();
    }
    case WM_TIMER:
        if (wparam == options_.caret_timer_id) {
            UiEventResult result = ui_.OnInputEvent(UiInputEvent{
                .type = UiEventType::Timer,
                .timer_id = static_cast<UINT_PTR>(wparam),
                .hwnd = window_.Hwnd(),
            });
            HandleUiResult(result);
            return win32::WindowMessageResult::Handled();
        }
        break;
    case WM_ACTIVATE:
        if (LOWORD(wparam) == WA_INACTIVE) {
            if (options_.enable_popup) {
                HandleUiResult(popup_.OnInputEvent(UiInputEvent{.type = UiEventType::OwnerDeactivated, .hwnd = window_.Hwnd()}));
            }
            HandleUiResult(ui_.OnInputEvent(UiInputEvent{.type = UiEventType::OwnerDeactivated, .hwnd = window_.Hwnd()}));
            Invalidate();
        }
        break;
    case WM_ACTIVATEAPP:
        if (wparam == FALSE && options_.enable_popup && popup_.IsOpen()) {
            popup_.Close();
        }
        break;
    case WM_GETOBJECT:
        if (lparam == UiaRootObjectId && accessibility_provider_ != nullptr) {
            return win32::WindowMessageResult::Handled(UiaReturnRawElementProvider(
                window_.Hwnd(),
                wparam,
                lparam,
                accessibility_provider_.get()));
        }
        break;
    case WM_CLOSE:
        if (options_.enable_popup) {
            popup_.Close();
        }
        window_.Destroy();
        return win32::WindowMessageResult::Handled();
    case WM_DESTROY:
        KillTimer(window_.Hwnd(), options_.caret_timer_id);
        if (options_.enable_popup) {
            popup_.Close();
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
    RETURN_IF_FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d_factory_.put()));
    RETURN_IF_FAILED(DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(dwrite_factory_.put())));
    RETURN_IF_FAILED(dwrite_factory_->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        options_.body_font_size,
        L"",
        body_text_format_.put()));
    RETURN_IF_FAILED(dwrite_factory_->CreateTextFormat(
        L"Segoe MDL2 Assets",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        options_.icon_font_size,
        L"",
        icon_text_format_.put()));
    RETURN_IF_FAILED(body_text_format_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
    RETURN_IF_FAILED(icon_text_format_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
    popup_.SetTextFormats(body_text_format_.get(), icon_text_format_.get());
    return S_OK;
}

HRESULT UiWindowHost::EnsureRenderTarget()
{
    if (render_target_ != nullptr) {
        return S_OK;
    }
    RETURN_IF_FAILED(d2d_factory_->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(),
            96.0f,
            96.0f),
        D2D1::HwndRenderTargetProperties(window_.Hwnd(), ClientPixelSize(window_.Hwnd())),
        render_target_.put()));
    return S_OK;
}

void UiWindowHost::Render()
{
    if (FAILED(EnsureRenderTarget())) {
        return;
    }
    const UiDrawContext draw_context{
        .d2d_context = render_target_.get(),
        .dwrite_factory = dwrite_factory_.get(),
        .body_text_format = body_text_format_.get(),
        .icon_text_format = icon_text_format_.get(),
        .viewport_size = ClientRenderSize(window_.Hwnd()),
    };
    render_target_->BeginDraw();
    ui_.Render(draw_context);
    if (options_.enable_popup) {
        popup_.Render(draw_context);
    }
    if (render_target_->EndDraw() == D2DERR_RECREATE_TARGET) {
        render_target_.reset();
    }
}

void UiWindowHost::HandleUiResult(UiEventResult result)
{
    if (result.capture == UiCaptureRequest::Capture) {
        SetCapture(window_.Hwnd());
    } else if (result.capture == UiCaptureRequest::Release) {
        ReleaseCapture();
    }
    if (result.needs_render) {
        Invalidate();
    }
    if (result.value_changed) {
        delegate_->OnUiValueChanged(*this, result);
    }
    if (result.effect_target != UiElementId::None && ui_.Root() != nullptr) {
        ui_.Root()->ApplyElementEffect(result.effect_target);
        Invalidate();
    }
    if (result.close_popup && options_.enable_popup) {
        popup_.Close();
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
    if (options_.enable_popup && popup_.IsOpen()) {
        UiInputEvent popup_event = event;
        popup_event.popup_host = &popup_;
        if (event.type == UiEventType::KeyDown) {
            popup_event.key.popup_host = &popup_;
        } else {
            popup_event.pointer.popup_host = &popup_;
        }
        UiEventResult result = popup_.OnInputEvent(popup_event);
        HandleUiResult(result);
        if (result.handled) {
            return result;
        }
    }
    UiEventResult result = ui_.OnInputEvent(event);
    HandleUiResult(result);
    return result;
}

UiModifiers UiWindowHost::CurrentModifiers() const
{
    return UiModifiers::Current();
}

void UiWindowHost::SyncCaretTimer()
{
    if (!IsFocusedTextElement()) {
        KillTimer(window_.Hwnd(), options_.caret_timer_id);
        return;
    }
    UiElement* root = ui_.Root() != nullptr ? ui_.Root()->Root() : nullptr;
    UiElement* element = root != nullptr ? root->FindById(ui_.FocusedElement()) : nullptr;
    if (auto* text_box = dynamic_cast<TextBox*>(element)) {
        text_box->SetCaretVisible(true);
    }
    SetTimer(window_.Hwnd(), options_.caret_timer_id, GetCaretBlinkTime() == INFINITE ? 530 : GetCaretBlinkTime(), nullptr);
}

void UiWindowHost::PositionIme()
{
    if (!options_.enable_ime || !IsFocusedTextElement()) {
        return;
    }
    HIMC ime = ImmGetContext(window_.Hwnd());
    if (ime == nullptr) {
        return;
    }
    const D2D1_POINT_2F point = CaretPoint();
    COMPOSITIONFORM form = {};
    form.dwStyle = CFS_POINT;
    form.ptCurrentPos = POINT{static_cast<LONG>(point.x), static_cast<LONG>(point.y)};
    ImmSetCompositionWindow(ime, &form);
    ImmReleaseContext(window_.Hwnd(), ime);
}

D2D1_POINT_2F UiWindowHost::CaretPoint() const
{
    const UiElementMetadata* metadata = ui_.ElementMetadata(ui_.FocusedElement());
    if (metadata == nullptr || metadata->role != UiElementRole::Edit) {
        return {};
    }
    const UiElement* root = ui_.Root() != nullptr ? ui_.Root()->Root() : nullptr;
    const UiElement* element = root != nullptr ? root->FindById(ui_.FocusedElement()) : nullptr;
    const auto* text_box = dynamic_cast<const TextBox*>(element);
    return text_box != nullptr ? text_box->CaretPoint() : D2D1_POINT_2F{};
}

bool UiWindowHost::IsFocusedTextElement() const
{
    const UiElementMetadata* metadata = ui_.ElementMetadata(ui_.FocusedElement());
    return metadata != nullptr && metadata->role == UiElementRole::Edit;
}

std::wstring UiWindowHost::ImeCompositionString(LPARAM lparam) const
{
    if ((lparam & GCS_COMPSTR) == 0) {
        return {};
    }
    HIMC ime = ImmGetContext(window_.Hwnd());
    if (ime == nullptr) {
        return {};
    }
    const LONG bytes = ImmGetCompositionStringW(ime, GCS_COMPSTR, nullptr, 0);
    std::wstring text(bytes > 0 ? static_cast<size_t>(bytes) / sizeof(wchar_t) : 0, L'\0');
    if (!text.empty()) {
        ImmGetCompositionStringW(ime, GCS_COMPSTR, text.data(), bytes);
    }
    ImmReleaseContext(window_.Hwnd(), ime);
    return text;
}
