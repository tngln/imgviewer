#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include <windows.h>
#include <quickjs.h>

#include "imgviewer.script_ui.hpp"
#include "script.quickjs_runtime.hpp"
#include "script.timer.hpp"
#include "script.view.hpp"

namespace imgviewer {

class ScriptWindowRootBase : public ScriptUiHost, public ScriptView {
public:
    ScriptWindowRootBase(
        script::QuickJsRuntime& engine,
        const char* script_relative_path,
        const char* app_global_name,
        std::wstring error_title);
    ~ScriptWindowRootBase() override;

    const UiDrawContext* ActiveDrawContext() const override;
    void RequestInvalidate() override;
    void RequestReload() override;
    void RequestClose() override;
    uint32_t SetScriptTimer(JSContext* context, JSValueConst callback, uint32_t delay_ms, bool repeat) override;
    void ClearScriptTimer(uint32_t id) override;

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
    void SetError(std::string text);
    void SetActiveDrawContext(const UiDrawContext* context);
    void RenderError(const UiDrawContext& context) const;
    void SetScriptTimerHwnd(HWND hwnd);

    script::QuickJsRuntime& engine_;
    script::ScriptTimerManager timers_;
    std::unique_ptr<script::QuickJsContext> script_context_;
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
