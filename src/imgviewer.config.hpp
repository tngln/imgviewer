#pragma once

#include <windows.h>

#include <string>

#include "imgviewer.keybindings.hpp"

namespace script {
class QuickJsRuntime;
}

struct WindowSizeConfig final {
    int width = 960;
    int height = 640;
};

enum class InitialImageViewMode {
    FitWindow,
    ActualSize,
};

struct ImgViewerConfig final {
    std::string language = "en-US";
    InitialImageViewMode initial_image_view_mode = InitialImageViewMode::FitWindow;
    bool remember_window_size = true;
    bool pixelated_sampling = false;
    bool checkerboard_background = false;
    bool borderless_window = false;
    bool edge_click_navigation = false;
    int window_opacity_percent = 100;
    int toolbar_scale_percent = 125;
    int edge_click_navigation_zone_percent = 10;
    WindowSizeConfig window_size;
    ActionBindings action_bindings;
};

int ClampWindowOpacityPercent(int percent);
int ClampToolbarScalePercent(int percent);
int ClampEdgeClickNavigationZonePercent(int percent);

HRESULT LoadImgViewerConfig(script::QuickJsRuntime& runtime, ImgViewerConfig* config);
HRESULT SaveImgViewerConfig(const ImgViewerConfig& config);
