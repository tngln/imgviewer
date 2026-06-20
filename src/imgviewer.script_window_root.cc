#include "imgviewer.script_window_root.hpp"

#include <utility>

#include <d2d1helper.h>

#include "script.quickjs_helper.hpp"

namespace imgviewer {

ScriptWindowRootBase::ScriptWindowRootBase(
    script::QuickJsRuntime& engine,
    const char* script_relative_path,
    const char* app_global_name,
    std::wstring error_title) :
    engine_(engine),
    timers_(engine_),
    script_relative_path_(script_relative_path),
    app_global_name_(app_global_name),
    error_title_(std::move(error_title))
{
}

ScriptWindowRootBase::~ScriptWindowRootBase() = default;

const UiDrawContext* ScriptWindowRootBase::ActiveDrawContext() const { return active_draw_context_; }
void ScriptWindowRootBase::RequestInvalidate() { invalidate_requested_ = true; }
void ScriptWindowRootBase::RequestReload() { reload_requested_ = true; }
void ScriptWindowRootBase::RequestClose() { close_requested_ = true; }
uint32_t ScriptWindowRootBase::SetScriptTimer(JSContext* context, JSValueConst callback, uint32_t delay_ms, bool repeat)
{
    return timers_.SetTimer(context, callback, delay_ms, repeat);
}
void ScriptWindowRootBase::ClearScriptTimer(uint32_t id) { timers_.ClearTimer(id); }

void ScriptWindowRootBase::Render(const UiDrawContext& context)
{
    if (!ready_) {
        RenderError(context);
        return;
    }

    JSContext* js_context = Context();
    script::QuickJsValue app(js_context, AppObject());
    script::QuickJsValue render = script::GetProperty(js_context, app.Get(), "render");
    if (!JS_IsFunction(js_context, render.Get())) {
        SetError(std::string(app_global_name_) + ".render is not a function");
        RenderError(context);
        return;
    }

    script::QuickJsValue canvas(js_context, CreateCanvasObject(js_context));
    script::QuickJsValue env(js_context, CreateRenderEnvironment(js_context, context));
    JSValue args[] = {canvas.Get(), env.Get()};
    active_draw_context_ = &context;
    script::QuickJsValue result = script::Call(js_context, render.Get(), app.Get(), 2, args);
    active_draw_context_ = nullptr;

    if (JS_IsException(result.Get())) {
        script_context_->CaptureException();
        SetError(engine_.TakeExceptionTextUtf8());
        RenderError(context);
        return;
    }
    if (engine_.PumpJobs() < 0) {
        SetError(engine_.TakeExceptionTextUtf8());
    }
}

UiEventResult ScriptWindowRootBase::OnPointerEvent(const UiPointerEvent& event)
{
    if (!ready_) {
        return UiEventResult{.handled = true};
    }
    return DispatchPointerToScript(event);
}

UiEventResult ScriptWindowRootBase::OnKeyEvent(const UiKeyEvent& event)
{
    if (event.type == UiEventType::KeyDown && event.virtual_key == VK_ESCAPE) {
        return UiEventResult{.handled = true, .action = CloseAction()};
    }
    if (event.type == UiEventType::KeyDown && event.virtual_key == VK_F5) {
        ReloadScript();
        return UiEventResult{.handled = true, .value_changed = true};
    }
    if (!ready_) {
        return {};
    }
    return DispatchKeyToScript(event);
}

UiEventResult ScriptWindowRootBase::OnInputEvent(const UiInputEvent& event)
{
    if (!ready_) {
        return {};
    }
    if (event.hwnd != nullptr) {
        timers_.SetHwnd(event.hwnd);
    }
    switch (event.type) {
    case UiEventType::Timer: {
        if (event.timer_id < script::kScriptTimerNativeBase) {
            return {};
        }
        const uint32_t timer_id = static_cast<uint32_t>(event.timer_id - script::kScriptTimerNativeBase);
        if (!timers_.HasTimer(timer_id)) {
            return {};
        }
        bool value_changed = false;
        if (!timers_.OnTimer(timer_id, &value_changed)) {
            SetError(engine_.TakeExceptionTextUtf8());
            return UiEventResult{.handled = true, .value_changed = true};
        }
        const bool wants_reload = reload_requested_;
        const bool wants_close = close_requested_;
        const bool wants_invalidate = value_changed || invalidate_requested_ || wants_reload;
        reload_requested_ = false;
        close_requested_ = false;
        invalidate_requested_ = false;
        if (wants_reload) {
            ReloadScript();
        }
        return UiEventResult{
            .handled = true,
            .action = wants_close ? CloseAction() : kUiActionNone,
            .value_changed = wants_invalidate,
        };
    }
    case UiEventType::TextChar:
    case UiEventType::ImeStartComposition:
    case UiEventType::ImeComposition:
    case UiEventType::ImeEndComposition:
        return DispatchInputToScript(event);
    default:
        return ScriptView::OnInputEvent(event);
    }
}

void ScriptWindowRootBase::ReloadScript()
{
    BeforeReload();
    timers_.ClearAll();
    script_context_.reset();
    script_context_ = engine_.CreateContext();
    ready_ = false;
    error_text_.clear();
    close_requested_ = false;
    reload_requested_ = false;
    invalidate_requested_ = true;
    script_path_ = ScriptPath(script_relative_path_);

    if (script_context_ == nullptr) {
        SetError(engine_.TakeExceptionTextUtf8());
        return;
    }

    JS_SetContextOpaque(Context(), this);
    InstallGlobals();

    std::optional<std::string> source = ReadTextFileUtf8(script_path_);
    if (!source.has_value()) {
        SetError("Could not read " + script_path_.string());
        return;
    }

    const script::QuickJsEvalResult eval = script_context_->EvalScript(*source, script_path_.string());
    if (!eval.ok) {
        SetError(engine_.TakeExceptionTextUtf8());
        return;
    }

    script::QuickJsValue app(Context(), AppObject());
    if (!JS_IsObject(app.Get())) {
        SetError(std::string("globalThis.") + app_global_name_ + " was not defined");
        return;
    }
    OnScriptLoaded();
    ready_ = true;
}

void ScriptWindowRootBase::InstallCustomGlobals(JSValue) {}

UiAction ScriptWindowRootBase::CloseAction() const
{
    return kUiActionNone;
}

UiEventResult ScriptWindowRootBase::FinishEventDispatch(JSValue result)
{
    UiEventResult event_result{};
    JSContext* context = Context();
    script::QuickJsValue result_value(context, result);
    if (JS_IsException(result_value.Get())) {
        script_context_->CaptureException();
        SetError(engine_.TakeExceptionTextUtf8());
        event_result.handled = true;
        event_result.value_changed = true;
        return event_result;
    }

    const bool handled = script::BoolProperty(context, result_value.Get(), "handled", false);
    const std::optional<bool> capture = script::OptionalBoolProperty(context, result_value.Get(), "capture");
    const bool invalidate = script::BoolProperty(context, result_value.Get(), "invalidate", false);
    event_result.ime_caret_point = ImeCaretPointProperty(context, result_value.Get());

    const bool wants_reload = reload_requested_;
    const bool wants_close = close_requested_;
    const bool wants_invalidate = invalidate || invalidate_requested_ || wants_reload;
    reload_requested_ = false;
    close_requested_ = false;
    invalidate_requested_ = false;

    if (wants_reload) {
        ReloadScript();
    }

    event_result.handled = handled || capture.has_value() || wants_invalidate || wants_close || wants_reload;
    if (capture.has_value()) {
        event_result.capture = *capture ? UiCaptureRequest::Capture : UiCaptureRequest::Release;
    }
    if (wants_close) {
        event_result.action = CloseAction();
    }
    event_result.value_changed = wants_invalidate;
    if (engine_.PumpJobs() < 0) {
        SetError(engine_.TakeExceptionTextUtf8());
        event_result.handled = true;
        event_result.value_changed = true;
    }
    return event_result;
}

JSContext* ScriptWindowRootBase::Context() const
{
    return script_context_ != nullptr ? script_context_->Context() : nullptr;
}

JSValue ScriptWindowRootBase::AppObject() const
{
    JSContext* context = Context();
    script::QuickJsValue global = script::GetGlobalObject(context);
    return script::GetProperty(context, global.Get(), app_global_name_).Release();
}

void ScriptWindowRootBase::SetError(std::string text)
{
    ready_ = false;
    error_text_ = text.empty() ? "Unknown JavaScript error" : std::move(text);
}

void ScriptWindowRootBase::SetActiveDrawContext(const UiDrawContext* context)
{
    active_draw_context_ = context;
}

void ScriptWindowRootBase::SetScriptTimerHwnd(HWND hwnd)
{
    timers_.SetHwnd(hwnd);
}

void ScriptWindowRootBase::InstallGlobals()
{
    JSContext* context = Context();
    script::QuickJsValue global = script::GetGlobalObject(context);

    JS_SetPropertyStr(context, global.Get(), "host", CreateHostObject(context));
    InstallTimerGlobals(context, global.Get());
    InstallTypographyGlobals(context, global.Get());

    InstallCustomGlobals(global.Get());
}

void ScriptWindowRootBase::RenderError(const UiDrawContext& context) const
{
    RenderScriptError(context, error_title_, script_path_, error_text_);
}

UiEventResult ScriptWindowRootBase::DispatchPointerToScript(const UiPointerEvent& event)
{
    JSContext* context = Context();
    script::QuickJsValue app(context, AppObject());
    script::QuickJsValue handler = script::GetProperty(context, app.Get(), "pointer");
    if (!JS_IsFunction(context, handler.Get())) {
        return {};
    }
    script::QuickJsValue js_event(context, CreatePointerEvent(context, event));
    JSValue args[] = {js_event.Get()};
    script::QuickJsValue result = script::Call(context, handler.Get(), app.Get(), 1, args);
    return FinishEventDispatch(result.Release());
}

UiEventResult ScriptWindowRootBase::DispatchKeyToScript(const UiKeyEvent& event)
{
    JSContext* context = Context();
    script::QuickJsValue app(context, AppObject());
    script::QuickJsValue handler = script::GetProperty(context, app.Get(), "key");
    if (!JS_IsFunction(context, handler.Get())) {
        return {};
    }
    script::QuickJsValue js_event(context, CreateKeyEvent(context, event));
    JSValue args[] = {js_event.Get()};
    script::QuickJsValue result = script::Call(context, handler.Get(), app.Get(), 1, args);
    return FinishEventDispatch(result.Release());
}

UiEventResult ScriptWindowRootBase::DispatchInputToScript(const UiInputEvent& event)
{
    JSContext* context = Context();
    script::QuickJsValue app(context, AppObject());
    script::QuickJsValue handler = script::GetProperty(context, app.Get(), "input");
    bool use_native_input_event = true;
    if (!JS_IsFunction(context, handler.Get())) {
        if (event.type != UiEventType::TextChar) {
            return {};
        }
        handler.Reset(context, JS_GetPropertyStr(context, app.Get(), "text"));
        if (!JS_IsFunction(context, handler.Get())) {
            return {};
        }
        use_native_input_event = false;
    }
    script::QuickJsValue js_event(
        context,
        use_native_input_event ? CreateInputEvent(context, event) : CreateTextEvent(context, event.character));
    JSValue args[] = {js_event.Get()};
    script::QuickJsValue result = script::Call(context, handler.Get(), app.Get(), 1, args);
    return FinishEventDispatch(result.Release());
}

} // namespace imgviewer
