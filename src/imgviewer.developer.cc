#include "imgviewer.developer.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <d2d1helper.h>
#include <quickjs.h>
#include <wil/result_macros.h>

#include "experimental/util.signal.hpp"
#include "imgviewer.hpp"
#include "imgviewer.action.hpp"
#include "imgviewer.messages.hpp"
#include "imgviewer.strings.hpp"
#include "imgviewer.ui.action.hpp"
#include "script.canvas_color.hpp"
#include "script.quickjs_runtime.hpp"
#include "ui.draw.hpp"
#include "ui.element.hpp"
#include "ui.theme.hpp"
#include "ui.window.hpp"
#include "win32.util.hpp"

namespace {

constexpr int kDeveloperInitialWidth = 920;
constexpr int kDeveloperInitialHeight = 720;
constexpr int kDeveloperMinClientWidth = 360;
constexpr int kDeveloperMinClientHeight = 260;
constexpr char kDeveloperScriptRelativePath[] = "scripts/developer_ui.js";
constexpr char kDeveloperCounterSignalName[] = "developer.counter";

std::wstring WideFromUtf8(std::string_view text)
{
    if (text.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0);
    if (length <= 0) {
        return {};
    }
    std::wstring value(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        value.data(),
        length);
    return value;
}

std::string Utf8FromValue(JSContext* context, JSValueConst value)
{
    const char* text = JS_ToCString(context, value);
    if (text == nullptr) {
        return {};
    }
    std::string result(text);
    JS_FreeCString(context, text);
    return result;
}

std::optional<std::string> ReadTextFileUtf8(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

std::filesystem::path ExecutableDirectory()
{
    std::vector<wchar_t> buffer(MAX_PATH);
    DWORD length = 0;
    while (true) {
        length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return std::filesystem::current_path();
        }
        if (length < buffer.size() - 1) {
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    std::filesystem::path path(std::wstring(buffer.data(), length));
    return path.parent_path();
}

std::filesystem::path DeveloperScriptPath()
{
    return ExecutableDirectory() / kDeveloperScriptRelativePath;
}

const char* PointerTypeName(UiEventType type)
{
    switch (type) {
    case UiEventType::PointerMove:
        return "move";
    case UiEventType::PointerDown:
        return "down";
    case UiEventType::PointerUp:
        return "up";
    case UiEventType::PointerLeave:
        return "leave";
    case UiEventType::PointerWheel:
        return "wheel";
    default:
        return "move";
    }
}

const char* PointerButtonName(UiPointerButton button)
{
    switch (button) {
    case UiPointerButton::Left:
        return "left";
    case UiPointerButton::Right:
        return "right";
    case UiPointerButton::Middle:
        return "middle";
    default:
        return "none";
    }
}

const char* KeyTypeName(UiEventType type)
{
    return type == UiEventType::KeyUp ? "up" : "down";
}

class DeveloperScriptUi;

DeveloperScriptUi* ScriptUi(JSContext* context)
{
    return static_cast<DeveloperScriptUi*>(JS_GetContextOpaque(context));
}

class DeveloperScriptUi final : public UiRoot {
public:
    DeveloperScriptUi()
        : root_(std::make_unique<UiElement>(
              UiRootMetadata(UiElementRole::Pane, ImgViewerString(ImgViewerStringId::Developer), false, true))),
          counter_signal_(0)
    {
        ReloadScript();
    }

    ~DeveloperScriptUi() override
    {
        ClearSubscriptions();
    }

    UiElement* Root() override { return root_.get(); }
    const UiElement* Root() const override { return root_.get(); }
    const wchar_t* AccessibilityRootName() const override { return ImgViewerString(ImgViewerStringId::Developer); }

    D2D1_SIZE_F Measure(const UiDrawContext&, D2D1_SIZE_F available_size) override
    {
        return available_size;
    }

    void Arrange(D2D1_RECT_F final_rect) override
    {
        root_->Arrange(final_rect);
        rect_ = final_rect;
    }

    void Render(const UiDrawContext& context, UiRootState state) override
    {
        const UiDraw draw(context);
        if (!ready_) {
            RenderError(context);
            return;
        }

        JSContext* js_context = runtime_->Context();
        JSValue app = AppObject();
        JSValue render = JS_GetPropertyStr(js_context, app, "render");
        if (!JS_IsFunction(js_context, render)) {
            JS_FreeValue(js_context, render);
            JS_FreeValue(js_context, app);
            SetError("imgviewerDeveloperUi.render is not a function");
            RenderError(context);
            return;
        }

        JSValue canvas = CreateCanvasObject();
        JSValue env = CreateRenderEnvironment(context, state);
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
            runtime_->CaptureException();
            SetError(runtime_->TakeExceptionTextUtf8());
            RenderError(context);
            return;
        }
        JS_FreeValue(js_context, result);
    }

    UiEventResult OnPointerEvent(const UiPointerEvent& event) override
    {
        if (!ready_) {
            return UiEventResult{.handled = true};
        }
        return DispatchPointerToScript(event);
    }

    UiEventResult OnKeyEvent(const UiKeyEvent& event) override
    {
        if (event.type == UiEventType::KeyDown && event.virtual_key == VK_ESCAPE) {
            return UiEventResult{.handled = true, .action = ImgViewerAction::CloseDeveloper};
        }
        if (event.type == UiEventType::KeyDown && event.virtual_key == VK_F5) {
            ReloadScript();
            return UiEventResult{.handled = true};
        }
        if (!ready_) {
            return {};
        }
        return DispatchKeyToScript(event);
    }

private:
    struct SignalSubscription final {
        size_t native_id = 0;
        int js_id = 0;
        JSValue callback = JS_UNDEFINED;
    };

    static JSValue HostInvalidate(JSContext* context, JSValueConst, int, JSValueConst*)
    {
        if (DeveloperScriptUi* ui = ScriptUi(context)) {
            ui->invalidate_requested_ = true;
        }
        return JS_UNDEFINED;
    }

    static JSValue HostReload(JSContext* context, JSValueConst, int, JSValueConst*)
    {
        if (DeveloperScriptUi* ui = ScriptUi(context)) {
            ui->reload_requested_ = true;
        }
        return JS_UNDEFINED;
    }

    static JSValue HostClose(JSContext* context, JSValueConst, int, JSValueConst*)
    {
        if (DeveloperScriptUi* ui = ScriptUi(context)) {
            ui->close_requested_ = true;
        }
        return JS_UNDEFINED;
    }

    static JSValue HostLog(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
    {
        if (argc > 0) {
            const std::string text = Utf8FromValue(context, argv[0]);
            OutputDebugStringW(WideFromUtf8(text + "\n").c_str());
        }
        return JS_UNDEFINED;
    }

    static JSValue SignalsGet(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
    {
        DeveloperScriptUi* ui = ScriptUi(context);
        if (ui == nullptr || argc < 1 || Utf8FromValue(context, argv[0]) != kDeveloperCounterSignalName) {
            return JS_UNDEFINED;
        }
        return JS_NewInt32(context, ui->counter_signal_.Get());
    }

    static JSValue SignalsSet(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
    {
        DeveloperScriptUi* ui = ScriptUi(context);
        if (ui == nullptr || argc < 2 || Utf8FromValue(context, argv[0]) != kDeveloperCounterSignalName) {
            return JS_FALSE;
        }
        int32_t value = 0;
        if (JS_ToInt32(context, &value, argv[1]) < 0) {
            return JS_EXCEPTION;
        }
        return JS_NewBool(context, ui->counter_signal_.Set(value));
    }

    static JSValue SignalsSubscribe(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
    {
        DeveloperScriptUi* ui = ScriptUi(context);
        if (ui == nullptr || argc < 2 || Utf8FromValue(context, argv[0]) != kDeveloperCounterSignalName ||
            !JS_IsFunction(context, argv[1])) {
            return JS_NewInt32(context, 0);
        }

        const int js_id = ui->next_subscription_id_++;
        SignalSubscription subscription;
        subscription.js_id = js_id;
        subscription.callback = JS_DupValue(context, argv[1]);
        subscription.native_id = ui->counter_signal_.Subscribe([ui, js_id](int value) {
            ui->NotifySignalSubscription(js_id, value);
        });
        ui->subscriptions_.push_back(subscription);
        return JS_NewInt32(context, js_id);
    }

    static JSValue SignalsUnsubscribe(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
    {
        DeveloperScriptUi* ui = ScriptUi(context);
        if (ui == nullptr || argc < 1) {
            return JS_FALSE;
        }
        int32_t js_id = 0;
        if (JS_ToInt32(context, &js_id, argv[0]) < 0) {
            return JS_EXCEPTION;
        }
        return JS_NewBool(context, ui->Unsubscribe(js_id));
    }

    static JSValue CanvasClear(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
    {
        DeveloperScriptUi* ui = ScriptUi(context);
        if (ui == nullptr || ui->active_draw_context_ == nullptr || argc < 1) {
            return JS_UNDEFINED;
        }
        const std::optional<D2D1_COLOR_F> color = script::ParseCanvasColor(Utf8FromValue(context, argv[0]));
        if (color.has_value()) {
            UiDraw(*ui->active_draw_context_).Clear(*color);
        }
        return JS_UNDEFINED;
    }

    static JSValue CanvasFillRect(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
    {
        DeveloperScriptUi* ui = ScriptUi(context);
        if (ui == nullptr || ui->active_draw_context_ == nullptr || argc < 5) {
            return JS_UNDEFINED;
        }
        double x = 0.0, y = 0.0, width = 0.0, height = 0.0;
        JS_ToFloat64(context, &x, argv[0]);
        JS_ToFloat64(context, &y, argv[1]);
        JS_ToFloat64(context, &width, argv[2]);
        JS_ToFloat64(context, &height, argv[3]);
        const std::optional<D2D1_COLOR_F> color = script::ParseCanvasColor(Utf8FromValue(context, argv[4]));
        if (color.has_value()) {
            UiDraw(*ui->active_draw_context_).FillRect(
                D2D1::RectF(static_cast<float>(x), static_cast<float>(y), static_cast<float>(x + width), static_cast<float>(y + height)),
                *color);
        }
        return JS_UNDEFINED;
    }

    static JSValue CanvasStrokeRect(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
    {
        DeveloperScriptUi* ui = ScriptUi(context);
        if (ui == nullptr || ui->active_draw_context_ == nullptr || argc < 5) {
            return JS_UNDEFINED;
        }
        double x = 0.0, y = 0.0, width = 0.0, height = 0.0, stroke_width = 1.0;
        JS_ToFloat64(context, &x, argv[0]);
        JS_ToFloat64(context, &y, argv[1]);
        JS_ToFloat64(context, &width, argv[2]);
        JS_ToFloat64(context, &height, argv[3]);
        if (argc > 5) {
            JS_ToFloat64(context, &stroke_width, argv[5]);
        }
        const std::optional<D2D1_COLOR_F> color = script::ParseCanvasColor(Utf8FromValue(context, argv[4]));
        if (color.has_value()) {
            UiDraw(*ui->active_draw_context_).DrawRect(
                D2D1::RectF(static_cast<float>(x), static_cast<float>(y), static_cast<float>(x + width), static_cast<float>(y + height)),
                *color,
                static_cast<float>(stroke_width));
        }
        return JS_UNDEFINED;
    }

    static JSValue CanvasFillText(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
    {
        DeveloperScriptUi* ui = ScriptUi(context);
        if (ui == nullptr || ui->active_draw_context_ == nullptr || argc < 6) {
            return JS_UNDEFINED;
        }
        const std::wstring text = WideFromUtf8(Utf8FromValue(context, argv[0]));
        double x = 0.0, y = 0.0, width = 0.0, height = 0.0;
        JS_ToFloat64(context, &x, argv[1]);
        JS_ToFloat64(context, &y, argv[2]);
        JS_ToFloat64(context, &width, argv[3]);
        JS_ToFloat64(context, &height, argv[4]);
        const std::optional<D2D1_COLOR_F> color = script::ParseCanvasColor(Utf8FromValue(context, argv[5]));
        if (color.has_value()) {
            UiDraw(*ui->active_draw_context_).DrawBodyText(
                text,
                D2D1::RectF(static_cast<float>(x), static_cast<float>(y), static_cast<float>(x + width), static_cast<float>(y + height)),
                *color,
                D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
        return JS_UNDEFINED;
    }

    static JSValue CanvasStrokeLine(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
    {
        DeveloperScriptUi* ui = ScriptUi(context);
        if (ui == nullptr || ui->active_draw_context_ == nullptr || argc < 5) {
            return JS_UNDEFINED;
        }
        double x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0, stroke_width = 1.0;
        JS_ToFloat64(context, &x1, argv[0]);
        JS_ToFloat64(context, &y1, argv[1]);
        JS_ToFloat64(context, &x2, argv[2]);
        JS_ToFloat64(context, &y2, argv[3]);
        if (argc > 5) {
            JS_ToFloat64(context, &stroke_width, argv[5]);
        }
        const std::optional<D2D1_COLOR_F> color = script::ParseCanvasColor(Utf8FromValue(context, argv[4]));
        if (color.has_value() && ui->active_draw_context_->d2d_context != nullptr) {
            wil::com_ptr<ID2D1SolidColorBrush> brush;
            if (SUCCEEDED(ui->active_draw_context_->d2d_context->CreateSolidColorBrush(*color, brush.put()))) {
                ui->active_draw_context_->d2d_context->DrawLine(
                    D2D1::Point2F(static_cast<float>(x1), static_cast<float>(y1)),
                    D2D1::Point2F(static_cast<float>(x2), static_cast<float>(y2)),
                    brush.get(),
                    static_cast<float>(stroke_width));
            }
        }
        return JS_UNDEFINED;
    }

    void ReloadScript()
    {
        ClearSubscriptions();
        runtime_ = std::make_unique<script::QuickJsRuntime>();
        ready_ = false;
        error_text_.clear();
        reload_requested_ = false;
        close_requested_ = false;
        invalidate_requested_ = true;

        if (!runtime_->Initialize()) {
            SetError(runtime_->TakeExceptionTextUtf8());
            return;
        }

        JS_SetContextOpaque(runtime_->Context(), this);
        InstallGlobals();

        script_path_ = DeveloperScriptPath();
        std::optional<std::string> source = ReadTextFileUtf8(script_path_);
        if (!source.has_value()) {
            SetError("Could not read " + script_path_.string());
            return;
        }

        const script::QuickJsEvalResult eval = runtime_->EvalScript(*source, script_path_.string());
        if (!eval.ok) {
            SetError(runtime_->TakeExceptionTextUtf8());
            return;
        }

        JSValue app = AppObject();
        if (!JS_IsObject(app)) {
            JS_FreeValue(runtime_->Context(), app);
            SetError("globalThis.imgviewerDeveloperUi was not defined");
            return;
        }
        JS_FreeValue(runtime_->Context(), app);
        ready_ = true;
    }

    void InstallGlobals()
    {
        JSContext* context = runtime_->Context();
        JSValue global = JS_GetGlobalObject(context);

        JSValue host = JS_NewObject(context);
        SetFunction(host, "invalidate", HostInvalidate, 0);
        SetFunction(host, "reload", HostReload, 0);
        SetFunction(host, "close", HostClose, 0);
        SetFunction(host, "log", HostLog, 1);
        JS_SetPropertyStr(context, global, "host", host);

        JSValue signals = JS_NewObject(context);
        SetFunction(signals, "get", SignalsGet, 1);
        SetFunction(signals, "set", SignalsSet, 2);
        SetFunction(signals, "subscribe", SignalsSubscribe, 2);
        SetFunction(signals, "unsubscribe", SignalsUnsubscribe, 1);
        JS_SetPropertyStr(context, global, "signals", signals);

        JS_FreeValue(context, global);
    }

    void SetFunction(JSValue object, const char* name, JSCFunction* function, int length)
    {
        JS_SetPropertyStr(runtime_->Context(), object, name, JS_NewCFunction(runtime_->Context(), function, name, length));
    }

    JSValue AppObject() const
    {
        JSContext* context = runtime_->Context();
        JSValue global = JS_GetGlobalObject(context);
        JSValue app = JS_GetPropertyStr(context, global, "imgviewerDeveloperUi");
        JS_FreeValue(context, global);
        return app;
    }

    JSValue CreateCanvasObject()
    {
        JSContext* context = runtime_->Context();
        JSValue canvas = JS_NewObject(context);
        JS_SetPropertyStr(context, canvas, "clear", JS_NewCFunction(context, CanvasClear, "clear", 1));
        JS_SetPropertyStr(context, canvas, "fillRect", JS_NewCFunction(context, CanvasFillRect, "fillRect", 5));
        JS_SetPropertyStr(context, canvas, "strokeRect", JS_NewCFunction(context, CanvasStrokeRect, "strokeRect", 6));
        JS_SetPropertyStr(context, canvas, "fillText", JS_NewCFunction(context, CanvasFillText, "fillText", 6));
        JS_SetPropertyStr(context, canvas, "strokeLine", JS_NewCFunction(context, CanvasStrokeLine, "strokeLine", 6));
        return canvas;
    }

    JSValue CreateRenderEnvironment(const UiDrawContext& context, UiRootState state)
    {
        JSContext* js_context = runtime_->Context();
        JSValue env = JS_NewObject(js_context);
        JS_SetPropertyStr(js_context, env, "width", JS_NewFloat64(js_context, context.viewport_size.width));
        JS_SetPropertyStr(js_context, env, "height", JS_NewFloat64(js_context, context.viewport_size.height));
        JS_SetPropertyStr(js_context, env, "dpiScale", JS_NewFloat64(js_context, context.dpi_scale));
        JS_SetPropertyStr(js_context, env, "hovered", JS_NewBool(js_context, state.hovered != UiElementId::None));
        JS_SetPropertyStr(js_context, env, "pressed", JS_NewBool(js_context, state.pressed != UiElementId::None));
        JS_SetPropertyStr(js_context, env, "focused", JS_NewBool(js_context, state.focused != UiElementId::None));
        return env;
    }

    JSValue CreatePointerEvent(const UiPointerEvent& event)
    {
        JSContext* context = runtime_->Context();
        JSValue value = JS_NewObject(context);
        JS_SetPropertyStr(context, value, "type", JS_NewString(context, PointerTypeName(event.type)));
        JS_SetPropertyStr(context, value, "x", JS_NewFloat64(context, event.point.x));
        JS_SetPropertyStr(context, value, "y", JS_NewFloat64(context, event.point.y));
        JS_SetPropertyStr(context, value, "button", JS_NewString(context, PointerButtonName(event.button)));
        JS_SetPropertyStr(context, value, "wheelDelta", JS_NewInt32(context, event.wheel_delta));
        AddModifiers(value, event.modifiers);
        return value;
    }

    JSValue CreateKeyEvent(const UiKeyEvent& event)
    {
        JSContext* context = runtime_->Context();
        JSValue value = JS_NewObject(context);
        JS_SetPropertyStr(context, value, "type", JS_NewString(context, KeyTypeName(event.type)));
        JS_SetPropertyStr(context, value, "virtualKey", JS_NewInt32(context, static_cast<int32_t>(event.virtual_key)));
        JS_SetPropertyStr(context, value, "repeat", JS_NewBool(context, event.repeat));
        AddModifiers(value, event.modifiers);
        return value;
    }

    void AddModifiers(JSValue object, UiModifiers modifiers)
    {
        JSContext* context = runtime_->Context();
        JS_SetPropertyStr(context, object, "ctrl", JS_NewBool(context, modifiers.ctrl));
        JS_SetPropertyStr(context, object, "shift", JS_NewBool(context, modifiers.shift));
        JS_SetPropertyStr(context, object, "alt", JS_NewBool(context, modifiers.alt));
    }

    UiEventResult DispatchPointerToScript(const UiPointerEvent& event)
    {
        JSContext* context = runtime_->Context();
        JSValue app = AppObject();
        JSValue handler = JS_GetPropertyStr(context, app, "pointer");
        if (!JS_IsFunction(context, handler)) {
            JS_FreeValue(context, handler);
            JS_FreeValue(context, app);
            return {};
        }

        JSValue js_event = CreatePointerEvent(event);
        JSValue args[] = {js_event};
        JSValue result = JS_Call(context, handler, app, 1, args);
        JS_FreeValue(context, js_event);
        JS_FreeValue(context, handler);
        JS_FreeValue(context, app);
        return FinishEventDispatch(result);
    }

    UiEventResult DispatchKeyToScript(const UiKeyEvent& event)
    {
        JSContext* context = runtime_->Context();
        JSValue app = AppObject();
        JSValue handler = JS_GetPropertyStr(context, app, "key");
        if (!JS_IsFunction(context, handler)) {
            JS_FreeValue(context, handler);
            JS_FreeValue(context, app);
            return {};
        }

        JSValue js_event = CreateKeyEvent(event);
        JSValue args[] = {js_event};
        JSValue result = JS_Call(context, handler, app, 1, args);
        JS_FreeValue(context, js_event);
        JS_FreeValue(context, handler);
        JS_FreeValue(context, app);
        return FinishEventDispatch(result);
    }

    UiEventResult FinishEventDispatch(JSValue result)
    {
        UiEventResult event_result{};
        if (JS_IsException(result)) {
            JS_FreeValue(runtime_->Context(), result);
            runtime_->CaptureException();
            SetError(runtime_->TakeExceptionTextUtf8());
            event_result.handled = true;
            return event_result;
        }

        const bool handled = BoolProperty(result, "handled", false);
        const std::optional<bool> capture = OptionalBoolProperty(result, "capture");
        const bool invalidate = BoolProperty(result, "invalidate", false);
        JS_FreeValue(runtime_->Context(), result);

        if (reload_requested_) {
            ReloadScript();
        }
        event_result.handled = handled || capture.has_value() || invalidate || close_requested_ || reload_requested_;
        if (capture.has_value()) {
            event_result.capture = *capture ? UiCaptureRequest::Capture : UiCaptureRequest::Release;
        }
        if (close_requested_) {
            close_requested_ = false;
            event_result.action = ImgViewerAction::CloseDeveloper;
        }
        if (invalidate || invalidate_requested_) {
            invalidate_requested_ = false;
            event_result.value_changed = true;
        }
        return event_result;
    }

    bool BoolProperty(JSValueConst object, const char* name, bool fallback)
    {
        if (!JS_IsObject(object)) {
            return fallback;
        }
        JSContext* context = runtime_->Context();
        JSValue value = JS_GetPropertyStr(context, object, name);
        const bool result = JS_IsUndefined(value) ? fallback : JS_ToBool(context, value) != 0;
        JS_FreeValue(context, value);
        return result;
    }

    std::optional<bool> OptionalBoolProperty(JSValueConst object, const char* name)
    {
        if (!JS_IsObject(object)) {
            return std::nullopt;
        }
        JSContext* context = runtime_->Context();
        JSValue value = JS_GetPropertyStr(context, object, name);
        if (JS_IsUndefined(value)) {
            JS_FreeValue(context, value);
            return std::nullopt;
        }
        const bool result = JS_ToBool(context, value) != 0;
        JS_FreeValue(context, value);
        return result;
    }

    void NotifySignalSubscription(int js_id, int value)
    {
        if (runtime_ == nullptr || runtime_->Context() == nullptr) {
            return;
        }
        auto it = std::find_if(subscriptions_.begin(), subscriptions_.end(), [js_id](const SignalSubscription& item) {
            return item.js_id == js_id;
        });
        if (it == subscriptions_.end()) {
            return;
        }

        JSContext* context = runtime_->Context();
        JSValue arg = JS_NewInt32(context, value);
        JSValue result = JS_Call(context, it->callback, JS_UNDEFINED, 1, &arg);
        JS_FreeValue(context, arg);
        if (JS_IsException(result)) {
            JS_FreeValue(context, result);
            runtime_->CaptureException();
            SetError(runtime_->TakeExceptionTextUtf8());
            return;
        }
        JS_FreeValue(context, result);
        runtime_->PumpJobs();
        invalidate_requested_ = true;
    }

    bool Unsubscribe(int js_id)
    {
        auto it = std::find_if(subscriptions_.begin(), subscriptions_.end(), [js_id](const SignalSubscription& item) {
            return item.js_id == js_id;
        });
        if (it == subscriptions_.end()) {
            return false;
        }
        counter_signal_.Unsubscribe(it->native_id);
        JS_FreeValue(runtime_->Context(), it->callback);
        subscriptions_.erase(it);
        return true;
    }

    void ClearSubscriptions()
    {
        if (runtime_ == nullptr || runtime_->Context() == nullptr) {
            subscriptions_.clear();
            return;
        }
        for (const SignalSubscription& subscription : subscriptions_) {
            counter_signal_.Unsubscribe(subscription.native_id);
            JS_FreeValue(runtime_->Context(), subscription.callback);
        }
        subscriptions_.clear();
    }

    void RenderError(const UiDrawContext& context) const
    {
        const UiDraw draw(context);
        draw.Clear(ui_theme::color::kWindowBackground);
        draw.DrawBodyText(
            L"Developer TypeScript UI failed",
            D2D1::RectF(24.0f, 24.0f, context.viewport_size.width - 24.0f, 52.0f),
            ui_theme::color::kBodyText,
            D2D1_DRAW_TEXT_OPTIONS_CLIP);
        const std::wstring script = L"Script: " + script_path_.wstring();
        draw.DrawBodyText(
            script,
            D2D1::RectF(24.0f, 60.0f, context.viewport_size.width - 24.0f, 84.0f),
            ui_theme::color::kMutedText,
            D2D1_DRAW_TEXT_OPTIONS_CLIP);
        draw.DrawBodyText(
            WideFromUtf8(error_text_),
            D2D1::RectF(24.0f, 96.0f, context.viewport_size.width - 24.0f, context.viewport_size.height - 56.0f),
            ui_theme::color::kBodyText,
            D2D1_DRAW_TEXT_OPTIONS_CLIP);
        draw.DrawBodyText(
            L"Press F5 to reload. Press Esc to close.",
            D2D1::RectF(24.0f, context.viewport_size.height - 44.0f, context.viewport_size.width - 24.0f, context.viewport_size.height - 16.0f),
            ui_theme::color::kMutedText,
            D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    void SetError(std::string text)
    {
        ready_ = false;
        error_text_ = text.empty() ? "Unknown JavaScript error" : std::move(text);
    }

    std::unique_ptr<UiElement> root_;
    std::unique_ptr<script::QuickJsRuntime> runtime_;
    util::Signal<int> counter_signal_;
    std::vector<SignalSubscription> subscriptions_;
    const UiDrawContext* active_draw_context_ = nullptr;
    std::filesystem::path script_path_;
    D2D1_RECT_F rect_ = {};
    std::string error_text_;
    int next_subscription_id_ = 1;
    bool ready_ = false;
    bool invalidate_requested_ = false;
    bool reload_requested_ = false;
    bool close_requested_ = false;
};

struct DeveloperWindowContext final : public UiWindowDelegate {
    HWND owner = nullptr;
    UiWindowHost host;
    bool standalone = false;

    void OnDestroy(UiWindowHost&) override
    {
        if (owner != nullptr) {
            PostMessageW(owner, kImgViewerOwnedWindowDestroyedMessage, 0, reinterpret_cast<LPARAM>(static_cast<UiWindowDelegate*>(this)));
        } else if (standalone) {
            PostQuitMessage(0);
        }
    }

    bool OnUiAction(UiWindowHost& window_host, UiAction action) override
    {
        if (ImgViewerActionFromUiAction(action) == ImgViewerAction::CloseDeveloper) {
            window_host.Close();
            return true;
        }
        return false;
    }

    win32::WindowMessageResult OnUnhandledMessage(
        UiWindowHost& window_host,
        UINT message,
        WPARAM,
        LPARAM lparam) override
    {
        if (message == WM_GETMINMAXINFO) {
            util::ApplyMinTrackSize(window_host.Hwnd(), lparam, kDeveloperMinClientWidth, kDeveloperMinClientHeight);
            return win32::WindowMessageResult::Handled();
        }
        return win32::WindowMessageResult::Unhandled();
    }
};

} // namespace

HRESULT OpenImgViewerDeveloperWindow(HWND owner, ImgViewerContext* context)
{
#if defined(IMGVIEWER_ENABLE_DEVELOPER_WINDOW)
    RETURN_HR_IF_NULL(E_INVALIDARG, context);

    if (context->developer_context != nullptr) {
        auto* developer_context = static_cast<DeveloperWindowContext*>(context->developer_context);
        if (developer_context->host.Hwnd() != nullptr && IsWindow(developer_context->host.Hwnd())) {
            ShowWindow(developer_context->host.Hwnd(), SW_SHOWNORMAL);
            SetForegroundWindow(developer_context->host.Hwnd());
            return S_OK;
        }
    }

    HINSTANCE instance = owner != nullptr
        ? reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner, GWLP_HINSTANCE))
        : GetModuleHandleW(nullptr);
    RETURN_HR_IF_NULL(E_UNEXPECTED, instance);

    auto* developer_context = new (std::nothrow) DeveloperWindowContext();
    RETURN_IF_NULL_ALLOC(developer_context);
    developer_context->owner = owner;
    context->developer_context = developer_context;

    auto root = std::make_unique<DeveloperScriptUi>();
    const HRESULT create_hr = developer_context->host.Create(
        UiWindowOptions{
            .native = win32::NativeWindowOptions{
                .instance = instance,
                .title = ImgViewerString(ImgViewerStringId::Developer),
                .frame = win32::NativeWindowFrame::Dialog,
                .width = kDeveloperInitialWidth,
                .height = kDeveloperInitialHeight,
                .owner = owner,
            },
            .action_message = kImgViewerUiActionMessage,
            .body_font_size = 9.0f,
            .icon_font_size = 11.0f,
        },
        std::move(root),
        developer_context,
        &context->graphics_device);
    if (FAILED(create_hr)) {
        context->developer_context = nullptr;
        delete developer_context;
        RETURN_IF_FAILED(create_hr);
    }

    developer_context->host.Window().Show(SW_SHOWNORMAL);
    context->interaction.SetModal(ImgViewerModalOwner::Developer);
    return S_OK;
#else
    UNREFERENCED_PARAMETER(owner);
    UNREFERENCED_PARAMETER(context);
    return S_FALSE;
#endif
}

HRESULT RunImgViewerDeveloperWindowApplication()
{
#if defined(IMGVIEWER_ENABLE_DEVELOPER_WINDOW)
    RETURN_IF_FAILED(util::InitializeDpiAwareness());
    const HRESULT co_initialize_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    RETURN_IF_FAILED(co_initialize_result);
    auto co_uninitialize = wil::scope_exit([] { CoUninitialize(); });

    ImgViewerContext context;
    RETURN_IF_FAILED(LoadImgViewerConfig(&context.config));
    SetImgViewerLanguage(context.config.language);
    RETURN_IF_FAILED(context.graphics_device.Initialize());

    RETURN_IF_FAILED(OpenImgViewerDeveloperWindow(nullptr, &context));
    auto* developer_context = static_cast<DeveloperWindowContext*>(context.developer_context);
    if (developer_context != nullptr) {
        developer_context->standalone = true;
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
#else
    return E_NOTIMPL;
#endif
}
