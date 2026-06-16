#include "v2/imgviewer.script_window_root.hpp"

#include <utility>

#include <d2d1helper.h>

namespace imgviewer::v2 {

ScriptWindowRootBase::ScriptWindowRootBase(
    ScriptEngine& engine,
    const char* script_relative_path,
    const char* app_global_name,
    std::wstring error_title) :
    engine_(engine),
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

void ScriptWindowRootBase::Render(const UiDrawContext& context)
{
    if (!ready_) {
        RenderError(context);
        return;
    }

    JSContext* js_context = Context();
    JSValue app = AppObject();
    JSValue render = JS_GetPropertyStr(js_context, app, "render");
    if (!JS_IsFunction(js_context, render)) {
        JS_FreeValue(js_context, render);
        JS_FreeValue(js_context, app);
        SetError(std::string(app_global_name_) + ".render is not a function");
        RenderError(context);
        return;
    }

    JSValue canvas = CreateCanvasObject(js_context);
    JSValue env = CreateRenderEnvironment(js_context, context);
    JSValue args[] = {canvas, env};
    active_draw_context_ = &context;
    JSValue result = JS_Call(js_context, render, app, 2, args);
    active_draw_context_ = nullptr;
    JS_FreeValue(js_context, env);
    JS_FreeValue(js_context, canvas);
    JS_FreeValue(js_context, render);
    JS_FreeValue(js_context, app);

    if (JS_IsException(result)) {
        JS_FreeValue(js_context, result);
        script_context_->CaptureException();
        SetError(engine_.TakeExceptionTextUtf8());
        RenderError(context);
        return;
    }
    JS_FreeValue(js_context, result);
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
    switch (event.type) {
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

    const ScriptEvalResult eval = script_context_->EvalScript(*source, script_path_.string());
    if (!eval.ok) {
        SetError(engine_.TakeExceptionTextUtf8());
        return;
    }

    JSValue app = AppObject();
    if (!JS_IsObject(app)) {
        JS_FreeValue(Context(), app);
        SetError(std::string("globalThis.") + app_global_name_ + " was not defined");
        return;
    }
    JS_FreeValue(Context(), app);
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
    if (JS_IsException(result)) {
        JS_FreeValue(context, result);
        script_context_->CaptureException();
        SetError(engine_.TakeExceptionTextUtf8());
        event_result.handled = true;
        event_result.value_changed = true;
        return event_result;
    }

    const bool handled = BoolProperty(result, "handled", false);
    const std::optional<bool> capture = OptionalBoolProperty(result, "capture");
    const bool invalidate = BoolProperty(result, "invalidate", false);
    event_result.ime_caret_point = ImeCaretPointProperty(context, result);
    JS_FreeValue(context, result);

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
    JSValue global = JS_GetGlobalObject(context);
    JSValue app = JS_GetPropertyStr(context, global, app_global_name_);
    JS_FreeValue(context, global);
    return app;
}

void ScriptWindowRootBase::SetFunction(JSValue object, const char* name, JSCFunction* function, int length)
{
    JS_SetPropertyStr(Context(), object, name, JS_NewCFunction(Context(), function, name, length));
}

bool ScriptWindowRootBase::BoolProperty(JSValueConst object, const char* name, bool fallback) const
{
    if (!JS_IsObject(object)) {
        return fallback;
    }
    JSValue value = JS_GetPropertyStr(Context(), object, name);
    const bool result = JS_IsUndefined(value) ? fallback : JS_ToBool(Context(), value) != 0;
    JS_FreeValue(Context(), value);
    return result;
}

std::optional<bool> ScriptWindowRootBase::OptionalBoolProperty(JSValueConst object, const char* name) const
{
    if (!JS_IsObject(object)) {
        return std::nullopt;
    }
    JSValue value = JS_GetPropertyStr(Context(), object, name);
    if (JS_IsUndefined(value)) {
        JS_FreeValue(Context(), value);
        return std::nullopt;
    }
    const bool result = JS_ToBool(Context(), value) != 0;
    JS_FreeValue(Context(), value);
    return result;
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

void ScriptWindowRootBase::InstallGlobals()
{
    JSContext* context = Context();
    JSValue global = JS_GetGlobalObject(context);

    JSValue host = JS_NewObject(context);
    SetFunction(host, "invalidate", HostInvalidate, 0);
    SetFunction(host, "reload", HostReload, 0);
    SetFunction(host, "close", HostClose, 0);
    SetFunction(host, "log", HostLog, 1);
    JS_SetPropertyStr(context, global, "host", host);

    InstallCustomGlobals(global);
    JS_FreeValue(context, global);
}

void ScriptWindowRootBase::RenderError(const UiDrawContext& context) const
{
    RenderScriptError(context, error_title_, script_path_, error_text_);
}

UiEventResult ScriptWindowRootBase::DispatchPointerToScript(const UiPointerEvent& event)
{
    JSContext* context = Context();
    JSValue app = AppObject();
    JSValue handler = JS_GetPropertyStr(context, app, "pointer");
    if (!JS_IsFunction(context, handler)) {
        JS_FreeValue(context, handler);
        JS_FreeValue(context, app);
        return {};
    }
    JSValue js_event = CreatePointerEvent(context, event);
    JSValue result = JS_Call(context, handler, app, 1, &js_event);
    JS_FreeValue(context, js_event);
    JS_FreeValue(context, handler);
    JS_FreeValue(context, app);
    return FinishEventDispatch(result);
}

UiEventResult ScriptWindowRootBase::DispatchKeyToScript(const UiKeyEvent& event)
{
    JSContext* context = Context();
    JSValue app = AppObject();
    JSValue handler = JS_GetPropertyStr(context, app, "key");
    if (!JS_IsFunction(context, handler)) {
        JS_FreeValue(context, handler);
        JS_FreeValue(context, app);
        return {};
    }
    JSValue js_event = CreateKeyEvent(context, event);
    JSValue result = JS_Call(context, handler, app, 1, &js_event);
    JS_FreeValue(context, js_event);
    JS_FreeValue(context, handler);
    JS_FreeValue(context, app);
    return FinishEventDispatch(result);
}

UiEventResult ScriptWindowRootBase::DispatchInputToScript(const UiInputEvent& event)
{
    JSContext* context = Context();
    JSValue app = AppObject();
    JSValue handler = JS_GetPropertyStr(context, app, "input");
    bool use_native_input_event = true;
    if (!JS_IsFunction(context, handler)) {
        JS_FreeValue(context, handler);
        if (event.type != UiEventType::TextChar) {
            JS_FreeValue(context, app);
            return {};
        }
        handler = JS_GetPropertyStr(context, app, "text");
        if (!JS_IsFunction(context, handler)) {
            JS_FreeValue(context, handler);
            JS_FreeValue(context, app);
            return {};
        }
        use_native_input_event = false;
    }
    JSValue js_event = use_native_input_event ? CreateInputEvent(context, event) : CreateTextEvent(context, event.character);
    JSValue result = JS_Call(context, handler, app, 1, &js_event);
    JS_FreeValue(context, js_event);
    JS_FreeValue(context, handler);
    JS_FreeValue(context, app);
    return FinishEventDispatch(result);
}

} // namespace imgviewer::v2
