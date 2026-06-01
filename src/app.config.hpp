#pragma once

#include <windows.h>

struct WindowSizeConfig final {
    int width = 960;
    int height = 640;
};

struct AppConfig final {
    bool remember_window_size = true;
    WindowSizeConfig window_size;
};

HRESULT LoadAppConfig(AppConfig* config);
HRESULT SaveAppConfig(const AppConfig& config);

