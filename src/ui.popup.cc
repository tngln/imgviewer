#include "ui.popup.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

#include <d2d1helper.h>
#include <quickjs.h>
#include <windowsx.h>
#include <wil/result_macros.h>

#include "script.quickjs_helper.hpp"
#include "ui.common_window.hpp"

LRESULT CALLBACK PopupWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

namespace {

constexpr wchar_t kPopupWindowClassName[] = L"UiPopupWindow";
constexpr float kPopupContentInset = 1.0f;
constexpr char kPopupScriptRelativePath[] = "scripts/popup_ui.js";

using imgviewer::ActionProperty;

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

class JsonPopupContent final : public PopupContent {
public:
    explicit JsonPopupContent(std::string state_json) : state_json_(std::move(state_json)) {}

    JSValue CreateState(JSContext* context) const override
    {
        script::QuickJsValue state(context, JS_ParseJSON(context, state_json_.c_str(), state_json_.size(), "<popup-state>"));
        if (!JS_IsException(state.Get())) {
            return state.Release();
        }

        script::QuickJsValue exception(context, JS_GetException(context));
        script::ObjectBuilder fallback(context);
        fallback.Set("kind", "none");
        return fallback.Release();
    }

    void ApplyResult(JSContext*, JSValueConst, UiEventResult*) override {}

private:
    std::string state_json_;
};

} // namespace

std::unique_ptr<PopupContent> MakeJsonPopupContent(std::string state_json)
{
    return std::make_unique<JsonPopupContent>(std::move(state_json));
}

HRESULT PopupHost::Initialize(HWND owner, UINT action_message, GraphicsDevice* graphics, script::QuickJsRuntime* script_engine)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, owner);
    RETURN_HR_IF(E_INVALIDARG, action_message == 0);
    RETURN_HR_IF_NULL(E_INVALIDARG, graphics);
    RETURN_HR_IF_NULL(E_INVALIDARG, script_engine);

    owner_ = owner;
    action_message_ = action_message;
    graphics_ = graphics;
    script_engine_ = script_engine;
    timers_ = std::make_unique<script::ScriptTimerManager>(*script_engine_);
    d2d_context_ = graphics_->D2DContext();
    dcomp_device_ = graphics_->DCompDevice();
    dwrite_factory_ = graphics_->DWriteFactory();

    RETURN_IF_FAILED(LoadScript());
    return ui_common_window::RegisterWindowClass(
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner_, GWLP_HINSTANCE)),
        kPopupWindowClassName,
        PopupWindowProc);
}

bool PopupHost::IsOpen() const
{
    return native_open_;
}

void PopupHost::Close()
{
    if (timers_ != nullptr) {
        timers_->ClearAll();
        timers_->SetHwnd(nullptr);
    }
    if (content_) {
        content_->OnClose();
        content_.reset();
    }
    native_open_ = false;
    ResetDCompPopupResources();
    if (popup_hwnd_ != nullptr) {
        DestroyWindow(popup_hwnd_);
        popup_hwnd_ = nullptr;
    }
}

HRESULT PopupHost::OpenPopup(D2D1_POINT_2F origin, std::unique_ptr<PopupContent> content)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, content);
    Close();
    content_ = std::move(content);
    const HRESULT hr = OpenScriptPopup(origin);
    if (FAILED(hr)) {
        content_.reset();
    }
    return hr;
}

UiEventResult PopupHost::OnInputEvent(const UiInputEvent& event)
{
    if (!native_open_) {
        return {};
    }

    if (event.type == UiEventType::Timer && timers_ != nullptr) {
        if (event.timer_id < script::kScriptTimerNativeBase) {
            return {};
        }
        const uint32_t timer_id = static_cast<uint32_t>(event.timer_id - script::kScriptTimerNativeBase);
        if (!timers_->HasTimer(timer_id)) {
            return {};
        }
        bool value_changed = false;
        if (!timers_->OnTimer(timer_id, &value_changed)) {
            error_text_ = script_engine_ != nullptr ? script_engine_->TakeExceptionTextUtf8() : "JavaScript timer failed";
            return UiEventResult{.handled = true, .value_changed = true, .close_popup = true};
        }
        const bool wants_close = close_requested_;
        const bool wants_invalidate = value_changed || invalidate_requested_;
        close_requested_ = false;
        invalidate_requested_ = false;
        return UiEventResult{.handled = true, .value_changed = wants_invalidate, .close_popup = wants_close};
    }

    if (event.type == UiEventType::Cancel || event.type == UiEventType::OwnerDeactivated ||
        (event.type == UiEventType::KeyDown && event.key.virtual_key == VK_ESCAPE)) {
        Close();
        return UiEventResult{.handled = true};
    }

    if (event.type == UiEventType::PointerDown && event.hwnd == owner_) {
        Close();
        return {};
    }

    UiEventResult result = DispatchScriptInput(OffsetPopupEvent(event));
    if (!result.close_popup && popup_hwnd_ != nullptr) {
        bool resized = false;
        ResizeNativePopupToContent(&resized);
        if (!resized) {
            RenderNativePopup();
        }
    }
    return result;
}

UiEventResult PopupHost::OnPointerEvent(const UiPointerEvent& event)
{
    return OnInputEvent(UiInputEvent::Pointer(event, nullptr));
}

UiEventResult PopupHost::OnKeyEvent(const UiKeyEvent& event)
{
    return OnInputEvent(UiInputEvent::Key(event, nullptr));
}

const UiDrawContext* PopupHost::ActiveDrawContext() const
{
    return active_draw_context_;
}

void PopupHost::RequestInvalidate()
{
    invalidate_requested_ = true;
}

void PopupHost::RequestReload()
{
    LoadScript();
    invalidate_requested_ = true;
}

void PopupHost::RequestClose()
{
    close_requested_ = true;
}

uint32_t PopupHost::SetScriptTimer(JSContext* context, JSValueConst callback, uint32_t delay_ms, bool repeat)
{
    return timers_ != nullptr ? timers_->SetTimer(context, callback, delay_ms, repeat) : 0;
}

void PopupHost::ClearScriptTimer(uint32_t id)
{
    if (timers_ != nullptr) {
        timers_->ClearTimer(id);
    }
}

HRESULT PopupHost::LoadScript()
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, script_engine_);
    script_context_.reset();
    if (timers_ != nullptr) {
        timers_->ClearAll();
    }
    script_context_ = script_engine_->CreateContext();
    if (script_context_ == nullptr) {
        error_text_ = script_engine_->TakeExceptionTextUtf8();
        return E_FAIL;
    }
    JS_SetContextOpaque(script_context_->Context(), this);

    JSContext* context = script_context_->Context();
    script::QuickJsValue global = script::GetGlobalObject(context);
    JS_SetPropertyStr(context, global.Get(), "host", imgviewer::CreateHostObject(context));
    imgviewer::InstallTimerGlobals(context, global.Get());
    imgviewer::InstallTypographyGlobals(context, global.Get());

    script_path_ = imgviewer::ScriptPath(kPopupScriptRelativePath);
    std::optional<std::string> source = imgviewer::ReadTextFileUtf8(script_path_);
    if (!source.has_value()) {
        error_text_ = "Could not read " + script_path_.string();
        return E_FAIL;
    }
    const script::QuickJsEvalResult eval = script_context_->EvalScript(*source, script_path_.string());
    if (!eval.ok) {
        error_text_ = script_engine_->TakeExceptionTextUtf8();
        return E_FAIL;
    }
    script::QuickJsValue app(context, AppObject());
    if (!JS_IsObject(app.Get())) {
        error_text_ = "globalThis.imgviewerPopupUi was not defined";
        return E_FAIL;
    }
    error_text_.clear();
    return S_OK;
}

HRESULT PopupHost::OpenScriptPopup(D2D1_POINT_2F origin)
{
    if (script_context_ == nullptr && FAILED(LoadScript())) {
        return E_FAIL;
    }
    const D2D1_SIZE_F size = QueryScriptContentSize();
    return OpenNativePopup(origin, size);
}

D2D1_SIZE_F PopupHost::QueryScriptContentSize()
{
    if (script_context_ == nullptr) {
        return D2D1::SizeF(1.0f, 1.0f);
    }
    JSContext* context = script_context_->Context();
    script::QuickJsValue app(context, AppObject());
    script::QuickJsValue measure = script::GetProperty(context, app.Get(), "measure");
    if (!JS_IsFunction(context, measure.Get())) {
        return D2D1::SizeF(1.0f, 1.0f);
    }
    script::QuickJsValue state(context, CreateStateObject());
    JSValue args[] = {state.Get()};
    script::QuickJsValue result = script::Call(context, measure.Get(), app.Get(), 1, args);
    if (JS_IsException(result.Get())) {
        script_context_->CaptureException();
        error_text_ = script_engine_->TakeExceptionTextUtf8();
        return D2D1::SizeF(1.0f, 1.0f);
    }
    const D2D1_SIZE_F size{
        (std::max)(1.0f, script::FloatProperty(context, result.Get(), "width", 1.0f)),
        (std::max)(1.0f, script::FloatProperty(context, result.Get(), "height", 1.0f)),
    };
    return size;
}

void PopupHost::RenderScriptContent(const UiDrawContext& draw_context)
{
    if (script_context_ == nullptr) {
        return;
    }
    JSContext* context = script_context_->Context();
    script::QuickJsValue app(context, AppObject());
    script::QuickJsValue render = script::GetProperty(context, app.Get(), "render");
    if (!JS_IsFunction(context, render.Get())) {
        return;
    }
    script::QuickJsValue canvas(context, imgviewer::CreateCanvasObject(context));
    script::QuickJsValue env(context, imgviewer::CreateRenderEnvironment(context, draw_context));
    script::QuickJsValue state(context, CreateStateObject());
    JSValue args[] = {canvas.Get(), env.Get(), state.Get()};
    active_draw_context_ = &draw_context;
    script::QuickJsValue result = script::Call(context, render.Get(), app.Get(), 3, args);
    active_draw_context_ = nullptr;
    if (JS_IsException(result.Get())) {
        script_context_->CaptureException();
        error_text_ = script_engine_->TakeExceptionTextUtf8();
        return;
    }
    script_engine_->PumpJobs();
}

UiEventResult PopupHost::DispatchScriptInput(const UiInputEvent& event)
{
    UiEventResult event_result{};
    if (script_context_ == nullptr) {
        return event_result;
    }
    JSContext* context = script_context_->Context();
    const bool pointer_event = event.type == UiEventType::PointerMove ||
        event.type == UiEventType::PointerDown ||
        event.type == UiEventType::PointerUp ||
        event.type == UiEventType::PointerLeave ||
        event.type == UiEventType::PointerWheel;
    script::QuickJsValue app(context, AppObject());
    script::QuickJsValue handler = script::GetProperty(context, app.Get(), pointer_event ? "pointer" : "key");
    if (!JS_IsFunction(context, handler.Get())) {
        return event_result;
    }
    script::QuickJsValue js_event(context, pointer_event
        ? imgviewer::CreatePointerEvent(context, event.pointer)
        : imgviewer::CreateKeyEvent(context, event.key));
    script::QuickJsValue state(context, CreateStateObject());
    JSValue args[] = {js_event.Get(), state.Get()};
    script::QuickJsValue result = script::Call(context, handler.Get(), app.Get(), 2, args);
    if (JS_IsException(result.Get())) {
        script_context_->CaptureException();
        error_text_ = script_engine_->TakeExceptionTextUtf8();
        event_result.handled = true;
        event_result.close_popup = true;
        return event_result;
    }

    event_result.handled = script::BoolProperty(context, result.Get(), "handled", false);
    event_result.close_popup = script::BoolProperty(context, result.Get(), "close", false) || close_requested_;
    event_result.value_changed = script::BoolProperty(context, result.Get(), "invalidate", false) || invalidate_requested_;
    event_result.action = ActionProperty(context, result.Get());
    script::QuickJsValue local_action = script::GetProperty(context, result.Get(), "localAction");
    if (!JS_IsUndefined(local_action.Get()) && !JS_IsNull(local_action.Get())) {
        event_result.local_action = script::ToStringUtf8(context, local_action.Get());
        event_result.local_action_arg = script::Int32Property(context, result.Get(), "actionArg", 0);
    }
    if (content_ != nullptr) {
        content_->ApplyResult(context, result.Get(), &event_result);
    }

    close_requested_ = false;
    invalidate_requested_ = false;
    script_engine_->PumpJobs();
    return event_result;
}

JSValue PopupHost::AppObject() const
{
    JSContext* context = script_context_->Context();
    script::QuickJsValue global = script::GetGlobalObject(context);
    return script::GetProperty(context, global.Get(), "imgviewerPopupUi").Release();
}

JSValue PopupHost::CreateStateObject() const
{
    JSContext* context = script_context_->Context();
    if (content_ != nullptr) {
        return content_->CreateState(context);
    }

    script::ObjectBuilder state(context);
    state.Set("kind", "none");
    return state.Release();
}

HRESULT PopupHost::OpenNativePopup(D2D1_POINT_2F origin, D2D1_SIZE_F size)
{
    const float dpi_scale = static_cast<float>(GetDpiForWindow(owner_)) / 96.0f;
    D2D1_POINT_2F window_origin = PopupWindowOrigin(origin);
    window_origin.x *= dpi_scale;
    window_origin.y *= dpi_scale;
    POINT screen_origin = ui_common_window::ClientToScreenPoint(owner_, window_origin);
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
        (std::max)(1, width),
        (std::max)(1, height),
        owner_,
        nullptr,
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner_, GWLP_HINSTANCE)),
        this);
    RETURN_LAST_ERROR_IF_NULL(popup_hwnd_);
    if (timers_ != nullptr) {
        timers_->SetHwnd(popup_hwnd_);
    }

    RETURN_IF_FAILED(ui_common_window::EnsureCompositionTarget(graphics_, popup_hwnd_, dcomp_target_, dcomp_visual_));
    RETURN_IF_FAILED(ui_common_window::EnsureCompositionSurface(
        dcomp_device_.get(),
        dcomp_visual_.get(),
        dcomp_surface_,
        static_cast<UINT>((std::max)(1, width)),
        static_cast<UINT>((std::max)(1, height)),
        &dcomp_surface_width_,
        &dcomp_surface_height_));

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

    const D2D1_SIZE_F size = PopupWindowSize(QueryScriptContentSize());
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

    RETURN_IF_FAILED(ui_common_window::EnsureCompositionSurface(
        dcomp_device_.get(),
        dcomp_visual_.get(),
        dcomp_surface_,
        static_cast<UINT>(width),
        static_cast<UINT>(height),
        &dcomp_surface_width_,
        &dcomp_surface_height_));
    RETURN_IF_FAILED(RenderDCompPopup(static_cast<UINT>(width), static_cast<UINT>(height)));
    RETURN_IF_WIN32_BOOL_FALSE(SetWindowPos(popup_hwnd_, HWND_TOPMOST, origin.x, origin.y, width, height, SWP_NOACTIVATE));
    *resized = true;
    return S_OK;
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
    RETURN_HR_IF_NULL(E_UNEXPECTED, graphics_);
    struct RenderState final {
        PopupHost* host;
        UINT width;
        UINT height;
        float dpi_scale;
    } state{this, width, height, static_cast<float>(GetDpiForWindow(popup_hwnd_)) / 96.0f};
    return ui_common_window::RenderUiCompositionSurface(
        graphics_,
        popup_hwnd_,
        dcomp_target_,
        dcomp_visual_,
        dcomp_surface_,
        width,
        height,
        &dcomp_surface_width_,
        &dcomp_surface_height_,
        dwrite_factory_.get(),
        D2D1::SizeF(static_cast<float>(width) / state.dpi_scale, static_cast<float>(height) / state.dpi_scale),
        state.dpi_scale,
        D2D1::Point2F(kPopupContentInset * state.dpi_scale, kPopupContentInset * state.dpi_scale),
        D2D1::ColorF(D2D1::ColorF::Black, 0.0f),
        [](const UiDrawContext& draw_context, void* user_data) -> HRESULT {
            const auto* state = static_cast<const RenderState*>(user_data);
            RETURN_HR_IF_NULL(E_INVALIDARG, state);
            PopupHost* host = state->host;
            RETURN_HR_IF_NULL(E_INVALIDARG, host);
            host->RenderScriptContent(draw_context);
            return S_OK;
        },
        &state);
}

void PopupHost::HandlePopupResult(UiEventResult result)
{
    if (!result.close_popup && popup_hwnd_ != nullptr) {
        bool resized = false;
        ResizeNativePopupToContent(&resized);
        if (!resized) {
            RenderNativePopup();
        }
    }

    const UiAction action = result.action;
    const bool close_popup = result.close_popup;
    if (close_popup) {
        Close();
    }

    if (action != kUiActionNone) {
        SendMessageW(owner_, action_message_, static_cast<WPARAM>(UiActionValue(action)), 0);
    }
}

LRESULT CALLBACK PopupWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_NCCREATE: {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        ui_common_window::SetWindowUserData(hwnd, reinterpret_cast<PopupHost*>(create->lpCreateParams));
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
        PopupHost* host = ui_common_window::WindowUserData<PopupHost>(hwnd);
        if (host == nullptr) {
            break;
        }
        const float dpi_scale = static_cast<float>(GetDpiForWindow(hwnd)) / 96.0f;
        const UiEventType type = message == WM_MOUSEMOVE ? UiEventType::PointerMove : message == WM_LBUTTONDOWN ? UiEventType::PointerDown : UiEventType::PointerUp;
        UiPointerEvent pointer{
            .type = type,
            .point = D2D1::Point2F(static_cast<float>(GET_X_LPARAM(lparam)) / dpi_scale, static_cast<float>(GET_Y_LPARAM(lparam)) / dpi_scale),
            .button = message == WM_MOUSEMOVE ? UiPointerButton::None : UiPointerButton::Left,
            .popup_host = host,
        };
        const UiEventResult result = host->DispatchScriptInput(OffsetPopupEvent(UiInputEvent::Pointer(pointer, hwnd)));
        host->HandlePopupResult(result);
        return 0;
    }
    case WM_KEYDOWN: {
        PopupHost* host = ui_common_window::WindowUserData<PopupHost>(hwnd);
        if (host != nullptr) {
            const UiKeyEvent key{.type = UiEventType::KeyDown, .virtual_key = static_cast<UINT>(wparam), .modifiers = UiModifiers::Current(), .popup_host = host};
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
    case WM_TIMER: {
        PopupHost* host = ui_common_window::WindowUserData<PopupHost>(hwnd);
        if (host != nullptr) {
            const UiEventResult result = host->OnInputEvent(UiInputEvent{
                .type = UiEventType::Timer,
                .timer_id = wparam,
                .hwnd = hwnd,
            });
            if (result.value_changed || result.action != kUiActionNone || result.close_popup) {
                host->HandlePopupResult(result);
            }
            if (result.handled) {
                return 0;
            }
        }
        break;
    }
    case WM_ACTIVATE:
        if (LOWORD(wparam) == WA_INACTIVE) {
            if (PopupHost* host = ui_common_window::WindowUserData<PopupHost>(hwnd)) {
                host->Close();
            }
        }
        return 0;
    case WM_DESTROY:
        ui_common_window::SetWindowUserData<PopupHost>(hwnd, nullptr);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}
