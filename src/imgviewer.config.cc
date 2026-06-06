#include "imgviewer.config.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include <wil/result_macros.h>

namespace {

constexpr int kDefaultWindowWidth = 960;
constexpr int kDefaultWindowHeight = 640;
constexpr int kMinimumWindowWidth = 320;
constexpr int kMinimumWindowHeight = 240;
constexpr int kMinimumWindowOpacityPercent = 10;
constexpr int kMaximumWindowOpacityPercent = 100;
constexpr wchar_t kConfigFileName[] = L"imgviewer.jsonc";

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

} // namespace

int ClampWindowOpacityPercent(int percent)
{
    return (std::clamp)(percent, kMinimumWindowOpacityPercent, kMaximumWindowOpacityPercent);
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
        if (const auto remember = root.find("rememberWindowSize");
            remember != root.end() && remember->is_boolean()) {
            config->remember_window_size = remember->get<bool>();
        }

        if (const auto pixelated = root.find("pixelatedSampling");
            pixelated != root.end() && pixelated->is_boolean()) {
            config->pixelated_sampling = pixelated->get<bool>();
        }

        if (const auto borderless = root.find("borderlessWindow");
            borderless != root.end() && borderless->is_boolean()) {
            config->borderless_window = borderless->get<bool>();
        }

        if (const auto opacity = root.find("windowOpacity");
            opacity != root.end() && opacity->is_number_integer()) {
            config->window_opacity_percent = ClampWindowOpacityPercent(opacity->get<int>());
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
    output << "  // When true, ImgViewer restores the last normal window size on startup.\n";
    output << "  \"rememberWindowSize\": " << (config.remember_window_size ? "true" : "false") << ",\n";
    output << "  // When true, enlarged images use nearest-neighbor sampling for crisp pixel previews.\n";
    output << "  \"pixelatedSampling\": " << (config.pixelated_sampling ? "true" : "false") << ",\n";
    output << "  // When true, the main viewer window uses no native border or shadow.\n";
    output << "  \"borderlessWindow\": " << (config.borderless_window ? "true" : "false") << ",\n";
    output << "  // Main viewer window opacity, clamped from 5 to 100 percent.\n";
    output << "  \"windowOpacity\": " << ClampWindowOpacityPercent(config.window_opacity_percent) << ",\n";
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
    output << "    \"fitWindow\": [\"Ctrl+9\"],\n";
    output << "    \"actualSize\": [\"Ctrl+1\"],\n";
    output << "    \"resetView\": [\"Ctrl+0\"],\n";
    output << "    \"openImage\": [\"Ctrl+O\"]\n";
    output << "  }\n";
    output << "}\n";

    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_WRITE_FAULT), !output);
    return S_OK;
}
