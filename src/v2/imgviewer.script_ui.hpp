#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <quickjs.h>

#include "ui.draw.hpp"
#include "ui.events.hpp"

namespace imgviewer::v2 {

class ScriptUiHost {
public:
    virtual ~ScriptUiHost() = default;
    virtual const UiDrawContext* ActiveDrawContext() const = 0;
    virtual void RequestInvalidate() = 0;
    virtual void RequestReload() = 0;
    virtual void RequestClose() = 0;
};

std::wstring WideFromUtf8(std::string_view text);
std::string Utf8FromWide(std::wstring_view text);
std::string Utf8FromValue(JSContext* context, JSValueConst value);
std::optional<std::string> ReadTextFileUtf8(const std::filesystem::path& path);
std::filesystem::path ExecutableDirectory();
std::filesystem::path ScriptPath(const char* relative_path);

JSValue HostInvalidate(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv);
JSValue HostReload(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv);
JSValue HostClose(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv);
JSValue HostLog(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv);

JSValue CreateCanvasObject(JSContext* context);
JSValue CreateRenderEnvironment(JSContext* context, const UiDrawContext& draw_context, UiRootState state);
JSValue CreatePointerEvent(JSContext* context, const UiPointerEvent& event);
JSValue CreateKeyEvent(JSContext* context, const UiKeyEvent& event);
JSValue CreateTextEvent(JSContext* context, wchar_t ch);

void RenderScriptError(
    const UiDrawContext& context,
    std::wstring_view title,
    const std::filesystem::path& script_path,
    std::string_view error_text);

} // namespace imgviewer::v2
