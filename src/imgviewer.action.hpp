#pragma once

enum class ImgViewerAction {
    None,
    OpenImage,
    SaveImageAs,
    PreviousImage,
    NextImage,
    ZoomIn,
    ZoomOut,
    FitWindow,
    ActualSize,
    RotateClockwise,
    FlipHorizontal,
    FlipVertical,
    ResetView,
    ToggleColorPicker,
    ToggleInfoPanel,
    OpenMenu,
    OpenSettings,
    CloseSettings,
    SaveSettings,
    OpenAbout,
    CloseAbout,
    CopyAboutNotices,
    ResetKeyBindings,
    TextCopy,
    TextCut,
    TextPaste,
    TextSelectAll,
    ToggleTopMost,
    Minimize,
    ToggleMaximize,
    Close,
};

const char* ImgViewerActionName(ImgViewerAction action);
ImgViewerAction ImgViewerActionFromName(const char* name);
