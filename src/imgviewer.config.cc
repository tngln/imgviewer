#include "imgviewer.config.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string_view>

#include <quickjs.h>
#include <wil/result_macros.h>

#include "script.quickjs_runtime.hpp"

namespace {

constexpr int kDefaultWindowWidth = 960;
constexpr int kDefaultWindowHeight = 640;
constexpr int kMinimumWindowWidth = 160;
constexpr int kMinimumWindowHeight = 120;
constexpr int kMinimumWindowOpacityPercent = 10;
constexpr int kMaximumWindowOpacityPercent = 100;
constexpr int kMinimumToolbarScalePercent = 80;
constexpr int kMaximumToolbarScalePercent = 160;
constexpr int kMinimumEdgeClickNavigationZonePercent = 5;
constexpr int kMaximumEdgeClickNavigationZonePercent = 40;
constexpr wchar_t kConfigFileName[] = L"imgviewer.config.js";
constexpr std::string_view kEnglishLanguageName = "en-US";
constexpr std::string_view kZhCnLanguageName = "zh-CN";

struct JsValue final {
    JSContext* context = nullptr;
    JSValue value = JS_UNDEFINED;

    JsValue(JSContext* context, JSValue value) : context(context), value(value) {}
    JsValue(const JsValue&) = delete;
    JsValue& operator=(const JsValue&) = delete;
    ~JsValue()
    {
        JS_FreeValue(context, value);
    }
};

std::filesystem::path ConfigFilePath()
{
    std::wstring module_path(MAX_PATH, L'\0');
    DWORD length = 0;
    while (true) {
        length = GetModuleFileNameW(nullptr, module_path.data(), static_cast<DWORD>(module_path.size()));
        if (length == 0) {
            return std::filesystem::path(kConfigFileName);
        }

        if (length < module_path.size()) {
            module_path.resize(length);
            break;
        }

        module_path.resize(module_path.size() * 2);
    }

    std::filesystem::path path(module_path);
    path.remove_filename();
    path /= kConfigFileName;
    return path;
}

std::string ReadFileUtf8(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }

    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::string JsString(JSContext* context, JSValueConst value)
{
    const char* text = JS_ToCString(context, value);
    if (text == nullptr) {
        return {};
    }

    std::string result(text);
    JS_FreeCString(context, text);
    return result;
}

std::string EscapeJsString(std::string_view value)
{
    std::string escaped;
    for (char ch : value) {
        switch (ch) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped.push_back(ch);
            break;
        }
    }
    return escaped;
}

bool ReadStringProperty(JSContext* context, JSValueConst object, const char* key, std::string* result)
{
    JsValue value(context, JS_GetPropertyStr(context, object, key));
    if (!JS_IsString(value.value)) {
        return false;
    }

    *result = JsString(context, value.value);
    return true;
}

bool ReadBoolProperty(JSContext* context, JSValueConst object, const char* key, bool* result)
{
    JsValue value(context, JS_GetPropertyStr(context, object, key));
    if (!JS_IsBool(value.value)) {
        return false;
    }

    *result = JS_ToBool(context, value.value) != 0;
    return true;
}

bool ReadIntProperty(JSContext* context, JSValueConst object, const char* key, int* result)
{
    JsValue value(context, JS_GetPropertyStr(context, object, key));
    if (!JS_IsNumber(value.value)) {
        return false;
    }

    int32_t parsed = 0;
    if (JS_ToInt32(context, &parsed, value.value) != 0) {
        return false;
    }
    *result = parsed;
    return true;
}

int ReadClampedIntProperty(
    JSContext* context,
    JSValueConst object,
    const char* key,
    int fallback,
    int minimum)
{
    int value = fallback;
    if (!ReadIntProperty(context, object, key, &value)) {
        return fallback;
    }
    return (std::max)(minimum, value);
}

std::string ReadLanguage(JSContext* context, JSValueConst root)
{
    std::string language;
    if (!ReadStringProperty(context, root, "language", &language)) {
        return std::string(kEnglishLanguageName);
    }

    if (language == kZhCnLanguageName) {
        return std::string(kZhCnLanguageName);
    }
    return std::string(kEnglishLanguageName);
}

std::string_view LanguageConfigName(const std::string& language)
{
    return language == kZhCnLanguageName ?
        kZhCnLanguageName :
        kEnglishLanguageName;
}

InitialImageViewMode ReadInitialImageViewMode(JSContext* context, JSValueConst root)
{
    std::string mode;
    if (!ReadStringProperty(context, root, "initialImageView", &mode)) {
        return InitialImageViewMode::FitWindow;
    }

    if (mode == "actualSize") {
        return InitialImageViewMode::ActualSize;
    }
    return InitialImageViewMode::FitWindow;
}

std::string_view InitialImageViewModeConfigName(InitialImageViewMode mode)
{
    return mode == InitialImageViewMode::ActualSize ?
        "actualSize" :
        "fitWindow";
}

bool ReadArrayLength(JSContext* context, JSValueConst array, uint32_t* length)
{
    JsValue length_value(context, JS_GetPropertyStr(context, array, "length"));
    return JS_ToUint32(context, length, length_value.value) == 0;
}

std::vector<std::string> ReadStringArray(JSContext* context, JSValueConst value)
{
    std::vector<std::string> result;
    if (JS_IsArray(value) != 1) {
        return result;
    }

    uint32_t length = 0;
    if (!ReadArrayLength(context, value, &length)) {
        return result;
    }

    result.reserve(length);
    for (uint32_t index = 0; index < length; ++index) {
        JsValue item(context, JS_GetPropertyUint32(context, value, index));
        if (JS_IsString(item.value)) {
            result.push_back(JsString(context, item.value));
        }
    }
    return result;
}

void ApplyKeyBindingsObject(JSContext* context, JSValueConst root, ActionBindings* bindings)
{
    JsValue key_bindings(context, JS_GetPropertyStr(context, root, "keyBindings"));
    if (!JS_IsObject(key_bindings.value)) {
        return;
    }

    for (const ImgViewerActionInfo& info : ImgViewerActions()) {
        if (!info.configurable_key) {
            continue;
        }

        JsValue item(context, JS_GetPropertyStr(context, key_bindings.value, info.name));
        if (JS_IsArray(item.value) == 1) {
            ApplyKeyBindingConfig(info.action, ReadStringArray(context, item.value), bindings);
        }
    }
}

void LoadConfigObject(JSContext* context, JSValueConst root, ImgViewerConfig* config)
{
    if (!JS_IsObject(root)) {
        return;
    }

    config->language = ReadLanguage(context, root);
    config->initial_image_view_mode = ReadInitialImageViewMode(context, root);

    ReadBoolProperty(context, root, "rememberWindowSize", &config->remember_window_size);
    ReadBoolProperty(context, root, "pixelatedSampling", &config->pixelated_sampling);
    ReadBoolProperty(context, root, "checkerboardBackground", &config->checkerboard_background);
    ReadBoolProperty(context, root, "borderlessWindow", &config->borderless_window);
    ReadBoolProperty(context, root, "edgeClickNavigation", &config->edge_click_navigation);

    int value = 0;
    if (ReadIntProperty(context, root, "windowOpacity", &value)) {
        config->window_opacity_percent = ClampWindowOpacityPercent(value);
    }
    if (ReadIntProperty(context, root, "toolbarScale", &value)) {
        config->toolbar_scale_percent = ClampToolbarScalePercent(value);
    }
    if (ReadIntProperty(context, root, "edgeClickNavigationZone", &value)) {
        config->edge_click_navigation_zone_percent = ClampEdgeClickNavigationZonePercent(value);
    }

    JsValue window(context, JS_GetPropertyStr(context, root, "window"));
    if (JS_IsObject(window.value)) {
        config->window_size.width =
            ReadClampedIntProperty(context, window.value, "width", kDefaultWindowWidth, kMinimumWindowWidth);
        config->window_size.height =
            ReadClampedIntProperty(context, window.value, "height", kDefaultWindowHeight, kMinimumWindowHeight);
    }

    ApplyKeyBindingsObject(context, root, &config->action_bindings);
}

void WriteKeyBindings(std::ofstream& output, const ActionBindings& bindings)
{
    output << "  // Optional keyboard overrides. Missing actions keep their defaults.\n";
    output << "  keyBindings: {\n";
    for (const ImgViewerActionInfo& info : ImgViewerActions()) {
        if (!info.configurable_key) {
            continue;
        }

        output << "    " << info.name << ": [";
        bool first = true;
        for (const KeyBinding& binding : bindings.key_bindings) {
            if (binding.action != info.action) {
                continue;
            }
            if (!first) {
                output << ", ";
            }
            first = false;
            output << "\"" << EscapeJsString(GestureConfigText(binding.gesture)) << "\"";
        }
        output << "],\n";
    }
    output << "  }\n";
}

} // namespace

int ClampWindowOpacityPercent(int percent)
{
    return (std::clamp)(percent, kMinimumWindowOpacityPercent, kMaximumWindowOpacityPercent);
}

int ClampToolbarScalePercent(int percent)
{
    return (std::clamp)(percent, kMinimumToolbarScalePercent, kMaximumToolbarScalePercent);
}

int ClampEdgeClickNavigationZonePercent(int percent)
{
    return (std::clamp)(percent, kMinimumEdgeClickNavigationZonePercent, kMaximumEdgeClickNavigationZonePercent);
}

HRESULT LoadImgViewerConfig(script::QuickJsRuntime& runtime, ImgViewerConfig* config)
{
    RETURN_HR_IF_NULL(E_POINTER, config);

    *config = ImgViewerConfig{};
    config->action_bindings = DefaultActionBindings();

    const std::filesystem::path path = ConfigFilePath();
    const std::string source = ReadFileUtf8(path);
    if (source.empty()) {
        return S_OK;
    }

    std::unique_ptr<script::QuickJsContext> script_context = runtime.CreateContext();
    if (script_context == nullptr) {
        return S_OK;
    }

    const script::QuickJsEvalResult eval = script_context->EvalScript(source, path.string());
    if (!eval.ok) {
        runtime.TakeExceptionTextUtf8();
        return S_OK;
    }

    JSContext* context = script_context->Context();
    JsValue global(context, JS_GetGlobalObject(context));
    JsValue root(context, JS_GetPropertyStr(context, global.value, "imgviewerConfig"));
    LoadConfigObject(context, root.value, config);
    return S_OK;
}

HRESULT SaveImgViewerConfig(const ImgViewerConfig& config)
{
    const std::filesystem::path path = ConfigFilePath();
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED), !output);

    output << "globalThis.imgviewerConfig = {\n";
    output << "  // UI language. Supported values: en-US, zh-CN.\n";
    output << "  language: \"" << LanguageConfigName(config.language) << "\",\n";
    output << "  // Initial view for newly loaded images. Supported values: fitWindow, actualSize.\n";
    output << "  initialImageView: \"" << InitialImageViewModeConfigName(config.initial_image_view_mode) << "\",\n";
    output << "  // When true, ImgViewer restores the last normal window size on startup.\n";
    output << "  rememberWindowSize: " << (config.remember_window_size ? "true" : "false") << ",\n";
    output << "  // When true, enlarged images use nearest-neighbor sampling for crisp pixel previews.\n";
    output << "  pixelatedSampling: " << (config.pixelated_sampling ? "true" : "false") << ",\n";
    output << "  // When true, the image canvas uses a checkerboard background for transparent pixels.\n";
    output << "  checkerboardBackground: " << (config.checkerboard_background ? "true" : "false") << ",\n";
    output << "  // When true, the main viewer window uses no native border or shadow.\n";
    output << "  borderlessWindow: " << (config.borderless_window ? "true" : "false") << ",\n";
    output << "  // When true, clicking the left or right window edge navigates images.\n";
    output << "  edgeClickNavigation: " << (config.edge_click_navigation ? "true" : "false") << ",\n";
    output << "  // Main viewer window opacity, clamped from 10 to 100 percent.\n";
    output << "  windowOpacity: " << ClampWindowOpacityPercent(config.window_opacity_percent) << ",\n";
    output << "  // Main viewer toolbar size, clamped from 80 to 160 percent.\n";
    output << "  toolbarScale: " << ClampToolbarScalePercent(config.toolbar_scale_percent) << ",\n";
    output << "  // Left and right edge click areas, clamped from 5 to 40 percent of window width.\n";
    output << "  edgeClickNavigationZone: "
        << ClampEdgeClickNavigationZonePercent(config.edge_click_navigation_zone_percent) << ",\n";
    output << "  window: {\n";
    output << "    width: " << (std::max)(kMinimumWindowWidth, config.window_size.width) << ",\n";
    output << "    height: " << (std::max)(kMinimumWindowHeight, config.window_size.height) << "\n";
    output << "  },\n";
    WriteKeyBindings(output, config.action_bindings);
    output << "};\n";

    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_WRITE_FAULT), !output);
    return S_OK;
}
