#pragma once

#include <windows.h>

#include "imgviewer.keybindings.hpp"
#include "imgviewer.strings.hpp"

struct WindowSizeConfig final {
    int width = 480;
    int height = 320;
};

struct ImgViewerConfig final {
    ImgViewerLanguage language = ImgViewerLanguage::English;
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

HRESULT LoadImgViewerConfig(ImgViewerConfig* config);
HRESULT SaveImgViewerConfig(const ImgViewerConfig& config);
