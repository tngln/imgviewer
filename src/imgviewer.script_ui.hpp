#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <quickjs.h>

#include "ui.action.hpp"
#include "ui.draw.hpp"
#include "ui.events.hpp"

namespace imgviewer {

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
void SetString(JSContext* context, JSValue object, const char* name, std::wstring_view value);
void SetString(JSContext* context, JSValue object, const char* name, std::string_view value);
void SetString(JSContext* context, JSValue object, const char* name, const char* value);
void SetBool(JSContext* context, JSValue object, const char* name, bool value);
void SetInt(JSContext* context, JSValue object, const char* name, int32_t value);
void SetUint(JSContext* context, JSValue object, const char* name, uint32_t value);
void SetFloat(JSContext* context, JSValue object, const char* name, float value);
void SetFunction(JSContext* context, JSValue object, const char* name, JSCFunction* function, int length);
bool BoolProperty(JSContext* context, JSValueConst object, const char* name, bool fallback);
std::optional<bool> OptionalBoolProperty(JSContext* context, JSValueConst object, const char* name);
int32_t Int32Property(JSContext* context, JSValueConst object, const char* name, int32_t fallback);
float FloatProperty(JSContext* context, JSValueConst object, const char* name, float fallback);
UiAction ActionProperty(JSContext* context, JSValueConst object);
JSValue CreateSystemPreferencesObject(JSContext* context);
JSValue CreateHostObject(JSContext* context);
void InstallTypographyGlobals(JSContext* context, JSValue global);

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

} // namespace imgviewer
