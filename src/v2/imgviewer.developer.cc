#include "imgviewer.developer.hpp"

#include <algorithm>
#include <filesystem>
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
#include "ui.element.hpp"
#include "ui.window.hpp"
#include "v2/imgviewer.script_engine.hpp"
#include "v2/imgviewer.script_ui.hpp"
#include "win32.util.hpp"

namespace {

constexpr int kDeveloperInitialWidth = 920;
constexpr int kDeveloperInitialHeight = 720;
constexpr int kDeveloperMinClientWidth = 360;
constexpr int kDeveloperMinClientHeight = 260;
constexpr char kDeveloperScriptRelativePath[] = "scripts/developer_ui.js";
constexpr char kDeveloperCounterSignalName[] = "developer.counter";

std::filesystem::path DeveloperScriptPath()
{
    return imgviewer::v2::ScriptPath(kDeveloperScriptRelativePath);
}

class DeveloperScriptUi;

DeveloperScriptUi* ScriptUi(JSContext* context)
{
    return static_cast<DeveloperScriptUi*>(JS_GetContextOpaque(context));
}

class DeveloperScriptUi final : public imgviewer::v2::ScriptUiHost, public UiRoot {
public:
    explicit DeveloperScriptUi(imgviewer::v2::ScriptEngine& engine)
        : engine_(engine),
          root_(std::make_unique<UiElement>(
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
    const UiDrawContext* ActiveDrawContext() const override { return active_draw_context_; }
    void RequestInvalidate() override { invalidate_requested_ = true; }
    void RequestReload() override { reload_requested_ = true; }
    void RequestClose() override { close_requested_ = true; }

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

        JSContext* js_context = script_context_->Context();
        JSValue app = AppObject();
        JSValue render = JS_GetPropertyStr(js_context, app, "render");
        if (!JS_IsFunction(js_context, render)) {
            JS_FreeValue(js_context, render);
            JS_FreeValue(js_context, app);
            SetError("imgviewerDeveloperUi.render is not a function");
            RenderError(context);
            return;
        }

        JSValue canvas = imgviewer::v2::CreateCanvasObject(js_context);
        JSValue env = imgviewer::v2::CreateRenderEnvironment(js_context, context, state);
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

    static JSValue SignalsGet(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
    {
        DeveloperScriptUi* ui = ScriptUi(context);
        if (ui == nullptr || argc < 1 || imgviewer::v2::Utf8FromValue(context, argv[0]) != kDeveloperCounterSignalName) {
            return JS_UNDEFINED;
        }
        return JS_NewInt32(context, ui->counter_signal_.Get());
    }

    static JSValue SignalsSet(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
    {
        DeveloperScriptUi* ui = ScriptUi(context);
        if (ui == nullptr || argc < 2 || imgviewer::v2::Utf8FromValue(context, argv[0]) != kDeveloperCounterSignalName) {
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
        if (ui == nullptr || argc < 2 || imgviewer::v2::Utf8FromValue(context, argv[0]) != kDeveloperCounterSignalName ||
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

    void ReloadScript()
    {
        ClearSubscriptions();
        script_context_.reset();
        script_context_ = engine_.CreateContext();
        ready_ = false;
        error_text_.clear();
        reload_requested_ = false;
        close_requested_ = false;
        invalidate_requested_ = true;

        if (script_context_ == nullptr) {
            SetError(engine_.TakeExceptionTextUtf8());
            return;
        }

        JS_SetContextOpaque(script_context_->Context(), this);
        InstallGlobals();

        script_path_ = DeveloperScriptPath();
        std::optional<std::string> source = imgviewer::v2::ReadTextFileUtf8(script_path_);
        if (!source.has_value()) {
            SetError("Could not read " + script_path_.string());
            return;
        }

        const imgviewer::v2::ScriptEvalResult eval = script_context_->EvalScript(*source, script_path_.string());
        if (!eval.ok) {
            SetError(engine_.TakeExceptionTextUtf8());
            return;
        }

        JSValue app = AppObject();
        if (!JS_IsObject(app)) {
            JS_FreeValue(script_context_->Context(), app);
            SetError("globalThis.imgviewerDeveloperUi was not defined");
            return;
        }
        JS_FreeValue(script_context_->Context(), app);
        ready_ = true;
    }

    void InstallGlobals()
    {
        JSContext* context = script_context_->Context();
        JSValue global = JS_GetGlobalObject(context);

        JSValue host = JS_NewObject(context);
        SetFunction(host, "invalidate", imgviewer::v2::HostInvalidate, 0);
        SetFunction(host, "reload", imgviewer::v2::HostReload, 0);
        SetFunction(host, "close", imgviewer::v2::HostClose, 0);
        SetFunction(host, "log", imgviewer::v2::HostLog, 1);
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
        JS_SetPropertyStr(script_context_->Context(), object, name, JS_NewCFunction(script_context_->Context(), function, name, length));
    }

    JSValue AppObject() const
    {
        JSContext* context = script_context_->Context();
        JSValue global = JS_GetGlobalObject(context);
        JSValue app = JS_GetPropertyStr(context, global, "imgviewerDeveloperUi");
        JS_FreeValue(context, global);
        return app;
    }

    UiEventResult DispatchPointerToScript(const UiPointerEvent& event)
    {
        JSContext* context = script_context_->Context();
        JSValue app = AppObject();
        JSValue handler = JS_GetPropertyStr(context, app, "pointer");
        if (!JS_IsFunction(context, handler)) {
            JS_FreeValue(context, handler);
            JS_FreeValue(context, app);
            return {};
        }

        JSValue js_event = imgviewer::v2::CreatePointerEvent(context, event);
        JSValue args[] = {js_event};
        JSValue result = JS_Call(context, handler, app, 1, args);
        JS_FreeValue(context, js_event);
        JS_FreeValue(context, handler);
        JS_FreeValue(context, app);
        return FinishEventDispatch(result);
    }

    UiEventResult DispatchKeyToScript(const UiKeyEvent& event)
    {
        JSContext* context = script_context_->Context();
        JSValue app = AppObject();
        JSValue handler = JS_GetPropertyStr(context, app, "key");
        if (!JS_IsFunction(context, handler)) {
            JS_FreeValue(context, handler);
            JS_FreeValue(context, app);
            return {};
        }

        JSValue js_event = imgviewer::v2::CreateKeyEvent(context, event);
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
            JS_FreeValue(script_context_->Context(), result);
            script_context_->CaptureException();
            SetError(engine_.TakeExceptionTextUtf8());
            event_result.handled = true;
            return event_result;
        }

        const bool handled = BoolProperty(result, "handled", false);
        const std::optional<bool> capture = OptionalBoolProperty(result, "capture");
        const bool invalidate = BoolProperty(result, "invalidate", false);
        event_result.ime_caret_point = imgviewer::v2::ImeCaretPointProperty(script_context_->Context(), result);
        JS_FreeValue(script_context_->Context(), result);

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
        JSContext* context = script_context_->Context();
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
        JSContext* context = script_context_->Context();
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
        if (script_context_ == nullptr || script_context_->Context() == nullptr) {
            return;
        }
        auto it = std::find_if(subscriptions_.begin(), subscriptions_.end(), [js_id](const SignalSubscription& item) {
            return item.js_id == js_id;
        });
        if (it == subscriptions_.end()) {
            return;
        }

        JSContext* context = script_context_->Context();
        JSValue arg = JS_NewInt32(context, value);
        JSValue result = JS_Call(context, it->callback, JS_UNDEFINED, 1, &arg);
        JS_FreeValue(context, arg);
        if (JS_IsException(result)) {
            JS_FreeValue(context, result);
            script_context_->CaptureException();
            SetError(engine_.TakeExceptionTextUtf8());
            return;
        }
        JS_FreeValue(context, result);
        engine_.PumpJobs();
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
        JS_FreeValue(script_context_->Context(), it->callback);
        subscriptions_.erase(it);
        return true;
    }

    void ClearSubscriptions()
    {
        if (script_context_ == nullptr || script_context_->Context() == nullptr) {
            subscriptions_.clear();
            return;
        }
        for (const SignalSubscription& subscription : subscriptions_) {
            counter_signal_.Unsubscribe(subscription.native_id);
            JS_FreeValue(script_context_->Context(), subscription.callback);
        }
        subscriptions_.clear();
    }

    void RenderError(const UiDrawContext& context) const
    {
        imgviewer::v2::RenderScriptError(context, L"Developer TypeScript UI failed", script_path_, error_text_);
    }

    void SetError(std::string text)
    {
        ready_ = false;
        error_text_ = text.empty() ? "Unknown JavaScript error" : std::move(text);
    }

    std::unique_ptr<UiElement> root_;
    imgviewer::v2::ScriptEngine& engine_;
    std::unique_ptr<imgviewer::v2::ScriptContext> script_context_;
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

    auto root = std::make_unique<DeveloperScriptUi>(*context->script_engine);
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
            .script_engine = context->script_engine.get(),
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
