#include "imgviewer.settings.hpp"

#include <cwctype>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <d2d1helper.h>
#include <quickjs.h>
#include <wil/com.h>
#include <wil/result_macros.h>

#include "imgviewer.hpp"
#include "imgviewer.action.hpp"
#include "imgviewer.config.hpp"
#include "imgviewer.keybindings.hpp"
#include "imgviewer.messages.hpp"
#include "imgviewer.strings.hpp"
#include "imgviewer.ui.action.hpp"
#include "ui.element.hpp"
#include "ui.window.hpp"
#include "v2/imgviewer.script_engine.hpp"
#include "v2/imgviewer.script_ui.hpp"
#include "win32.util.hpp"

namespace {

constexpr int kSettingsInitialWidth = 1000;
constexpr int kSettingsInitialHeight = 1300;
constexpr int kSettingsMinClientWidth = 400;
constexpr int kSettingsMinClientHeight = 620;
constexpr char kSettingsScriptRelativePath[] = "scripts/settings_ui.js";

std::filesystem::path SettingsScriptPath()
{
    return imgviewer::v2::ScriptPath(kSettingsScriptRelativePath);
}

std::wstring ShortcutsForAction(const ActionBindings& bindings, ImgViewerAction action)
{
    std::wstring text;
    for (const KeyBinding& binding : bindings.key_bindings) {
        if (binding.action != action) {
            continue;
        }
        if (!text.empty()) {
            text += L", ";
        }
        text += GestureText(binding.gesture);
    }
    return text.empty() ? ImgViewerString(ImgViewerStringId::NoShortcutConfigured) : text;
}

class SettingsScriptUi;

SettingsScriptUi* ScriptUi(JSContext* context)
{
    return static_cast<SettingsScriptUi*>(JS_GetContextOpaque(context));
}

class SettingsScriptUi final : public imgviewer::v2::ScriptUiHost, public UiRoot {
public:
    SettingsScriptUi(imgviewer::v2::ScriptEngine& engine, ImgViewerConfig config) :
        engine_(engine),
        root_(std::make_unique<UiElement>(
            UiRootMetadata(UiElementRole::Pane, ImgViewerString(ImgViewerStringId::Settings), false, true))),
        draft_(std::move(config))
    {
        ReloadScript();
    }

    ImgViewerConfig Draft() const { return draft_; }
    int OpacityPercent() const { return ClampWindowOpacityPercent(draft_.window_opacity_percent); }
    int ToolbarScalePercent() const { return ClampToolbarScalePercent(draft_.toolbar_scale_percent); }

    void SetOpacityPercent(int percent)
    {
        draft_.window_opacity_percent = ClampWindowOpacityPercent(percent);
        invalidate_requested_ = true;
        value_changed_requested_ = true;
    }

    void SetToolbarScalePercent(int percent)
    {
        draft_.toolbar_scale_percent = ClampToolbarScalePercent(percent);
        invalidate_requested_ = true;
        value_changed_requested_ = true;
    }

    UiElement* Root() override { return root_.get(); }
    const UiElement* Root() const override { return root_.get(); }
    const wchar_t* AccessibilityRootName() const override { return ImgViewerString(ImgViewerStringId::Settings); }
    const UiDrawContext* ActiveDrawContext() const override { return active_draw_context_; }
    void RequestInvalidate() override { invalidate_requested_ = true; }
    void RequestReload() override { reload_requested_ = true; }
    void RequestClose() override { close_requested_ = true; }

    D2D1_SIZE_F Measure(const UiDrawContext&, D2D1_SIZE_F available_size) override { return available_size; }

    void Arrange(D2D1_RECT_F final_rect) override
    {
        root_->Arrange(final_rect);
        rect_ = final_rect;
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
            SetError("imgviewerSettingsUi.render is not a function");
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
            return UiEventResult{.handled = true, .action = ImgViewerAction::CloseSettings};
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

    UiEventResult OnInputEvent(const UiInputEvent& event) override
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
            return {};
        }
    }

private:
    static JSValue SettingsGet(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
    {
        SettingsScriptUi* ui = ScriptUi(context);
        if (ui == nullptr || argc < 1) {
            return JS_UNDEFINED;
        }
        const std::string name = imgviewer::v2::Utf8FromValue(context, argv[0]);
        const ImgViewerConfig& config = ui->draft_;
        if (name == "language") {
            return JS_NewInt32(context, config.language == ImgViewerLanguage::SimplifiedChinese ? 1 : 0);
        }
        if (name == "initialImageViewMode") {
            return JS_NewInt32(context, config.initial_image_view_mode == InitialImageViewMode::ActualSize ? 1 : 0);
        }
        if (name == "rememberWindowSize") {
            return JS_NewBool(context, config.remember_window_size);
        }
        if (name == "pixelatedSampling") {
            return JS_NewBool(context, config.pixelated_sampling);
        }
        if (name == "checkerboardBackground") {
            return JS_NewBool(context, config.checkerboard_background);
        }
        if (name == "borderlessWindow") {
            return JS_NewBool(context, config.borderless_window);
        }
        if (name == "edgeClickNavigation") {
            return JS_NewBool(context, config.edge_click_navigation);
        }
        if (name == "windowOpacityPercent") {
            return JS_NewInt32(context, ClampWindowOpacityPercent(config.window_opacity_percent));
        }
        if (name == "toolbarScalePercent") {
            return JS_NewInt32(context, ClampToolbarScalePercent(config.toolbar_scale_percent));
        }
        if (name == "edgeClickNavigationZonePercent") {
            return JS_NewInt32(context, ClampEdgeClickNavigationZonePercent(config.edge_click_navigation_zone_percent));
        }
        return JS_UNDEFINED;
    }

    static JSValue SettingsSet(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
    {
        SettingsScriptUi* ui = ScriptUi(context);
        if (ui == nullptr || argc < 2) {
            return JS_FALSE;
        }
        const std::string name = imgviewer::v2::Utf8FromValue(context, argv[0]);
        ImgViewerConfig& config = ui->draft_;
        bool changed = false;
        if (name == "language") {
            int32_t value = 0;
            if (JS_ToInt32(context, &value, argv[1]) < 0) {
                return JS_EXCEPTION;
            }
            const ImgViewerLanguage next = value == 1 ? ImgViewerLanguage::SimplifiedChinese : ImgViewerLanguage::English;
            changed = config.language != next;
            config.language = next;
        } else if (name == "initialImageViewMode") {
            int32_t value = 0;
            if (JS_ToInt32(context, &value, argv[1]) < 0) {
                return JS_EXCEPTION;
            }
            const InitialImageViewMode next = value == 1 ? InitialImageViewMode::ActualSize : InitialImageViewMode::FitWindow;
            changed = config.initial_image_view_mode != next;
            config.initial_image_view_mode = next;
        } else if (name == "rememberWindowSize") {
            changed = SetBool(JS_ToBool(context, argv[1]) != 0, &config.remember_window_size);
        } else if (name == "pixelatedSampling") {
            changed = SetBool(JS_ToBool(context, argv[1]) != 0, &config.pixelated_sampling);
        } else if (name == "checkerboardBackground") {
            changed = SetBool(JS_ToBool(context, argv[1]) != 0, &config.checkerboard_background);
        } else if (name == "borderlessWindow") {
            changed = SetBool(JS_ToBool(context, argv[1]) != 0, &config.borderless_window);
        } else if (name == "edgeClickNavigation") {
            changed = SetBool(JS_ToBool(context, argv[1]) != 0, &config.edge_click_navigation);
        } else if (name == "windowOpacityPercent") {
            int32_t value = 0;
            if (JS_ToInt32(context, &value, argv[1]) < 0) {
                return JS_EXCEPTION;
            }
            const int next = ClampWindowOpacityPercent(value);
            changed = config.window_opacity_percent != next;
            config.window_opacity_percent = next;
            ui->value_changed_requested_ = true;
        } else if (name == "toolbarScalePercent") {
            int32_t value = 0;
            if (JS_ToInt32(context, &value, argv[1]) < 0) {
                return JS_EXCEPTION;
            }
            const int next = ClampToolbarScalePercent(value);
            changed = config.toolbar_scale_percent != next;
            config.toolbar_scale_percent = next;
            ui->value_changed_requested_ = true;
        } else if (name == "edgeClickNavigationZonePercent") {
            int32_t value = 0;
            if (JS_ToInt32(context, &value, argv[1]) < 0) {
                return JS_EXCEPTION;
            }
            const int next = ClampEdgeClickNavigationZonePercent(value);
            changed = config.edge_click_navigation_zone_percent != next;
            config.edge_click_navigation_zone_percent = next;
        } else {
            return JS_FALSE;
        }
        if (changed) {
            ui->invalidate_requested_ = true;
        }
        return JS_NewBool(context, true);
    }

    static JSValue SettingsSave(JSContext* context, JSValueConst, int, JSValueConst*)
    {
        if (SettingsScriptUi* ui = ScriptUi(context)) {
            ui->save_requested_ = true;
        }
        return JS_UNDEFINED;
    }

    static JSValue SettingsResetKeyBindings(JSContext* context, JSValueConst, int, JSValueConst*)
    {
        if (SettingsScriptUi* ui = ScriptUi(context)) {
            ui->draft_.action_bindings = DefaultActionBindings();
            ui->invalidate_requested_ = true;
        }
        return JS_UNDEFINED;
    }

    static JSValue SettingsActionRows(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
    {
        SettingsScriptUi* ui = ScriptUi(context);
        if (ui == nullptr) {
            return JS_NewArray(context);
        }
        const std::wstring filter = argc > 0
            ? imgviewer::v2::WideFromUtf8(imgviewer::v2::Utf8FromValue(context, argv[0]))
            : std::wstring();
        return ui->CreateActionRows(filter);
    }

    static bool SetBool(bool value, bool* target)
    {
        const bool changed = *target != value;
        *target = value;
        return changed;
    }

    void ReloadScript()
    {
        script_context_.reset();
        script_context_ = engine_.CreateContext();
        ready_ = false;
        error_text_.clear();
        close_requested_ = false;
        save_requested_ = false;
        reload_requested_ = false;
        invalidate_requested_ = true;
        value_changed_requested_ = false;

        if (script_context_ == nullptr) {
            SetError(engine_.TakeExceptionTextUtf8());
            return;
        }

        JS_SetContextOpaque(script_context_->Context(), this);
        InstallGlobals();

        script_path_ = SettingsScriptPath();
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
            SetError("globalThis.imgviewerSettingsUi was not defined");
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

        JSValue settings = JS_NewObject(context);
        SetFunction(settings, "get", SettingsGet, 1);
        SetFunction(settings, "set", SettingsSet, 2);
        SetFunction(settings, "save", SettingsSave, 0);
        SetFunction(settings, "resetKeyBindings", SettingsResetKeyBindings, 0);
        SetFunction(settings, "actionRows", SettingsActionRows, 1);
        JS_SetPropertyStr(context, global, "settings", settings);

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
        JSValue app = JS_GetPropertyStr(context, global, "imgviewerSettingsUi");
        JS_FreeValue(context, global);
        return app;
    }

    JSValue CreateActionRows(std::wstring filter)
    {
        for (wchar_t& ch : filter) {
            ch = static_cast<wchar_t>(towlower(ch));
        }

        JSContext* context = script_context_->Context();
        JSValue rows = JS_NewArray(context);
        uint32_t index = 0;
        for (const ImgViewerActionInfo& action : ImgViewerActions()) {
            if (!action.shown_in_settings) {
                continue;
            }
            const std::wstring name = ImgViewerString(action.display_name);
            const std::wstring shortcut = ShortcutsForAction(draft_.action_bindings, action.action);
            std::wstring haystack = name + L" " + shortcut;
            for (wchar_t& ch : haystack) {
                ch = static_cast<wchar_t>(towlower(ch));
            }
            if (!filter.empty() && haystack.find(filter) == std::wstring::npos) {
                continue;
            }

            JSValue row = JS_NewObject(context);
            JS_SetPropertyStr(context, row, "name", JS_NewString(context, imgviewer::v2::Utf8FromWide(name).c_str()));
            JS_SetPropertyStr(context, row, "shortcut", JS_NewString(context, imgviewer::v2::Utf8FromWide(shortcut).c_str()));
            JS_SetPropertyUint32(context, rows, index++, row);
        }
        return rows;
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
        JSValue result = JS_Call(context, handler, app, 1, &js_event);
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
        JSValue result = JS_Call(context, handler, app, 1, &js_event);
        JS_FreeValue(context, js_event);
        JS_FreeValue(context, handler);
        JS_FreeValue(context, app);
        return FinishEventDispatch(result);
    }

    UiEventResult DispatchInputToScript(const UiInputEvent& event)
    {
        JSContext* context = script_context_->Context();
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
        JSValue js_event = use_native_input_event
            ? imgviewer::v2::CreateInputEvent(context, event)
            : imgviewer::v2::CreateTextEvent(context, event.character);
        JSValue result = JS_Call(context, handler, app, 1, &js_event);
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
            event_result.value_changed = true;
            return event_result;
        }

        const bool handled = BoolProperty(result, "handled", false);
        const std::optional<bool> capture = OptionalBoolProperty(result, "capture");
        const bool invalidate = BoolProperty(result, "invalidate", false);
        event_result.ime_caret_point = imgviewer::v2::ImeCaretPointProperty(script_context_->Context(), result);
        JS_FreeValue(script_context_->Context(), result);

        const bool wants_reload = reload_requested_;
        const bool wants_close = close_requested_;
        const bool wants_save = save_requested_;
        const bool wants_invalidate = invalidate || invalidate_requested_ || wants_reload;
        const bool wants_value_changed = value_changed_requested_;
        reload_requested_ = false;
        close_requested_ = false;
        save_requested_ = false;
        invalidate_requested_ = false;
        value_changed_requested_ = false;

        if (wants_reload) {
            ReloadScript();
        }

        event_result.handled = handled || capture.has_value() || wants_invalidate || wants_close || wants_save || wants_reload;
        if (capture.has_value()) {
            event_result.capture = *capture ? UiCaptureRequest::Capture : UiCaptureRequest::Release;
        }
        if (wants_save) {
            event_result.action = ImgViewerAction::SaveSettings;
        } else if (wants_close) {
            event_result.action = ImgViewerAction::CloseSettings;
        }
        event_result.value_changed = wants_invalidate || wants_value_changed;
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

    void RenderError(const UiDrawContext& context) const
    {
        imgviewer::v2::RenderScriptError(context, L"Settings TypeScript UI failed", script_path_, error_text_);
    }

    void SetError(std::string text)
    {
        ready_ = false;
        error_text_ = text.empty() ? "Unknown JavaScript error" : std::move(text);
    }

    std::unique_ptr<UiElement> root_;
    imgviewer::v2::ScriptEngine& engine_;
    std::unique_ptr<imgviewer::v2::ScriptContext> script_context_;
    ImgViewerConfig draft_;
    const UiDrawContext* active_draw_context_ = nullptr;
    std::filesystem::path script_path_;
    D2D1_RECT_F rect_ = {};
    std::string error_text_;
    bool ready_ = false;
    bool close_requested_ = false;
    bool save_requested_ = false;
    bool reload_requested_ = false;
    bool invalidate_requested_ = false;
    bool value_changed_requested_ = false;
};

struct SettingsWindowContext final : public UiWindowDelegate {
    HWND owner = nullptr;
    ImgViewerContext* app = nullptr;
    UiWindowHost host;
    SettingsScriptUi* ui = nullptr;
    int original_opacity_percent = 100;
    int original_toolbar_scale_percent = 125;
    bool saved = false;

    SettingsWindowContext(int original_opacity, int original_toolbar_scale) :
        original_opacity_percent(ClampWindowOpacityPercent(original_opacity)),
        original_toolbar_scale_percent(ClampToolbarScalePercent(original_toolbar_scale))
    {
    }

    HRESULT OnCreate(UiWindowHost& window_host) override
    {
        if (app != nullptr) {
            app->settings_window = window_host.Hwnd();
        }
        return S_OK;
    }

    void OnDestroy(UiWindowHost&) override
    {
        if (app != nullptr) {
            if (!saved) {
                app->current_window_opacity_percent = original_opacity_percent;
                ApplyWindowOpacity(owner, original_opacity_percent);
                SetImgViewerToolbarScale(owner, app, original_toolbar_scale_percent);
            }
            app->settings_window = nullptr;
            PostMessageW(owner, kImgViewerOwnedWindowDestroyedMessage, 0, reinterpret_cast<LPARAM>(static_cast<UiWindowDelegate*>(this)));
        }
    }

    bool OnUiAction(UiWindowHost& window_host, UiAction action) override;
    void OnUiValueChanged(UiWindowHost&, UiEventResult) override;
    win32::WindowMessageResult OnUnhandledMessage(UiWindowHost& window_host, UINT message, WPARAM wparam, LPARAM lparam) override;
};

void CaptureCurrentWindowSize(HWND hwnd, ImgViewerConfig* config)
{
    if (config != nullptr && config->remember_window_size) {
        util::CaptureWindowSize(hwnd, &config->window_size.width, &config->window_size.height);
    }
}

void SaveSettings(HWND hwnd, SettingsWindowContext* context)
{
    if (context == nullptr) {
        DestroyWindow(hwnd);
        return;
    }

    if (context->app != nullptr && context->ui != nullptr) {
        ImgViewerConfig draft = context->ui->Draft();
        CaptureCurrentWindowSize(context->owner, &draft);
        const bool frame_changed = context->app->config.borderless_window != draft.borderless_window;
        const bool language_changed = context->app->config.language != draft.language;
        context->app->config = draft;
        SetImgViewerLanguage(context->app->config.language);
        context->app->current_window_opacity_percent = context->app->config.window_opacity_percent;
        ApplyWindowOpacity(context->owner, context->app->current_window_opacity_percent);
        SetImgViewerToolbarScale(context->owner, context->app, context->app->config.toolbar_scale_percent);
        context->app->viewer.SetPixelatedSampling(context->app->config.pixelated_sampling);
        context->app->renderer.SetCheckerboardBackground(context->app->config.checkerboard_background);
        SaveImgViewerConfig(context->app->config);
        if (language_changed) {
            ResetImgViewerUi(context->owner, context->app);
        }
        if (frame_changed) {
            ApplyImgViewerWindowFrame(context->owner, context->app, true);
        } else if (!language_changed) {
            InvalidateRect(context->owner, nullptr, FALSE);
        }
    }
    context->saved = true;
    context->host.Close();
}

bool SettingsWindowContext::OnUiAction(UiWindowHost& window_host, UiAction action)
{
    switch (ImgViewerActionFromUiAction(action)) {
    case ImgViewerAction::SaveSettings:
        SaveSettings(window_host.Hwnd(), this);
        return true;
    case ImgViewerAction::CloseSettings:
        window_host.Close();
        return true;
    case ImgViewerAction::ResetKeyBindings:
        if (ui != nullptr) {
            ImgViewerConfig draft = ui->Draft();
            draft.action_bindings = DefaultActionBindings();
            auto root = std::make_unique<SettingsScriptUi>(*app->script_engine, std::move(draft));
            ui = root.get();
            window_host.ResetRoot(std::move(root));
        }
        return true;
    default:
        break;
    }
    return false;
}

void SettingsWindowContext::OnUiValueChanged(UiWindowHost& window_host, UiEventResult)
{
    if (owner != nullptr && ui != nullptr) {
        if (app != nullptr) {
            app->current_window_opacity_percent = ui->OpacityPercent();
            SetImgViewerToolbarScale(owner, app, ui->ToolbarScalePercent());
        }
        ApplyWindowOpacity(owner, ui->OpacityPercent());
        window_host.Invalidate();
    }
}

win32::WindowMessageResult SettingsWindowContext::OnUnhandledMessage(UiWindowHost& window_host, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_GETMINMAXINFO:
        util::ApplyMinTrackSize(window_host.Hwnd(), lparam, kSettingsMinClientWidth, kSettingsMinClientHeight);
        return win32::WindowMessageResult::Handled();
    case kImgViewerSettingsOpacityChangedMessage:
        if (ui != nullptr) {
            ui->SetOpacityPercent(static_cast<int>(wparam));
            window_host.Invalidate();
        }
        return win32::WindowMessageResult::Handled();
    default:
        break;
    }
    return win32::WindowMessageResult::Unhandled();
}

} // namespace

HRESULT OpenImgViewerSettingsWindow(HWND owner, ImgViewerContext* context)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, owner);
    RETURN_HR_IF_NULL(E_INVALIDARG, context);
    if (context->settings_window != nullptr && IsWindow(context->settings_window)) {
        ShowWindow(context->settings_window, SW_SHOWNORMAL);
        SetForegroundWindow(context->settings_window);
        return S_OK;
    }

    HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner, GWLP_HINSTANCE));
    RETURN_HR_IF_NULL(E_UNEXPECTED, instance);

    ImgViewerConfig draft = context->config;
    draft.window_opacity_percent = context->current_window_opacity_percent;
    draft.toolbar_scale_percent = context->current_toolbar_scale_percent;
    auto* settings_context = new (std::nothrow) SettingsWindowContext(
        context->current_window_opacity_percent,
        context->current_toolbar_scale_percent);
    RETURN_IF_NULL_ALLOC(settings_context);
    settings_context->owner = owner;
    settings_context->app = context;
    context->settings_context = settings_context;

    auto root = std::make_unique<SettingsScriptUi>(*context->script_engine, std::move(draft));
    settings_context->ui = root.get();
    const HRESULT create_hr = settings_context->host.Create(
        UiWindowOptions{
            .native = win32::NativeWindowOptions{
                .instance = instance,
                .title = ImgViewerString(ImgViewerStringId::Settings),
                .frame = win32::NativeWindowFrame::Dialog,
                .width = kSettingsInitialWidth,
                .height = kSettingsInitialHeight,
                .owner = owner,
            },
            .action_message = kImgViewerUiActionMessage,
            .body_font_size = 9.5f,
            .icon_font_size = 11.0f,
        },
        std::move(root),
        settings_context,
        &context->graphics_device);
    if (FAILED(create_hr)) {
        context->settings_context = nullptr;
        delete settings_context;
        RETURN_IF_FAILED(create_hr);
    }

    settings_context->host.Window().Show(SW_SHOWNORMAL);
    context->interaction.SetModal(ImgViewerModalOwner::Settings);
    return S_OK;
}
