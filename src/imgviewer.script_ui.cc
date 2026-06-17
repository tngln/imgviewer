#include "imgviewer.script_ui.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>
#include <vector>

#include <d2d1helper.h>
#include <dwmapi.h>
#include <wil/com.h>

#include "script.canvas_color.hpp"

namespace imgviewer {

namespace {

constexpr DWORD kDefaultAccentColor = 0xff2f6fed;
constexpr D2D1_COLOR_F kScriptErrorBackground = {0xf7 / 255.0f, 0xf9 / 255.0f, 0xfc / 255.0f, 1.0f};
constexpr D2D1_COLOR_F kScriptErrorText = {0x17 / 255.0f, 0x20 / 255.0f, 0x33 / 255.0f, 1.0f};
constexpr D2D1_COLOR_F kScriptErrorMutedText = {0x69 / 255.0f, 0x73 / 255.0f, 0x86 / 255.0f, 1.0f};
constexpr wchar_t kPersonalizeRegistryPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
constexpr wchar_t kAppsUseLightThemeRegistryName[] = L"AppsUseLightTheme";

void SetString(JSContext* context, JSValue object, const char* name, const std::string& value)
{
    JS_SetPropertyStr(context, object, name, JS_NewString(context, value.c_str()));
}

void SetBool(JSContext* context, JSValue object, const char* name, bool value)
{
    JS_SetPropertyStr(context, object, name, JS_NewBool(context, value));
}

void SetInt(JSContext* context, JSValue object, const char* name, int32_t value)
{
    JS_SetPropertyStr(context, object, name, JS_NewInt32(context, value));
}

void SetUint(JSContext* context, JSValue object, const char* name, uint32_t value)
{
    JS_SetPropertyStr(context, object, name, JS_NewUint32(context, value));
}

std::string ArgbColorString(DWORD color)
{
    std::ostringstream output;
    output << '#' << std::uppercase << std::hex << std::setfill('0') << std::setw(8) << color;
    return output.str();
}

std::string UserDefaultLocaleName()
{
    wchar_t locale_name[LOCALE_NAME_MAX_LENGTH] = {};
    if (GetUserDefaultLocaleName(locale_name, LOCALE_NAME_MAX_LENGTH) <= 0) {
        return "en-US";
    }
    return Utf8FromWide(locale_name);
}

std::vector<std::string> UserPreferredLanguages()
{
    ULONG language_count = 0;
    ULONG buffer_length = 0;
    if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &language_count, nullptr, &buffer_length) || buffer_length == 0) {
        return {};
    }

    std::vector<wchar_t> buffer(buffer_length);
    if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &language_count, buffer.data(), &buffer_length)) {
        return {};
    }

    std::vector<std::string> languages;
    const wchar_t* current = buffer.data();
    while (*current != L'\0') {
        languages.push_back(Utf8FromWide(current));
        current += wcslen(current) + 1;
    }
    return languages;
}

bool PrefersDarkTheme()
{
    DWORD apps_use_light_theme = 1;
    DWORD size = sizeof(apps_use_light_theme);
    const LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        kPersonalizeRegistryPath,
        kAppsUseLightThemeRegistryName,
        RRF_RT_REG_DWORD,
        nullptr,
        &apps_use_light_theme,
        &size);
    return status == ERROR_SUCCESS && apps_use_light_theme == 0;
}

bool HighContrastEnabled()
{
    HIGHCONTRASTW high_contrast{.cbSize = sizeof(high_contrast)};
    if (!SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(high_contrast), &high_contrast, 0)) {
        return false;
    }
    return (high_contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

bool ClientAreaAnimationEnabled()
{
    BOOL enabled = TRUE;
    if (!SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0)) {
        return true;
    }
    return enabled != FALSE;
}

JSValue StringArray(JSContext* context, const std::vector<std::string>& values)
{
    JSValue array = JS_NewArray(context);
    uint32_t index = 0;
    for (const std::string& value : values) {
        JS_SetPropertyUint32(context, array, index++, JS_NewString(context, value.c_str()));
    }
    return array;
}

JSValue WideStringArray(JSContext* context, const std::vector<std::wstring>& values)
{
    JSValue array = JS_NewArray(context);
    uint32_t index = 0;
    for (const std::wstring& value : values) {
        JS_SetPropertyUint32(context, array, index++, JS_NewString(context, Utf8FromWide(value).c_str()));
    }
    return array;
}

} // namespace

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

JSValue CreateSystemPreferencesObject(JSContext* context)
{
    DWORD accent_color = kDefaultAccentColor;
    BOOL accent_opaque_blend = FALSE;
    if (FAILED(DwmGetColorizationColor(&accent_color, &accent_opaque_blend))) {
        accent_color = kDefaultAccentColor;
        accent_opaque_blend = FALSE;
    }

    std::vector<std::string> languages = UserPreferredLanguages();
    if (languages.empty()) {
        languages.push_back(UserDefaultLocaleName());
    }
    if (languages.front().empty()) {
        languages.front() = "en-US";
    }

    JSValue preferences = JS_NewObject(context);
    SetUint(context, preferences, "caretBlinkTime", GetCaretBlinkTime());
    SetUint(context, preferences, "doubleClickTime", GetDoubleClickTime());
    SetString(context, preferences, "accentColor", ArgbColorString(accent_color));
    SetBool(context, preferences, "accentColorOpaqueBlend", accent_opaque_blend != FALSE);
    SetBool(context, preferences, "prefersDarkTheme", PrefersDarkTheme());
    SetString(context, preferences, "preferredLanguage", languages.front());
    JS_SetPropertyStr(context, preferences, "preferredLanguages", StringArray(context, languages));
    SetBool(context, preferences, "highContrast", HighContrastEnabled());
    SetBool(context, preferences, "clientAreaAnimationEnabled", ClientAreaAnimationEnabled());
    return preferences;
}

JSValue CreateHostObject(JSContext* context)
{
    JSValue host = JS_NewObject(context);
    JS_SetPropertyStr(context, host, "invalidate", JS_NewCFunction(context, HostInvalidate, "invalidate", 0));
    JS_SetPropertyStr(context, host, "reload", JS_NewCFunction(context, HostReload, "reload", 0));
    JS_SetPropertyStr(context, host, "close", JS_NewCFunction(context, HostClose, "close", 0));
    JS_SetPropertyStr(context, host, "log", JS_NewCFunction(context, HostLog, "log", 1));
    JS_SetPropertyStr(context, host, "systemPreferences", CreateSystemPreferencesObject(context));
    return host;
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

const char* InputKindName(UiEventType type)
{
    switch (type) {
    case UiEventType::PointerMove:
    case UiEventType::PointerDown:
    case UiEventType::PointerUp:
    case UiEventType::PointerLeave:
    case UiEventType::PointerWheel:
        return "pointer";
    case UiEventType::KeyDown:
    case UiEventType::KeyUp:
        return "key";
    case UiEventType::TextChar:
        return "text";
    case UiEventType::ImeStartComposition:
        return "imeStart";
    case UiEventType::ImeComposition:
        return "imeComposition";
    case UiEventType::ImeEndComposition:
        return "imeEnd";
    case UiEventType::ContextMenu:
        return "contextMenu";
    case UiEventType::Timer:
        return "timer";
    case UiEventType::FilesDropped:
        return "filesDropped";
    case UiEventType::WindowMoved:
        return "windowMoved";
    case UiEventType::WindowResized:
        return "windowResized";
    case UiEventType::DpiChanged:
        return "dpiChanged";
    case UiEventType::WindowClose:
        return "windowClose";
    case UiEventType::WindowDestroyed:
        return "windowDestroyed";
    case UiEventType::Cancel:
        return "cancel";
    case UiEventType::OwnerDeactivated:
        return "ownerDeactivated";
    case UiEventType::FocusGained:
        return "focusGained";
    case UiEventType::FocusLost:
        return "focusLost";
    default:
        return "unknown";
    }
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

class D2DTransformGuard final {
public:
    D2DTransformGuard(ID2D1DeviceContext* target, D2D1_MATRIX_3X2_F transform) : target_(target)
    {
        if (target_ == nullptr) {
            return;
        }
        target_->GetTransform(&old_transform_);
        target_->SetTransform(transform * old_transform_);
    }

    D2DTransformGuard(const D2DTransformGuard&) = delete;
    D2DTransformGuard& operator=(const D2DTransformGuard&) = delete;

    ~D2DTransformGuard()
    {
        if (target_ != nullptr) {
            target_->SetTransform(old_transform_);
        }
    }

private:
    ID2D1DeviceContext* target_ = nullptr;
    D2D1_MATRIX_3X2_F old_transform_ = D2D1::Matrix3x2F::Identity();
};

bool ReadArrayLength(JSContext* context, JSValueConst value, uint32_t* length)
{
    JSValue length_value = JS_GetPropertyStr(context, value, "length");
    const bool ok = JS_ToUint32(context, length, length_value) == 0;
    JS_FreeValue(context, length_value);
    return ok;
}

bool ReadArrayNumber(JSContext* context, JSValueConst value, uint32_t index, float* number)
{
    JSValue item = JS_GetPropertyUint32(context, value, index);
    double parsed = 0.0;
    const bool ok = JS_ToFloat64(context, &parsed, item) == 0;
    JS_FreeValue(context, item);
    if (!ok) {
        return false;
    }
    *number = static_cast<float>(parsed);
    return true;
}

bool ReadPoint(JSContext* context, JSValueConst value, uint32_t index, D2D1_POINT_2F* point)
{
    return ReadArrayNumber(context, value, index, &point->x) &&
        ReadArrayNumber(context, value, index + 1, &point->y);
}

bool ReadVectorIconCommand(JSContext* context, JSValueConst value, icons::PathCommand* command)
{
    uint32_t length = 0;
    if (JS_IsArray(value) != 1 || !ReadArrayLength(context, value, &length) || length == 0) {
        return false;
    }

    JSValue verb_value = JS_GetPropertyUint32(context, value, 0);
    const std::string verb = Utf8FromValue(context, verb_value);
    JS_FreeValue(context, verb_value);

    *command = {};
    if (verb == "M" && length >= 3) {
        command->verb = icons::PathVerb::MoveTo;
        return ReadPoint(context, value, 1, &command->points[0]);
    }
    if (verb == "L" && length >= 3) {
        command->verb = icons::PathVerb::LineTo;
        return ReadPoint(context, value, 1, &command->points[0]);
    }
    if (verb == "C" && length >= 7) {
        command->verb = icons::PathVerb::CubicTo;
        return ReadPoint(context, value, 1, &command->points[0]) &&
            ReadPoint(context, value, 3, &command->points[1]) &&
            ReadPoint(context, value, 5, &command->points[2]);
    }
    if (verb == "Z") {
        command->verb = icons::PathVerb::Close;
        return true;
    }
    return false;
}

uint64_t HashFloat(uint64_t hash, float value)
{
    const auto* bytes = reinterpret_cast<const unsigned char*>(&value);
    for (size_t index = 0; index < sizeof(value); ++index) {
        hash ^= bytes[index];
        hash *= 1099511628211ull;
    }
    return hash;
}

uint64_t HashIcon(const ScriptVectorIcon& icon)
{
    uint64_t hash = 1469598103934665603ull;
    hash = HashFloat(hash, icon.view_box.left);
    hash = HashFloat(hash, icon.view_box.top);
    hash = HashFloat(hash, icon.view_box.right);
    hash = HashFloat(hash, icon.view_box.bottom);
    for (const icons::PathCommand& command : icon.commands) {
        hash ^= static_cast<uint64_t>(command.verb);
        hash *= 1099511628211ull;
        for (const D2D1_POINT_2F& point : command.points) {
            hash = HashFloat(hash, point.x);
            hash = HashFloat(hash, point.y);
        }
    }
    return hash;
}

std::string IconCacheKey(const ScriptVectorIcon& icon)
{
    std::ostringstream stream;
    if (!icon.id.empty()) {
        stream << "id:" << icon.id << ":";
    } else {
        stream << "hash:";
    }
    stream << std::hex << HashIcon(icon);
    return stream.str();
}

struct CachedIconGeometry final {
    std::string key;
    wil::com_ptr<ID2D1PathGeometry> geometry;
};

ID2D1PathGeometry* CachedGeometryForIcon(ID2D1Factory1* factory, const ScriptVectorIcon& icon)
{
    constexpr size_t kMaxCachedIcons = 64;
    static ID2D1Factory1* cached_factory = nullptr;
    static std::vector<CachedIconGeometry> cache;

    if (factory == nullptr || icon.commands.empty()) {
        return nullptr;
    }
    if (factory != cached_factory) {
        cache.clear();
        cached_factory = factory;
    }

    const std::string key = IconCacheKey(icon);
    const auto found = std::find_if(cache.begin(), cache.end(), [&](const CachedIconGeometry& item) {
        return item.key == key;
    });
    if (found != cache.end()) {
        return found->geometry.get();
    }

    CachedIconGeometry item;
    item.key = key;
    if (FAILED(CreatePathGeometryFromIcon(factory, icon.commands.data(), icon.commands.size(), item.geometry.put()))) {
        return nullptr;
    }
    if (cache.size() >= kMaxCachedIcons) {
        cache.erase(cache.begin());
    }
    cache.push_back(std::move(item));
    return cache.back().geometry.get();
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

JSValue CanvasDrawIcon(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
{
    const UiDrawContext* draw_context = ActiveDrawContext(context);
    if (draw_context == nullptr || draw_context->d2d_context == nullptr || draw_context->d2d_factory == nullptr ||
        argc < 6) {
        return JS_UNDEFINED;
    }

    ScriptVectorIcon icon;
    if (!ReadVectorIcon(context, argv[0], &icon)) {
        return JS_UNDEFINED;
    }

    double x = 0.0, y = 0.0, width = 0.0, height = 0.0, stroke_width = 1.0;
    JS_ToFloat64(context, &x, argv[1]);
    JS_ToFloat64(context, &y, argv[2]);
    JS_ToFloat64(context, &width, argv[3]);
    JS_ToFloat64(context, &height, argv[4]);
    if (argc > 6) {
        JS_ToFloat64(context, &stroke_width, argv[6]);
    }

    const std::optional<D2D1_COLOR_F> color = script::ParseCanvasColor(Utf8FromValue(context, argv[5]));
    const float box_width = static_cast<float>(width);
    const float box_height = static_cast<float>(height);
    const float icon_width = icon.view_box.right - icon.view_box.left;
    const float icon_height = icon.view_box.bottom - icon.view_box.top;
    if (!color.has_value() || box_width <= 0.0f || box_height <= 0.0f || icon_width <= 0.0f || icon_height <= 0.0f) {
        return JS_UNDEFINED;
    }

    ID2D1PathGeometry* geometry = CachedGeometryForIcon(draw_context->d2d_factory, icon);
    if (geometry == nullptr) {
        return JS_UNDEFINED;
    }

    const float scale = (std::min)(box_width / icon_width, box_height / icon_height);
    if (scale <= 0.0f) {
        return JS_UNDEFINED;
    }
    const float left = static_cast<float>(x) + (box_width - icon_width * scale) * 0.5f;
    const float top = static_cast<float>(y) + (box_height - icon_height * scale) * 0.5f;
    const D2DTransformGuard transform_guard(
        draw_context->d2d_context,
        D2D1::Matrix3x2F::Translation(-icon.view_box.left, -icon.view_box.top) *
        D2D1::Matrix3x2F::Scale(scale, scale) *
        D2D1::Matrix3x2F::Translation(left, top));
    UiDraw(*draw_context).DrawGeometry(geometry, *color, static_cast<float>(stroke_width) / scale);
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
    JS_SetPropertyStr(context, canvas, "drawIcon", JS_NewCFunction(context, CanvasDrawIcon, "drawIcon", 7));
    return canvas;
}

JSValue CreateRenderEnvironment(JSContext* context, const UiDrawContext& draw_context)
{
    JSValue env = JS_NewObject(context);
    JS_SetPropertyStr(context, env, "width", JS_NewFloat64(context, draw_context.viewport_size.width));
    JS_SetPropertyStr(context, env, "height", JS_NewFloat64(context, draw_context.viewport_size.height));
    JS_SetPropertyStr(context, env, "dpiScale", JS_NewFloat64(context, draw_context.dpi_scale));
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
    JS_SetPropertyStr(context, value, "system", JS_NewBool(context, event.system));
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

JSValue CreateInputEvent(JSContext* context, const UiInputEvent& event)
{
    JSValue value = JS_NewObject(context);
    SetString(context, value, "kind", InputKindName(event.type));
    switch (event.type) {
    case UiEventType::PointerMove:
    case UiEventType::PointerDown:
    case UiEventType::PointerUp:
    case UiEventType::PointerLeave:
    case UiEventType::PointerWheel:
        JS_SetPropertyStr(context, value, "type", JS_NewString(context, PointerTypeName(event.type)));
        JS_SetPropertyStr(context, value, "x", JS_NewFloat64(context, event.pointer.point.x));
        JS_SetPropertyStr(context, value, "y", JS_NewFloat64(context, event.pointer.point.y));
        JS_SetPropertyStr(context, value, "button", JS_NewString(context, PointerButtonName(event.pointer.button)));
        JS_SetPropertyStr(context, value, "wheelDelta", JS_NewInt32(context, event.pointer.wheel_delta));
        AddModifiers(context, value, event.pointer.modifiers);
        break;
    case UiEventType::KeyDown:
    case UiEventType::KeyUp:
        JS_SetPropertyStr(context, value, "type", JS_NewString(context, KeyTypeName(event.type)));
        JS_SetPropertyStr(context, value, "virtualKey", JS_NewInt32(context, static_cast<int32_t>(event.key.virtual_key)));
        JS_SetPropertyStr(context, value, "repeat", JS_NewBool(context, event.key.repeat));
        JS_SetPropertyStr(context, value, "system", JS_NewBool(context, event.key.system));
        AddModifiers(context, value, event.key.modifiers);
        break;
    case UiEventType::TextChar: {
        const std::wstring text(1, event.character);
        JS_SetPropertyStr(context, value, "text", JS_NewString(context, Utf8FromWide(text).c_str()));
        break;
    }
    case UiEventType::ImeStartComposition:
        break;
    case UiEventType::ImeComposition:
        JS_SetPropertyStr(context, value, "text", JS_NewString(context, Utf8FromWide(event.text).c_str()));
        break;
    case UiEventType::ImeEndComposition:
        break;
    case UiEventType::ContextMenu:
        JS_SetPropertyStr(context, value, "x", JS_NewFloat64(context, event.point.x));
        JS_SetPropertyStr(context, value, "y", JS_NewFloat64(context, event.point.y));
        JS_SetPropertyStr(context, value, "screenX", JS_NewInt32(context, event.screen_point.x));
        JS_SetPropertyStr(context, value, "screenY", JS_NewInt32(context, event.screen_point.y));
        break;
    case UiEventType::Timer:
        JS_SetPropertyStr(context, value, "timerId", JS_NewUint32(context, static_cast<uint32_t>(event.timer_id)));
        break;
    case UiEventType::FilesDropped:
        JS_SetPropertyStr(context, value, "files", WideStringArray(context, event.file_paths));
        break;
    case UiEventType::WindowResized:
    case UiEventType::DpiChanged:
        JS_SetPropertyStr(context, value, "pixelWidth", JS_NewUint32(context, event.pixel_size.width));
        JS_SetPropertyStr(context, value, "pixelHeight", JS_NewUint32(context, event.pixel_size.height));
        JS_SetPropertyStr(context, value, "width", JS_NewFloat64(context, event.ui_size.width));
        JS_SetPropertyStr(context, value, "height", JS_NewFloat64(context, event.ui_size.height));
        if (event.dpi != 0) {
            JS_SetPropertyStr(context, value, "dpi", JS_NewUint32(context, event.dpi));
        }
        break;
    default:
        break;
    }
    return value;
}

bool ReadVectorIcon(JSContext* context, JSValueConst value, ScriptVectorIcon* icon)
{
    if (context == nullptr || icon == nullptr || !JS_IsObject(value)) {
        return false;
    }

    ScriptVectorIcon parsed;

    JSValue id_value = JS_GetPropertyStr(context, value, "id");
    if (!JS_IsUndefined(id_value) && !JS_IsNull(id_value)) {
        parsed.id = Utf8FromValue(context, id_value);
    }
    JS_FreeValue(context, id_value);

    JSValue view_box = JS_GetPropertyStr(context, value, "viewBox");
    uint32_t view_box_length = 0;
    float view_x = 0.0f;
    float view_y = 0.0f;
    float view_width = 0.0f;
    float view_height = 0.0f;
    const bool valid_view_box =
        JS_IsArray(view_box) == 1 &&
        ReadArrayLength(context, view_box, &view_box_length) &&
        view_box_length == 4 &&
        ReadArrayNumber(context, view_box, 0, &view_x) &&
        ReadArrayNumber(context, view_box, 1, &view_y) &&
        ReadArrayNumber(context, view_box, 2, &view_width) &&
        ReadArrayNumber(context, view_box, 3, &view_height) &&
        view_width > 0.0f &&
        view_height > 0.0f;
    JS_FreeValue(context, view_box);
    if (!valid_view_box) {
        return false;
    }
    parsed.view_box = D2D1::RectF(view_x, view_y, view_x + view_width, view_y + view_height);

    JSValue commands = JS_GetPropertyStr(context, value, "commands");
    uint32_t command_count = 0;
    if (JS_IsArray(commands) != 1 || !ReadArrayLength(context, commands, &command_count) || command_count == 0) {
        JS_FreeValue(context, commands);
        return false;
    }

    parsed.commands.reserve(command_count);
    for (uint32_t index = 0; index < command_count; ++index) {
        JSValue command_value = JS_GetPropertyUint32(context, commands, index);
        icons::PathCommand command = {};
        const bool ok = ReadVectorIconCommand(context, command_value, &command);
        JS_FreeValue(context, command_value);
        if (!ok) {
            JS_FreeValue(context, commands);
            return false;
        }
        parsed.commands.push_back(command);
    }
    JS_FreeValue(context, commands);

    *icon = std::move(parsed);
    return true;
}

std::optional<D2D1_POINT_2F> ImeCaretPointProperty(JSContext* context, JSValueConst object)
{
    if (!JS_IsObject(object)) {
        return std::nullopt;
    }
    JSValue caret = JS_GetPropertyStr(context, object, "imeCaret");
    if (!JS_IsObject(caret)) {
        JS_FreeValue(context, caret);
        return std::nullopt;
    }

    JSValue x_value = JS_GetPropertyStr(context, caret, "x");
    JSValue y_value = JS_GetPropertyStr(context, caret, "y");
    double x = 0.0;
    double y = 0.0;
    const bool ok = JS_ToFloat64(context, &x, x_value) == 0 && JS_ToFloat64(context, &y, y_value) == 0;
    JS_FreeValue(context, y_value);
    JS_FreeValue(context, x_value);
    JS_FreeValue(context, caret);
    if (!ok) {
        return std::nullopt;
    }
    return D2D1::Point2F(static_cast<float>(x), static_cast<float>(y));
}

void RenderScriptError(
    const UiDrawContext& context,
    std::wstring_view title,
    const std::filesystem::path& script_path,
    std::string_view error_text)
{
    const UiDraw draw(context);
    draw.Clear(kScriptErrorBackground);
    draw.DrawBodyText(
        std::wstring(title),
        D2D1::RectF(24.0f, 24.0f, context.viewport_size.width - 24.0f, 52.0f),
        kScriptErrorText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP);
    const std::wstring script = L"Script: " + script_path.wstring();
    draw.DrawBodyText(
        script,
        D2D1::RectF(24.0f, 60.0f, context.viewport_size.width - 24.0f, 84.0f),
        kScriptErrorMutedText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP);
    draw.DrawBodyText(
        WideFromUtf8(error_text),
        D2D1::RectF(24.0f, 96.0f, context.viewport_size.width - 24.0f, context.viewport_size.height - 56.0f),
        kScriptErrorText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP);
    draw.DrawBodyText(
        L"Press F5 to reload. Press Esc to close.",
        D2D1::RectF(24.0f, context.viewport_size.height - 44.0f, context.viewport_size.width - 24.0f, context.viewport_size.height - 16.0f),
        kScriptErrorMutedText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

} // namespace imgviewer
