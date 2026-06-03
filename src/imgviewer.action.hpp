#pragma once

enum class ImgViewerAction {
    None,
    OpenImage,
    PreviousImage,
    NextImage,
    ZoomIn,
    ZoomOut,
    RotateClockwise,
    FlipHorizontal,
    FlipVertical,
    ResetView,
    OpenMenu,
    OpenSettings,
    CloseSettings,
    SaveSettings,
    ResetKeyBindings,
    ToggleTopMost,
    Minimize,
    ToggleMaximize,
    Close,
};

const char* ImgViewerActionName(ImgViewerAction action);
ImgViewerAction ImgViewerActionFromName(const char* name);
