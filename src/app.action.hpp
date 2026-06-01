#pragma once

enum class AppAction {
    None,
    OpenImage,
    PreviousImage,
    NextImage,
    ZoomIn,
    ZoomOut,
    RotateClockwise,
    ResetView,
    ToggleTopMost,
    Minimize,
    ToggleMaximize,
    Close,
};

const char* AppActionName(AppAction action);
AppAction AppActionFromName(const char* name);
