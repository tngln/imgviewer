#pragma once

#include <span>

#define IMGVIEWER_ACTIONS(X) \
    X(OpenImage, "openImage", L"Open Image", true, true) \
    X(SaveImageAs, "saveImageAs", L"Save As", false, false) \
    X(PreviousImage, "previousImage", L"Previous Image", true, true) \
    X(NextImage, "nextImage", L"Next Image", true, true) \
    X(ZoomIn, "zoomIn", L"Zoom In", true, true) \
    X(ZoomOut, "zoomOut", L"Zoom Out", true, true) \
    X(FitWindow, "fitWindow", L"Fit Window", true, true) \
    X(ActualSize, "actualSize", L"Actual Size", true, true) \
    X(RotateClockwise, "rotateClockwise", L"Rotate Clockwise", true, true) \
    X(FlipHorizontal, "flipHorizontal", L"Flip Horizontal", true, true) \
    X(FlipVertical, "flipVertical", L"Flip Vertical", true, true) \
    X(ResetView, "resetView", L"Reset View", true, true) \
    X(ToggleColorPicker, "toggleColorPicker", L"Color Picker", false, false) \
    X(ToggleInfoPanel, "toggleInfoPanel", L"Info Panel", true, false) \
    X(OpenMenu, "openMenu", L"Menu", false, false) \
    X(OpenSettings, "openSettings", L"Settings", false, false) \
    X(CloseSettings, "closeSettings", L"Close Settings", false, false) \
    X(SaveSettings, "saveSettings", L"Save Settings", false, false) \
    X(OpenAbout, "openAbout", L"About", false, false) \
    X(CloseAbout, "closeAbout", L"Close About", false, false) \
    X(CopyAboutNotices, "copyAboutNotices", L"Copy About Notices", false, false) \
    X(ResetKeyBindings, "resetKeyBindings", L"Reset Shortcuts", false, false) \
    X(TextCopy, "textCopy", L"Copy", false, false) \
    X(TextCut, "textCut", L"Cut", false, false) \
    X(TextPaste, "textPaste", L"Paste", false, false) \
    X(TextSelectAll, "textSelectAll", L"Select All", false, false) \
    X(ToggleTopMost, "toggleTopMost", L"Top Most", false, false) \
    X(Minimize, "minimize", L"Minimize", false, false) \
    X(ToggleMaximize, "toggleMaximize", L"Maximize or Restore", false, false) \
    X(Close, "close", L"Close", false, false)

enum class ImgViewerAction {
    None,
#define IMGVIEWER_ACTION_ENUM(value, name, display_name, configurable_key, shown_in_settings) value,
    IMGVIEWER_ACTIONS(IMGVIEWER_ACTION_ENUM)
#undef IMGVIEWER_ACTION_ENUM
};

struct ImgViewerActionInfo final {
    ImgViewerAction action = ImgViewerAction::None;
    const char* name = "";
    const wchar_t* display_name = L"";
    bool configurable_key = false;
    bool shown_in_settings = false;
};

std::span<const ImgViewerActionInfo> ImgViewerActions();
const ImgViewerActionInfo* ImgViewerActionInfoFor(ImgViewerAction action);
const char* ImgViewerActionName(ImgViewerAction action);
const wchar_t* ImgViewerActionDisplayName(ImgViewerAction action);
ImgViewerAction ImgViewerActionFromName(const char* name);
bool IsImgViewerActionConfigurableKeyBinding(ImgViewerAction action);
bool IsImgViewerActionShownInSettings(ImgViewerAction action);
