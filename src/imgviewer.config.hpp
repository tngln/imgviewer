#pragma once

#include <windows.h>

#include "imgviewer.keybindings.hpp"

struct WindowSizeConfig final {
    int width = 960;
    int height = 640;
};

struct ImgViewerConfig final {
    bool remember_window_size = true;
    bool pixelated_sampling = false;
    bool borderless_window = false;
    int window_opacity_percent = 100;
    int toolbar_scale_percent = 125;
    WindowSizeConfig window_size;
    ActionBindings action_bindings;
};

int ClampWindowOpacityPercent(int percent);
int ClampToolbarScalePercent(int percent);

HRESULT LoadImgViewerConfig(ImgViewerConfig* config);
HRESULT SaveImgViewerConfig(const ImgViewerConfig& config);
