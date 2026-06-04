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
    int window_opacity_percent = 100;
    WindowSizeConfig window_size;
    ActionBindings action_bindings;
};

int ClampWindowOpacityPercent(int percent);

HRESULT LoadImgViewerConfig(ImgViewerConfig* config);
HRESULT SaveImgViewerConfig(const ImgViewerConfig& config);
