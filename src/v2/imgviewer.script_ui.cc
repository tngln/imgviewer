#include "v2/imgviewer.script_ui.hpp"

#include <fstream>
#include <vector>

#include <d2d1helper.h>
#include <wil/com.h>

#include "script.canvas_color.hpp"
#include "ui.theme.hpp"

namespace imgviewer::v2 {

std::wstring WideFromUtf8(std::string_view text)
{
    if (text.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring value(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), value.data(), length);
    return value;
}

std::string Utf8FromWide(std::wstring_view text)
{
    if (text.empty()) {
        return {};
    }
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        return {};
    }
    std::string value(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), value.data(), length, nullptr, nullptr);
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
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::filesystem::path ExecutableDirectory()
{
    std::vector<wchar_t> buffer(MAX_PATH);
    while (true) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return std::filesystem::current_path();
        }
        if (length < buffer.size() - 1) {
            return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::filesystem::path ScriptPath(const char* relative_path)
{
    return ExecutableDirectory() / relative_path;
}

JSValue HostInvalidate(JSContext* context, JSValueConst, int, JSValueConst*)
{
    if (auto* host = static_cast<ScriptUiHost*>(JS_GetContextOpaque(context))) {
        host->RequestInvalidate();
    }
    return JS_UNDEFINED;
}

JSValue HostReload(JSContext* context, JSValueConst, int, JSValueConst*)
{
    if (auto* host = static_cast<ScriptUiHost*>(JS_GetContextOpaque(context))) {
        host->RequestReload();
    }
    return JS_UNDEFINED;
}

JSValue HostClose(JSContext* context, JSValueConst, int, JSValueConst*)
{
    if (auto* host = static_cast<ScriptUiHost*>(JS_GetContextOpaque(context))) {
        host->RequestClose();
    }
    return JS_UNDEFINED;
}

JSValue HostLog(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
{
    if (argc > 0) {
        OutputDebugStringW(WideFromUtf8(Utf8FromValue(context, argv[0]) + "\n").c_str());
    }
    return JS_UNDEFINED;
}

namespace {

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

void AddModifiers(JSContext* context, JSValue object, UiModifiers modifiers)
{
    JS_SetPropertyStr(context, object, "ctrl", JS_NewBool(context, modifiers.ctrl));
    JS_SetPropertyStr(context, object, "shift", JS_NewBool(context, modifiers.shift));
    JS_SetPropertyStr(context, object, "alt", JS_NewBool(context, modifiers.alt));
}

const UiDrawContext* ActiveDrawContext(JSContext* context)
{
    auto* host = static_cast<ScriptUiHost*>(JS_GetContextOpaque(context));
    return host != nullptr ? host->ActiveDrawContext() : nullptr;
}

JSValue CanvasClear(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
{
    const UiDrawContext* draw_context = ActiveDrawContext(context);
    if (draw_context == nullptr || argc < 1) {
        return JS_UNDEFINED;
    }
    const std::optional<D2D1_COLOR_F> color = script::ParseCanvasColor(Utf8FromValue(context, argv[0]));
    if (color.has_value()) {
        UiDraw(*draw_context).Clear(*color);
    }
    return JS_UNDEFINED;
}

JSValue CanvasFillRect(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
{
    const UiDrawContext* draw_context = ActiveDrawContext(context);
    if (draw_context == nullptr || argc < 5) {
        return JS_UNDEFINED;
    }
    double x = 0.0, y = 0.0, width = 0.0, height = 0.0;
    JS_ToFloat64(context, &x, argv[0]);
    JS_ToFloat64(context, &y, argv[1]);
    JS_ToFloat64(context, &width, argv[2]);
    JS_ToFloat64(context, &height, argv[3]);
    const std::optional<D2D1_COLOR_F> color = script::ParseCanvasColor(Utf8FromValue(context, argv[4]));
    if (color.has_value()) {
        UiDraw(*draw_context).FillRect(
            D2D1::RectF(static_cast<float>(x), static_cast<float>(y), static_cast<float>(x + width), static_cast<float>(y + height)),
            *color);
    }
    return JS_UNDEFINED;
}

JSValue CanvasStrokeRect(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
{
    const UiDrawContext* draw_context = ActiveDrawContext(context);
    if (draw_context == nullptr || argc < 5) {
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
        UiDraw(*draw_context).DrawRect(
            D2D1::RectF(static_cast<float>(x), static_cast<float>(y), static_cast<float>(x + width), static_cast<float>(y + height)),
            *color,
            static_cast<float>(stroke_width));
    }
    return JS_UNDEFINED;
}

JSValue CanvasFillText(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
{
    const UiDrawContext* draw_context = ActiveDrawContext(context);
    if (draw_context == nullptr || argc < 6) {
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
        UiDraw(*draw_context).DrawBodyText(
            text,
            D2D1::RectF(static_cast<float>(x), static_cast<float>(y), static_cast<float>(x + width), static_cast<float>(y + height)),
            *color,
            D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
    return JS_UNDEFINED;
}

JSValue CanvasStrokeLine(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
{
    const UiDrawContext* draw_context = ActiveDrawContext(context);
    if (draw_context == nullptr || argc < 5) {
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
    if (color.has_value() && draw_context->d2d_context != nullptr) {
        wil::com_ptr<ID2D1SolidColorBrush> brush;
        if (SUCCEEDED(draw_context->d2d_context->CreateSolidColorBrush(*color, brush.put()))) {
            draw_context->d2d_context->DrawLine(
                D2D1::Point2F(static_cast<float>(x1), static_cast<float>(y1)),
                D2D1::Point2F(static_cast<float>(x2), static_cast<float>(y2)),
                brush.get(),
                static_cast<float>(stroke_width));
        }
    }
    return JS_UNDEFINED;
}

} // namespace

JSValue CreateCanvasObject(JSContext* context)
{
    JSValue canvas = JS_NewObject(context);
    JS_SetPropertyStr(context, canvas, "clear", JS_NewCFunction(context, CanvasClear, "clear", 1));
    JS_SetPropertyStr(context, canvas, "fillRect", JS_NewCFunction(context, CanvasFillRect, "fillRect", 5));
    JS_SetPropertyStr(context, canvas, "strokeRect", JS_NewCFunction(context, CanvasStrokeRect, "strokeRect", 6));
    JS_SetPropertyStr(context, canvas, "fillText", JS_NewCFunction(context, CanvasFillText, "fillText", 6));
    JS_SetPropertyStr(context, canvas, "strokeLine", JS_NewCFunction(context, CanvasStrokeLine, "strokeLine", 6));
    return canvas;
}

JSValue CreateRenderEnvironment(JSContext* context, const UiDrawContext& draw_context, UiRootState state)
{
    JSValue env = JS_NewObject(context);
    JS_SetPropertyStr(context, env, "width", JS_NewFloat64(context, draw_context.viewport_size.width));
    JS_SetPropertyStr(context, env, "height", JS_NewFloat64(context, draw_context.viewport_size.height));
    JS_SetPropertyStr(context, env, "dpiScale", JS_NewFloat64(context, draw_context.dpi_scale));
    JS_SetPropertyStr(context, env, "hovered", JS_NewBool(context, state.hovered != UiElementId::None));
    JS_SetPropertyStr(context, env, "pressed", JS_NewBool(context, state.pressed != UiElementId::None));
    JS_SetPropertyStr(context, env, "focused", JS_NewBool(context, state.focused != UiElementId::None));
    return env;
}

JSValue CreatePointerEvent(JSContext* context, const UiPointerEvent& event)
{
    JSValue value = JS_NewObject(context);
    JS_SetPropertyStr(context, value, "type", JS_NewString(context, PointerTypeName(event.type)));
    JS_SetPropertyStr(context, value, "x", JS_NewFloat64(context, event.point.x));
    JS_SetPropertyStr(context, value, "y", JS_NewFloat64(context, event.point.y));
    JS_SetPropertyStr(context, value, "button", JS_NewString(context, PointerButtonName(event.button)));
    JS_SetPropertyStr(context, value, "wheelDelta", JS_NewInt32(context, event.wheel_delta));
    AddModifiers(context, value, event.modifiers);
    return value;
}

JSValue CreateKeyEvent(JSContext* context, const UiKeyEvent& event)
{
    JSValue value = JS_NewObject(context);
    JS_SetPropertyStr(context, value, "type", JS_NewString(context, KeyTypeName(event.type)));
    JS_SetPropertyStr(context, value, "virtualKey", JS_NewInt32(context, static_cast<int32_t>(event.virtual_key)));
    JS_SetPropertyStr(context, value, "repeat", JS_NewBool(context, event.repeat));
    AddModifiers(context, value, event.modifiers);
    return value;
}

JSValue CreateTextEvent(JSContext* context, wchar_t ch)
{
    JSValue value = JS_NewObject(context);
    const std::wstring text(1, ch);
    JS_SetPropertyStr(context, value, "text", JS_NewString(context, Utf8FromWide(text).c_str()));
    return value;
}

void RenderScriptError(
    const UiDrawContext& context,
    std::wstring_view title,
    const std::filesystem::path& script_path,
    std::string_view error_text)
{
    const UiDraw draw(context);
    draw.Clear(ui_theme::color::kWindowBackground);
    draw.DrawBodyText(
        std::wstring(title),
        D2D1::RectF(24.0f, 24.0f, context.viewport_size.width - 24.0f, 52.0f),
        ui_theme::color::kBodyText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP);
    const std::wstring script = L"Script: " + script_path.wstring();
    draw.DrawBodyText(
        script,
        D2D1::RectF(24.0f, 60.0f, context.viewport_size.width - 24.0f, 84.0f),
        ui_theme::color::kMutedText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP);
    draw.DrawBodyText(
        WideFromUtf8(error_text),
        D2D1::RectF(24.0f, 96.0f, context.viewport_size.width - 24.0f, context.viewport_size.height - 56.0f),
        ui_theme::color::kBodyText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP);
    draw.DrawBodyText(
        L"Press F5 to reload. Press Esc to close.",
        D2D1::RectF(24.0f, context.viewport_size.height - 44.0f, context.viewport_size.width - 24.0f, context.viewport_size.height - 16.0f),
        ui_theme::color::kMutedText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

} // namespace imgviewer::v2
