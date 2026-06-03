#pragma once

#include <windows.h>

#include "app.keybindings.hpp"

struct WindowSizeConfig final {
    int width = 960;
    int height = 640;
};

struct AppConfig final {
    bool remember_window_size = true;
    WindowSizeConfig window_size;
    ActionBindings action_bindings;
};

HRESULT LoadAppConfig(AppConfig* config);
HRESULT SaveAppConfig(const AppConfig& config);
