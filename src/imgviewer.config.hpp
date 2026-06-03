#pragma once

#include <windows.h>

#include "imgviewer.keybindings.hpp"

struct WindowSizeConfig final {
    int width = 960;
    int height = 640;
};

struct ImgViewerConfig final {
    bool remember_window_size = true;
    WindowSizeConfig window_size;
    ActionBindings action_bindings;
};

HRESULT LoadImgViewerConfig(ImgViewerConfig* config);
HRESULT SaveImgViewerConfig(const ImgViewerConfig& config);
