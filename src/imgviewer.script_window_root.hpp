#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <quickjs.h>

#include "imgviewer.action.hpp"
#include "script.view.hpp"
#include "imgviewer.script_engine.hpp"
#include "imgviewer.script_ui.hpp"

namespace imgviewer {

class ScriptWindowRootBase : public ScriptUiHost, public ScriptView {
public:
    ScriptWindowRootBase(
        ScriptEngine& engine,
        const char* script_relative_path,
        const char* app_global_name,
        std::wstring error_title);
    ~ScriptWindowRootBase() override;

    const UiDrawContext* ActiveDrawContext() const override;
    void RequestInvalidate() override;
    void RequestReload() override;
    void RequestClose() override;

    void Render(const UiDrawContext& context) override;
    UiEventResult OnInputEvent(const UiInputEvent& event) override;
    UiEventResult OnPointerEvent(const UiPointerEvent& event) override;
    UiEventResult OnKeyEvent(const UiKeyEvent& event) override;

protected:
    void ReloadScript();
    virtual void BeforeReload() {}
    virtual void InstallCustomGlobals(JSValue global);
    virtual void OnScriptLoaded() {}
    virtual UiAction CloseAction() const;
    virtual UiEventResult FinishEventDispatch(JSValue result);

    JSContext* Context() const;
    JSValue AppObject() const;
    void SetFunction(JSValue object, const char* name, JSCFunction* function, int length);
    bool BoolProperty(JSValueConst object, const char* name, bool fallback) const;
    std::optional<bool> OptionalBoolProperty(JSValueConst object, const char* name) const;
    void SetError(std::string text);
    void SetActiveDrawContext(const UiDrawContext* context);
    void RenderError(const UiDrawContext& context) const;

    ScriptEngine& engine_;
    std::unique_ptr<ScriptContext> script_context_;
    std::filesystem::path script_path_;
    std::string error_text_;
    bool ready_ = false;
    bool invalidate_requested_ = false;
    bool reload_requested_ = false;
    bool close_requested_ = false;

private:
    void InstallGlobals();
    UiEventResult DispatchPointerToScript(const UiPointerEvent& event);
    UiEventResult DispatchKeyToScript(const UiKeyEvent& event);
    UiEventResult DispatchInputToScript(const UiInputEvent& event);

    const char* script_relative_path_ = nullptr;
    const char* app_global_name_ = nullptr;
    std::wstring error_title_;
    const UiDrawContext* active_draw_context_ = nullptr;
};

} // namespace imgviewer
