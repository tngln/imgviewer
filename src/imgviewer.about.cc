#include "imgviewer.about.hpp"

#include <filesystem>
#include <memory>
#include <string>

#include <quickjs.h>
#include <wil/result_macros.h>

#include "imgviewer.hpp"
#include "imgviewer.action.hpp"
#include "imgviewer.messages.hpp"
#include "imgviewer.strings.hpp"
#include "imgviewer.ui.action.hpp"
#include "ui.draw.hpp"
#include "ui.element.hpp"
#include "ui.theme.hpp"
#include "ui.window.hpp"
#include "v2/imgviewer.script_engine.hpp"
#include "v2/imgviewer.script_ui.hpp"
#include "win32.util.hpp"

namespace {

constexpr int kAboutInitialWidth = 560;
constexpr int kAboutInitialHeight = 520;
constexpr int kAboutMinClientWidth = 240;
constexpr int kAboutMinClientHeight = 210;
constexpr char kAboutScriptRelativePath[] = "scripts/about_ui.js";

class AboutScriptUi;

AboutScriptUi* ScriptUi(JSContext* context)
{
    return static_cast<AboutScriptUi*>(JS_GetContextOpaque(context));
}

JSValue AboutHostInvalidate(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv)
{
    return imgviewer::v2::HostInvalidate(context, this_value, argc, argv);
}

JSValue AboutHostReload(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv)
{
    return imgviewer::v2::HostReload(context, this_value, argc, argv);
}

JSValue AboutHostClose(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv)
{
    return imgviewer::v2::HostClose(context, this_value, argc, argv);
}

JSValue AboutHostLog(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv)
{
    return imgviewer::v2::HostLog(context, this_value, argc, argv);
}

class AboutScriptUi final : public imgviewer::v2::ScriptUiHost, public UiRoot {
public:
    explicit AboutScriptUi(imgviewer::v2::ScriptEngine& engine)
        : engine_(engine),
          root_(std::make_unique<UiElement>(
              UiRootMetadata(UiElementRole::Pane, ImgViewerString(ImgViewerStringId::AboutImgViewer), false, true)))
    {
        ReloadScript();
    }

    UiElement* Root() override { return root_.get(); }
    const UiElement* Root() const override { return root_.get(); }
    const wchar_t* AccessibilityRootName() const override { return ImgViewerString(ImgViewerStringId::AboutImgViewer); }
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
    }

    void Render(const UiDrawContext& context, UiRootState state) override
    {
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
            SetError("imgviewerAboutUi.render is not a function");
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

    UiEventResult OnKeyEvent(const UiKeyEvent& event) override
    {
        if (event.type == UiEventType::KeyDown && event.virtual_key == VK_ESCAPE) {
            return UiEventResult{.handled = true, .action = ImgViewerAction::CloseAbout};
        }
        if (event.type == UiEventType::KeyDown && event.virtual_key == VK_F5) {
            ReloadScript();
            return UiEventResult{.handled = true};
        }
        if (!ready_) {
            return UiEventResult{.handled = true};
        }

        JSContext* js_context = script_context_->Context();
        JSValue app = AppObject();
        JSValue key = JS_GetPropertyStr(js_context, app, "key");
        if (!JS_IsFunction(js_context, key)) {
            JS_FreeValue(js_context, key);
            JS_FreeValue(js_context, app);
            return {};
        }
        JSValue event_value = imgviewer::v2::CreateKeyEvent(js_context, event);
        JSValue args[] = {event_value};
        JSValue result = JS_Call(js_context, key, app, 1, args);
        JS_FreeValue(js_context, event_value);
        JS_FreeValue(js_context, key);
        JS_FreeValue(js_context, app);
        return FinishEventDispatch(result);
    }

private:
    void ReloadScript()
    {
        ready_ = false;
        error_text_.clear();
        script_path_ = imgviewer::v2::ScriptPath(kAboutScriptRelativePath);
        script_context_ = engine_.CreateContext();
        if (script_context_ == nullptr) {
            SetError("Could not create QuickJS context");
            return;
        }

        JS_SetContextOpaque(script_context_->Context(), this);
        InstallGlobals();
        const std::optional<std::string> source = imgviewer::v2::ReadTextFileUtf8(script_path_);
        if (!source.has_value()) {
            SetError("Could not read script: " + script_path_.string());
            return;
        }

        const imgviewer::v2::ScriptEvalResult result = script_context_->EvalScript(*source, script_path_.string());
        if (!result.ok) {
            SetError(engine_.TakeExceptionTextUtf8());
            return;
        }
        ready_ = true;
    }

    void InstallGlobals()
    {
        JSContext* context = script_context_->Context();
        JSValue global = JS_GetGlobalObject(context);
        JSValue host = JS_NewObject(context);
        JS_SetPropertyStr(context, host, "invalidate", JS_NewCFunction(context, AboutHostInvalidate, "invalidate", 0));
        JS_SetPropertyStr(context, host, "reload", JS_NewCFunction(context, AboutHostReload, "reload", 0));
        JS_SetPropertyStr(context, host, "close", JS_NewCFunction(context, AboutHostClose, "close", 0));
        JS_SetPropertyStr(context, host, "log", JS_NewCFunction(context, AboutHostLog, "log", 1));
        JS_SetPropertyStr(context, global, "host", host);
        JS_FreeValue(context, global);
    }

    JSValue AppObject() const
    {
        JSContext* context = script_context_->Context();
        JSValue global = JS_GetGlobalObject(context);
        JSValue app = JS_GetPropertyStr(context, global, "imgviewerAboutUi");
        JS_FreeValue(context, global);
        return app;
    }

    UiEventResult FinishEventDispatch(JSValue result)
    {
        UiEventResult event_result = {};
        JSContext* context = script_context_->Context();
        if (JS_IsException(result)) {
            JS_FreeValue(context, result);
            script_context_->CaptureException();
            SetError(engine_.TakeExceptionTextUtf8());
            return UiEventResult{.handled = true, .value_changed = true};
        }

        const bool invalidate = BoolProperty(result, "invalidate", false);
        const bool handled = BoolProperty(result, "handled", false);
        JS_FreeValue(context, result);
        engine_.PumpJobs();
        if (reload_requested_) {
            reload_requested_ = false;
            ReloadScript();
        }
        event_result.handled = handled || invalidate || close_requested_;
        if (close_requested_) {
            close_requested_ = false;
            event_result.action = ImgViewerAction::CloseAbout;
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

    void RenderError(const UiDrawContext& context) const
    {
        imgviewer::v2::RenderScriptError(context, L"About TypeScript UI failed", script_path_, error_text_);
    }

    void SetError(std::string text)
    {
        ready_ = false;
        error_text_ = text.empty() ? "Unknown JavaScript error" : std::move(text);
    }

    std::unique_ptr<UiElement> root_;
    imgviewer::v2::ScriptEngine& engine_;
    std::unique_ptr<imgviewer::v2::ScriptContext> script_context_;
    const UiDrawContext* active_draw_context_ = nullptr;
    std::filesystem::path script_path_;
    std::string error_text_;
    bool ready_ = false;
    bool invalidate_requested_ = false;
    bool reload_requested_ = false;
    bool close_requested_ = false;
};

struct AboutWindowContext final : public UiWindowDelegate {
    HWND owner = nullptr;
    UiWindowHost host;

    void OnDestroy(UiWindowHost&) override
    {
        if (owner != nullptr) {
            PostMessageW(owner, kImgViewerOwnedWindowDestroyedMessage, 0, reinterpret_cast<LPARAM>(static_cast<UiWindowDelegate*>(this)));
        }
    }

    bool OnUiAction(UiWindowHost& window_host, UiAction action) override
    {
        if (ImgViewerActionFromUiAction(action) == ImgViewerAction::CloseAbout) {
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
            util::ApplyMinTrackSize(window_host.Hwnd(), lparam, kAboutMinClientWidth, kAboutMinClientHeight);
            return win32::WindowMessageResult::Handled();
        }
        return win32::WindowMessageResult::Unhandled();
    }
};

} // namespace

HRESULT OpenImgViewerAboutWindow(HWND owner, ImgViewerContext* context)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, owner);
    RETURN_HR_IF_NULL(E_INVALIDARG, context);

    if (context->about_context != nullptr) {
        auto* about_context = static_cast<AboutWindowContext*>(context->about_context);
        if (about_context->host.Hwnd() != nullptr && IsWindow(about_context->host.Hwnd())) {
            ShowWindow(about_context->host.Hwnd(), SW_SHOWNORMAL);
            SetForegroundWindow(about_context->host.Hwnd());
            return S_OK;
        }
    }

    HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner, GWLP_HINSTANCE));
    RETURN_HR_IF_NULL(E_UNEXPECTED, instance);

    auto* about_context = new (std::nothrow) AboutWindowContext();
    RETURN_IF_NULL_ALLOC(about_context);
    about_context->owner = owner;
    context->about_context = about_context;

    auto root = std::make_unique<AboutScriptUi>(*context->script_engine);
    const HRESULT create_hr = about_context->host.Create(
        UiWindowOptions{
            .native = win32::NativeWindowOptions{
                .instance = instance,
                .title = ImgViewerString(ImgViewerStringId::AboutImgViewer),
                .frame = win32::NativeWindowFrame::Dialog,
                .width = kAboutInitialWidth,
                .height = kAboutInitialHeight,
                .owner = owner,
            },
            .action_message = kImgViewerUiActionMessage,
            .body_font_size = 9.0f,
            .icon_font_size = 11.0f,
            .script_engine = context->script_engine.get(),
        },
        std::move(root),
        about_context,
        &context->graphics_device);
    if (FAILED(create_hr)) {
        context->about_context = nullptr;
        delete about_context;
        RETURN_IF_FAILED(create_hr);
    }

    about_context->host.Window().Show(SW_SHOWNORMAL);
    context->interaction.SetModal(ImgViewerModalOwner::About);
    return S_OK;
}
