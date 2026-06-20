#include "ui.popup.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

#include <d2d1helper.h>
#include <quickjs.h>
#include <windowsx.h>
#include <wil/result_macros.h>

#include "ui.common_window.hpp"

LRESULT CALLBACK PopupWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

namespace {

constexpr wchar_t kPopupWindowClassName[] = L"UiPopupWindow";
constexpr float kPopupContentInset = 1.0f;
constexpr char kPopupScriptRelativePath[] = "scripts/popup_ui.js";

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

void SetString(JSContext* context, JSValue object, const char* name, std::wstring_view value)
{
    JS_SetPropertyStr(context, object, name, JS_NewString(context, imgviewer::Utf8FromWide(value).c_str()));
}

void SetString(JSContext* context, JSValue object, const char* name, const char* value)
{
    JS_SetPropertyStr(context, object, name, JS_NewString(context, value != nullptr ? value : ""));
}

void SetBool(JSContext* context, JSValue object, const char* name, bool value)
{
    JS_SetPropertyStr(context, object, name, JS_NewBool(context, value));
}

bool BoolProperty(JSContext* context, JSValueConst object, const char* name, bool fallback)
{
    if (!JS_IsObject(object)) {
        return fallback;
    }
    JSValue value = JS_GetPropertyStr(context, object, name);
    const bool result = JS_IsUndefined(value) ? fallback : JS_ToBool(context, value) != 0;
    JS_FreeValue(context, value);
    return result;
}

int32_t Int32Property(JSContext* context, JSValueConst object, const char* name, int32_t fallback)
{
    if (!JS_IsObject(object)) {
        return fallback;
    }
    JSValue value = JS_GetPropertyStr(context, object, name);
    if (JS_IsUndefined(value)) {
        JS_FreeValue(context, value);
        return fallback;
    }
    int32_t result = fallback;
    JS_ToInt32(context, &result, value);
    JS_FreeValue(context, value);
    return result;
}

float FloatProperty(JSContext* context, JSValueConst object, const char* name, float fallback)
{
    if (!JS_IsObject(object)) {
        return fallback;
    }
    JSValue value = JS_GetPropertyStr(context, object, name);
    if (JS_IsUndefined(value)) {
        JS_FreeValue(context, value);
        return fallback;
    }
    double result = fallback;
    JS_ToFloat64(context, &result, value);
    JS_FreeValue(context, value);
    return static_cast<float>(result);
}

UiAction ActionProperty(JSContext* context, JSValueConst object)
{
    if (!JS_IsObject(object)) {
        return kUiActionNone;
    }
    JSValue action_value = JS_GetPropertyStr(context, object, "actionValue");
    if (!JS_IsUndefined(action_value)) {
        int32_t value = 0;
        JS_ToInt32(context, &value, action_value);
        JS_FreeValue(context, action_value);
        return UiAction(value, Int32Property(context, object, "actionArg", 0));
    }
    JS_FreeValue(context, action_value);

    JSValue value = JS_GetPropertyStr(context, object, "action");
    const std::string name = imgviewer::Utf8FromValue(context, value);
    JS_FreeValue(context, value);
    if (name.empty()) {
        return kUiActionNone;
    }
    return UiAction(static_cast<int>(ImgViewerActionFromName(name.c_str())), Int32Property(context, object, "actionArg", 0));
}

class JsonPopupContent final : public PopupContent {
public:
    explicit JsonPopupContent(std::string state_json) : state_json_(std::move(state_json)) {}

    JSValue CreateState(JSContext* context) const override
    {
        JSValue state = JS_ParseJSON(context, state_json_.c_str(), state_json_.size(), "<popup-state>");
        if (!JS_IsException(state)) {
            return state;
        }

        JS_FreeValue(context, state);
        JSValue exception = JS_GetException(context);
        JS_FreeValue(context, exception);
        JSValue fallback = JS_NewObject(context);
        SetString(context, fallback, "kind", "none");
        return fallback;
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
    d2d_context_ = graphics_->D2DContext();
    dcomp_device_ = graphics_->DCompDevice();
    dwrite_factory_ = graphics_->DWriteFactory();

    RETURN_IF_FAILED(LoadScript());
    return ui_common_window::RegisterWindowClass(
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner_, GWLP_HINSTANCE)),
        kPopupWindowClassName,
        PopupWindowProc);
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

HRESULT PopupHost::LoadScript()
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, script_engine_);
    script_context_.reset();
    script_context_ = script_engine_->CreateContext();
    if (script_context_ == nullptr) {
        error_text_ = script_engine_->TakeExceptionTextUtf8();
        return E_FAIL;
    }
    JS_SetContextOpaque(script_context_->Context(), this);

    JSContext* context = script_context_->Context();
    JSValue global = JS_GetGlobalObject(context);
    JS_SetPropertyStr(context, global, "host", imgviewer::CreateHostObject(context));
    JS_FreeValue(context, global);

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
    JSValue app = AppObject();
    if (!JS_IsObject(app)) {
        JS_FreeValue(context, app);
        error_text_ = "globalThis.imgviewerPopupUi was not defined";
        return E_FAIL;
    }
    JS_FreeValue(context, app);
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
    JSValue app = AppObject();
    JSValue measure = JS_GetPropertyStr(context, app, "measure");
    if (!JS_IsFunction(context, measure)) {
        JS_FreeValue(context, measure);
        JS_FreeValue(context, app);
        return D2D1::SizeF(1.0f, 1.0f);
    }
    JSValue state = CreateStateObject();
    JSValue result = JS_Call(context, measure, app, 1, &state);
    JS_FreeValue(context, state);
    JS_FreeValue(context, measure);
    JS_FreeValue(context, app);
    if (JS_IsException(result)) {
        JS_FreeValue(context, result);
        script_context_->CaptureException();
        error_text_ = script_engine_->TakeExceptionTextUtf8();
        return D2D1::SizeF(1.0f, 1.0f);
    }
    const D2D1_SIZE_F size{
        (std::max)(1.0f, FloatProperty(context, result, "width", 1.0f)),
        (std::max)(1.0f, FloatProperty(context, result, "height", 1.0f)),
    };
    JS_FreeValue(context, result);
    return size;
}

void PopupHost::RenderScriptContent(const UiDrawContext& draw_context)
{
    if (script_context_ == nullptr) {
        return;
    }
    JSContext* context = script_context_->Context();
    JSValue app = AppObject();
    JSValue render = JS_GetPropertyStr(context, app, "render");
    if (!JS_IsFunction(context, render)) {
        JS_FreeValue(context, render);
        JS_FreeValue(context, app);
        return;
    }
    JSValue canvas = imgviewer::CreateCanvasObject(context);
    JSValue env = imgviewer::CreateRenderEnvironment(context, draw_context);
    JSValue state = CreateStateObject();
    JSValue args[] = {canvas, env, state};
    active_draw_context_ = &draw_context;
    JSValue result = JS_Call(context, render, app, 3, args);
    active_draw_context_ = nullptr;
    JS_FreeValue(context, state);
    JS_FreeValue(context, env);
    JS_FreeValue(context, canvas);
    JS_FreeValue(context, render);
    JS_FreeValue(context, app);
    if (JS_IsException(result)) {
        JS_FreeValue(context, result);
        script_context_->CaptureException();
        error_text_ = script_engine_->TakeExceptionTextUtf8();
        return;
    }
    JS_FreeValue(context, result);
    script_engine_->PumpJobs();
}

UiEventResult PopupHost::DispatchScriptInput(const UiInputEvent& event)
{
    UiEventResult event_result{};
    if (script_context_ == nullptr) {
        return event_result;
    }
    JSContext* context = script_context_->Context();
    JSValue app = AppObject();
    const bool pointer_event = event.type == UiEventType::PointerMove ||
        event.type == UiEventType::PointerDown ||
        event.type == UiEventType::PointerUp ||
        event.type == UiEventType::PointerLeave ||
        event.type == UiEventType::PointerWheel;
    JSValue handler = JS_GetPropertyStr(context, app, pointer_event ? "pointer" : "key");
    if (!JS_IsFunction(context, handler)) {
        JS_FreeValue(context, handler);
        JS_FreeValue(context, app);
        return event_result;
    }
    JSValue js_event = pointer_event
        ? imgviewer::CreatePointerEvent(context, event.pointer)
        : imgviewer::CreateKeyEvent(context, event.key);
    JSValue state = CreateStateObject();
    JSValue args[] = {js_event, state};
    JSValue result = JS_Call(context, handler, app, 2, args);
    JS_FreeValue(context, state);
    JS_FreeValue(context, js_event);
    JS_FreeValue(context, handler);
    JS_FreeValue(context, app);
    if (JS_IsException(result)) {
        JS_FreeValue(context, result);
        script_context_->CaptureException();
        error_text_ = script_engine_->TakeExceptionTextUtf8();
        event_result.handled = true;
        event_result.close_popup = true;
        return event_result;
    }

    event_result.handled = BoolProperty(context, result, "handled", false);
    event_result.close_popup = BoolProperty(context, result, "close", false) || close_requested_;
    event_result.value_changed = BoolProperty(context, result, "invalidate", false) || invalidate_requested_;
    event_result.action = ActionProperty(context, result);
    if (content_ != nullptr) {
        content_->ApplyResult(context, result, &event_result);
    }
    JS_FreeValue(context, result);

    close_requested_ = false;
    invalidate_requested_ = false;
    script_engine_->PumpJobs();
    return event_result;
}

JSValue PopupHost::AppObject() const
{
    JSContext* context = script_context_->Context();
    JSValue global = JS_GetGlobalObject(context);
    JSValue app = JS_GetPropertyStr(context, global, "imgviewerPopupUi");
    JS_FreeValue(context, global);
    return app;
}

JSValue PopupHost::CreateStateObject() const
{
    JSContext* context = script_context_->Context();
    if (content_ != nullptr) {
        return content_->CreateState(context);
    }

    JSValue state = JS_NewObject(context);
    SetString(context, state, "kind", "none");
    return state;
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
        body_text_format_,
        icon_text_format_,
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
