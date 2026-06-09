#include "imgviewer.config.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string_view>

#include <nlohmann/json.hpp>

#include <wil/result_macros.h>

namespace {

constexpr int kDefaultWindowWidth = 480;
constexpr int kDefaultWindowHeight = 320;
constexpr int kMinimumWindowWidth = 160;
constexpr int kMinimumWindowHeight = 120;
constexpr int kMinimumWindowOpacityPercent = 10;
constexpr int kMaximumWindowOpacityPercent = 100;
constexpr int kMinimumToolbarScalePercent = 80;
constexpr int kMaximumToolbarScalePercent = 160;
constexpr int kMinimumEdgeClickNavigationZonePercent = 5;
constexpr int kMaximumEdgeClickNavigationZonePercent = 40;
constexpr wchar_t kConfigFileName[] = L"imgviewer.jsonc";
constexpr std::string_view kEnglishLanguageName = "en-US";
constexpr std::string_view kSimplifiedChineseLanguageName = "zh-CN";

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

int ReadClampedInt(const nlohmann::json& object, const char* key, int fallback, int minimum)
{
    const auto value = object.find(key);
    if (value == object.end() || !value->is_number_integer()) {
        return fallback;
    }

    return (std::max)(minimum, value->get<int>());
}

ImgViewerLanguage ReadLanguage(const nlohmann::json& root)
{
    const auto value = root.find("language");
    if (value == root.end() || !value->is_string()) {
        return ImgViewerLanguage::English;
    }

    const std::string language = value->get<std::string>();
    if (language == kSimplifiedChineseLanguageName) {
        return ImgViewerLanguage::SimplifiedChinese;
    }
    return ImgViewerLanguage::English;
}

std::string_view LanguageConfigName(ImgViewerLanguage language)
{
    return language == ImgViewerLanguage::SimplifiedChinese ?
        kSimplifiedChineseLanguageName :
        kEnglishLanguageName;
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

HRESULT LoadImgViewerConfig(ImgViewerConfig* config)
{
    RETURN_HR_IF_NULL(E_POINTER, config);

    *config = ImgViewerConfig{};
    config->action_bindings = DefaultActionBindings();
    const std::filesystem::path path = ConfigFilePath();
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return S_OK;
    }

    try {
        const nlohmann::json root = nlohmann::json::parse(input, nullptr, true, true);
        config->language = ReadLanguage(root);

        if (const auto remember = root.find("rememberWindowSize");
            remember != root.end() && remember->is_boolean()) {
            config->remember_window_size = remember->get<bool>();
        }

        if (const auto pixelated = root.find("pixelatedSampling");
            pixelated != root.end() && pixelated->is_boolean()) {
            config->pixelated_sampling = pixelated->get<bool>();
        }

        if (const auto checkerboard = root.find("checkerboardBackground");
            checkerboard != root.end() && checkerboard->is_boolean()) {
            config->checkerboard_background = checkerboard->get<bool>();
        }

        if (const auto borderless = root.find("borderlessWindow");
            borderless != root.end() && borderless->is_boolean()) {
            config->borderless_window = borderless->get<bool>();
        }

        if (const auto edge_click = root.find("edgeClickNavigation");
            edge_click != root.end() && edge_click->is_boolean()) {
            config->edge_click_navigation = edge_click->get<bool>();
        }

        if (const auto opacity = root.find("windowOpacity");
            opacity != root.end() && opacity->is_number_integer()) {
            config->window_opacity_percent = ClampWindowOpacityPercent(opacity->get<int>());
        }

        if (const auto toolbar_scale = root.find("toolbarScale");
            toolbar_scale != root.end() && toolbar_scale->is_number_integer()) {
            config->toolbar_scale_percent = ClampToolbarScalePercent(toolbar_scale->get<int>());
        }

        if (const auto edge_click_zone = root.find("edgeClickNavigationZone");
            edge_click_zone != root.end() && edge_click_zone->is_number_integer()) {
            config->edge_click_navigation_zone_percent =
                ClampEdgeClickNavigationZonePercent(edge_click_zone->get<int>());
        }

        if (const auto window = root.find("window");
            window != root.end() && window->is_object()) {
            config->window_size.width =
                ReadClampedInt(*window, "width", kDefaultWindowWidth, kMinimumWindowWidth);
            config->window_size.height =
                ReadClampedInt(*window, "height", kDefaultWindowHeight, kMinimumWindowHeight);
        }

        ApplyKeyBindingsConfig(root, &config->action_bindings);
    } catch (const nlohmann::json::exception&) {
        return S_OK;
    }

    return S_OK;
}

HRESULT SaveImgViewerConfig(const ImgViewerConfig& config)
{
    const std::filesystem::path path = ConfigFilePath();
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED), !output);

    output << "{\n";
    output << "  // UI language. Supported values: en-US, zh-CN.\n";
    output << "  \"language\": \"" << LanguageConfigName(config.language) << "\",\n";
    output << "  // When true, ImgViewer restores the last normal window size on startup.\n";
    output << "  \"rememberWindowSize\": " << (config.remember_window_size ? "true" : "false") << ",\n";
    output << "  // When true, enlarged images use nearest-neighbor sampling for crisp pixel previews.\n";
    output << "  \"pixelatedSampling\": " << (config.pixelated_sampling ? "true" : "false") << ",\n";
    output << "  // When true, the image canvas uses a checkerboard background for transparent pixels.\n";
    output << "  \"checkerboardBackground\": " << (config.checkerboard_background ? "true" : "false") << ",\n";
    output << "  // When true, the main viewer window uses no native border or shadow.\n";
    output << "  \"borderlessWindow\": " << (config.borderless_window ? "true" : "false") << ",\n";
    output << "  // When true, clicking the left or right window edge navigates images.\n";
    output << "  \"edgeClickNavigation\": " << (config.edge_click_navigation ? "true" : "false") << ",\n";
    output << "  // Main viewer window opacity, clamped from 10 to 100 percent.\n";
    output << "  \"windowOpacity\": " << ClampWindowOpacityPercent(config.window_opacity_percent) << ",\n";
    output << "  // Main viewer toolbar size, clamped from 80 to 160 percent.\n";
    output << "  \"toolbarScale\": " << ClampToolbarScalePercent(config.toolbar_scale_percent) << ",\n";
    output << "  // Left and right edge click areas, clamped from 5 to 40 percent of window width.\n";
    output << "  \"edgeClickNavigationZone\": "
        << ClampEdgeClickNavigationZonePercent(config.edge_click_navigation_zone_percent) << ",\n";
    output << "  \"window\": {\n";
    output << "    \"width\": " << (std::max)(kMinimumWindowWidth, config.window_size.width) << ",\n";
    output << "    \"height\": " << (std::max)(kMinimumWindowHeight, config.window_size.height) << "\n";
    output << "  },\n";
    output << "  // Optional keyboard overrides. Missing actions keep their defaults.\n";
    output << "  \"keyBindings\": {\n";
    output << "    \"previousImage\": [\"Left\"],\n";
    output << "    \"nextImage\": [\"Right\"],\n";
    output << "    \"rotateClockwise\": [\"R\"],\n";
    output << "    \"flipHorizontal\": [\"H\"],\n";
    output << "    \"flipVertical\": [\"V\"],\n";
    output << "    \"zoomIn\": [\"Ctrl+=\"],\n";
    output << "    \"zoomOut\": [\"Ctrl+-\"],\n";
    output << "    \"toggleInfoPanel\": [\"I\"],\n";
    output << "    \"fitWindow\": [\"Ctrl+9\"],\n";
    output << "    \"actualSize\": [\"Ctrl+1\"],\n";
    output << "    \"resetView\": [\"Ctrl+0\"],\n";
    output << "    \"openImage\": [\"Ctrl+O\"]\n";
    output << "  }\n";
    output << "}\n";

    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_WRITE_FAULT), !output);
    return S_OK;
}
