#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <quickjs.h>

#include "ui.draw.hpp"
#include "ui.events.hpp"

namespace imgviewer::v2 {

struct ScriptVectorIcon final {
    std::string id;
    D2D1_RECT_F view_box = {};
    std::vector<icons::PathCommand> commands;
};

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
JSValue CreateSystemPreferencesObject(JSContext* context);
JSValue CreateHostObject(JSContext* context);

JSValue CreateCanvasObject(JSContext* context);
JSValue CreateRenderEnvironment(JSContext* context, const UiDrawContext& draw_context);
JSValue CreatePointerEvent(JSContext* context, const UiPointerEvent& event);
JSValue CreateKeyEvent(JSContext* context, const UiKeyEvent& event);
JSValue CreateTextEvent(JSContext* context, wchar_t ch);
JSValue CreateInputEvent(JSContext* context, const UiInputEvent& event);
bool ReadVectorIcon(JSContext* context, JSValueConst value, ScriptVectorIcon* icon);
std::optional<D2D1_POINT_2F> ImeCaretPointProperty(JSContext* context, JSValueConst object);

void RenderScriptError(
    const UiDrawContext& context,
    std::wstring_view title,
    const std::filesystem::path& script_path,
    std::string_view error_text);

} // namespace imgviewer::v2
