#pragma once

#include <span>

#define IMGVIEWER_ACTIONS(X) \
    X(OpenImage, "openImage", L"Open Image", true, true) \
    X(CaptureDesktop, "captureDesktop", L"Desktop Screenshot", true, true) \
    X(CaptureRegion, "captureRegion", L"Region Screenshot", true, true) \
    X(SaveImageAs, "saveImageAs", L"Save As", false, false) \
    X(ShowInFileExplorer, "showInFileExplorer", L"Show in File Explorer", true, true) \
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
    X(ToggleEditMode, "toggleEditMode", L"Edit Mode", true, true) \
    X(EditSelect, "editSelect", L"Edit Select", true, true) \
    X(EditPixelSelect, "editPixelSelect", L"Edit Pixel Select", true, true) \
    X(EditPen, "editPen", L"Edit Pen", true, true) \
    X(EditPenColorRed, "editPenColorRed", L"Pen Color Red", true, true) \
    X(EditPenColorYellow, "editPenColorYellow", L"Pen Color Yellow", true, true) \
    X(EditPenColorGreen, "editPenColorGreen", L"Pen Color Green", true, true) \
    X(EditPenColorCyan, "editPenColorCyan", L"Pen Color Cyan", true, true) \
    X(EditPenColorBlue, "editPenColorBlue", L"Pen Color Blue", true, true) \
    X(EditPenColorMagenta, "editPenColorMagenta", L"Pen Color Magenta", true, true) \
    X(EditPenColorWhite, "editPenColorWhite", L"Pen Color White", true, true) \
    X(EditPenColorBlack, "editPenColorBlack", L"Pen Color Black", true, true) \
    X(EditPenWidth2, "editPenWidth2", L"Pen Width 2px", true, true) \
    X(EditPenWidth4, "editPenWidth4", L"Pen Width 4px", true, true) \
    X(EditPenWidth8, "editPenWidth8", L"Pen Width 8px", true, true) \
    X(EditPenWidth12, "editPenWidth12", L"Pen Width 12px", true, true) \
    X(EditText, "editText", L"Edit Text", true, true) \
    X(EditTextFontChanged, "editTextFontChanged", L"Text Font", false, false) \
    X(EditTextSize12, "editTextSize12", L"Text Size 12px", true, true) \
    X(EditTextSize16, "editTextSize16", L"Text Size 16px", true, true) \
    X(EditTextSize20, "editTextSize20", L"Text Size 20px", true, true) \
    X(EditTextSize28, "editTextSize28", L"Text Size 28px", true, true) \
    X(EditTextSize36, "editTextSize36", L"Text Size 36px", true, true) \
    X(EditTextColorRed, "editTextColorRed", L"Text Color Red", true, true) \
    X(EditTextColorYellow, "editTextColorYellow", L"Text Color Yellow", true, true) \
    X(EditTextColorGreen, "editTextColorGreen", L"Text Color Green", true, true) \
    X(EditTextColorCyan, "editTextColorCyan", L"Text Color Cyan", true, true) \
    X(EditTextColorBlue, "editTextColorBlue", L"Text Color Blue", true, true) \
    X(EditTextColorMagenta, "editTextColorMagenta", L"Text Color Magenta", true, true) \
    X(EditTextColorWhite, "editTextColorWhite", L"Text Color White", true, true) \
    X(EditTextColorBlack, "editTextColorBlack", L"Text Color Black", true, true) \
    X(EditTextBackgroundTransparent, "editTextBackgroundTransparent", L"Text Background Transparent", true, true) \
    X(EditTextBackgroundYellow, "editTextBackgroundYellow", L"Text Background Yellow", true, true) \
    X(EditTextBackgroundWhite, "editTextBackgroundWhite", L"Text Background White", true, true) \
    X(EditTextBackgroundBlack, "editTextBackgroundBlack", L"Text Background Black", true, true) \
    X(EditTextBackgroundRed, "editTextBackgroundRed", L"Text Background Red", true, true) \
    X(EditTextBackgroundBlue, "editTextBackgroundBlue", L"Text Background Blue", true, true) \
    X(EditCrop, "editCrop", L"Edit Crop", true, true) \
    X(EditCancelCrop, "editCancelCrop", L"Cancel Crop", true, true) \
    X(EditCopySelection, "editCopySelection", L"Copy Pixel Selection", true, true) \
    X(EditMosaicSelection, "editMosaicSelection", L"Mosaic Pixel Selection", true, true) \
    X(EditDeleteSelection, "editDeleteSelection", L"Delete Edit Selection", true, true) \
    X(EditRotateClockwise, "editRotateClockwise", L"Edit Rotate Clockwise", true, true) \
    X(EditUndo, "editUndo", L"Edit Undo", true, true) \
    X(EditRedo, "editRedo", L"Edit Redo", true, true) \
    X(ToggleInfoPanel, "toggleInfoPanel", L"Info Panel", true, false) \
    X(ToggleAnimationLoop, "toggleAnimationLoop", L"Loop Animation", false, false) \
    X(ToggleAnimationPlayback, "toggleAnimationPlayback", L"Play or Pause Animation", false, false) \
    X(PreviousAnimationFrame, "previousAnimationFrame", L"Previous Animation Frame", false, false) \
    X(NextAnimationFrame, "nextAnimationFrame", L"Next Animation Frame", false, false) \
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
