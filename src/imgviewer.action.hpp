#pragma once

#include <span>

#include "imgviewer.strings.hpp"

#define IMGVIEWER_IMAGE_ACTIONS(X) \
    X(SaveImageAs, "saveImageAs", ImgViewerStringId::SaveAs, false, false) \
    X(ZoomIn, "zoomIn", ImgViewerStringId::ZoomIn, true, true) \
    X(ZoomOut, "zoomOut", ImgViewerStringId::ZoomOut, true, true) \
    X(FitWindow, "fitWindow", ImgViewerStringId::FitWindow, true, true) \
    X(ActualSize, "actualSize", ImgViewerStringId::ActualSize, true, true) \
    X(RotateClockwise, "rotateClockwise", ImgViewerStringId::RotateClockwise, true, true) \
    X(FlipHorizontal, "flipHorizontal", ImgViewerStringId::FlipHorizontal, true, true) \
    X(FlipVertical, "flipVertical", ImgViewerStringId::FlipVertical, true, true) \
    X(ResetView, "resetView", ImgViewerStringId::ResetView, true, true) \
    X(ToggleColorPicker, "toggleColorPicker", ImgViewerStringId::ColorPicker, false, false)

#define IMGVIEWER_SEQUENCE_ACTIONS(X) \
    X(PreviousImage, "previousImage", ImgViewerStringId::PreviousImage, true, true) \
    X(NextImage, "nextImage", ImgViewerStringId::NextImage, true, true)

#define IMGVIEWER_EDIT_ACTIONS(X) \
    X(ToggleEditMode, "toggleEditMode", ImgViewerStringId::EditMode, true, true) \
    X(EditSelect, "editSelect", ImgViewerStringId::EditSelect, true, true) \
    X(EditPixelSelect, "editPixelSelect", ImgViewerStringId::EditPixelSelect, true, true) \
    X(EditPen, "editPen", ImgViewerStringId::EditPen, true, true) \
    X(EditSetPenColor, "editSetPenColor", ImgViewerStringId::RedPen, true, true) \
    X(EditSetPenWidth, "editSetPenWidth", ImgViewerStringId::PenWidth2, true, true) \
    X(EditShape, "editShape", ImgViewerStringId::EditShape, true, true) \
    X(EditSetShapeKind, "editSetShapeKind", ImgViewerStringId::RectangleShape, true, true) \
    X(EditText, "editText", ImgViewerStringId::EditText, true, true) \
    X(EditTextFontChanged, "editTextFontChanged", ImgViewerStringId::TextFont, false, false) \
    X(EditSetTextFontSize, "editSetTextFontSize", ImgViewerStringId::TextSize12, true, true) \
    X(EditSetTextColor, "editSetTextColor", ImgViewerStringId::TextColorRed, true, true) \
    X(EditSetTextBackground, "editSetTextBackground", ImgViewerStringId::TextBackgroundTransparent, true, true) \
    X(EditCrop, "editCrop", ImgViewerStringId::EditCrop, true, true) \
    X(EditCancelCrop, "editCancelCrop", ImgViewerStringId::EditCancelCrop, true, true) \
    X(EditCopySelection, "editCopySelection", ImgViewerStringId::EditCopySelection, true, true) \
    X(EditMosaicSelection, "editMosaicSelection", ImgViewerStringId::EditMosaicSelection, true, true) \
    X(EditDeleteSelection, "editDeleteSelection", ImgViewerStringId::EditDeleteSelection, true, true) \
    X(EditRotateClockwise, "editRotateClockwise", ImgViewerStringId::EditRotateClockwise, true, true) \
    X(EditUndo, "editUndo", ImgViewerStringId::EditUndo, true, true) \
    X(EditRedo, "editRedo", ImgViewerStringId::EditRedo, true, true)

#define IMGVIEWER_ANIMATION_ACTIONS(X) \
    X(ToggleAnimationLoop, "toggleAnimationLoop", ImgViewerStringId::LoopAnimation, false, false) \
    X(ToggleAnimationPlayback, "toggleAnimationPlayback", ImgViewerStringId::PlayOrPauseAnimation, false, false) \
    X(PreviousAnimationFrame, "previousAnimationFrame", ImgViewerStringId::PreviousAnimationFrame, false, false) \
    X(NextAnimationFrame, "nextAnimationFrame", ImgViewerStringId::NextAnimationFrame, false, false)

#define IMGVIEWER_OTHER_ACTIONS(X) \
    X(OpenImage, "openImage", ImgViewerStringId::OpenImage, true, true) \
    X(CaptureRegion, "captureRegion", ImgViewerStringId::CaptureRegion, true, true) \
    X(ShowInFileExplorer, "showInFileExplorer", ImgViewerStringId::ShowInFileExplorer, true, true) \
    X(CopyColorPickerValue, "copyColorPickerValue", ImgViewerStringId::CopyColorPickerValue, false, false) \
    X(ToggleInfoPanel, "toggleInfoPanel", ImgViewerStringId::ToggleInfoPanel, true, false) \
    X(OpenMenu, "openMenu", ImgViewerStringId::Menu, false, false) \
    X(OpenSettings, "openSettings", ImgViewerStringId::Settings, false, false) \
    X(CloseSettings, "closeSettings", ImgViewerStringId::CloseSettings, false, false) \
    X(SaveSettings, "saveSettings", ImgViewerStringId::SaveSettings, false, false) \
    X(OpenDeveloper, "openDeveloper", ImgViewerStringId::Developer, false, false) \
    X(CloseDeveloper, "closeDeveloper", ImgViewerStringId::CloseDeveloper, false, false) \
    X(OpenAbout, "openAbout", ImgViewerStringId::About, false, false) \
    X(CloseAbout, "closeAbout", ImgViewerStringId::CloseAbout, false, false) \
    X(ResetKeyBindings, "resetKeyBindings", ImgViewerStringId::ResetShortcuts, false, false) \
    X(TextCopy, "textCopy", ImgViewerStringId::Copy, false, false) \
    X(TextCut, "textCut", ImgViewerStringId::Cut, false, false) \
    X(TextPaste, "textPaste", ImgViewerStringId::Paste, false, false) \
    X(TextSelectAll, "textSelectAll", ImgViewerStringId::SelectAll, false, false) \
    X(ToggleTopMost, "toggleTopMost", ImgViewerStringId::TopMost, false, false) \
    X(Minimize, "minimize", ImgViewerStringId::Minimize, false, false) \
    X(ToggleMaximize, "toggleMaximize", ImgViewerStringId::MaximizeOrRestore, false, false) \
    X(Close, "close", ImgViewerStringId::Close, false, false)

#define IMGVIEWER_ACTIONS(X) \
    IMGVIEWER_OTHER_ACTIONS(X) \
    IMGVIEWER_IMAGE_ACTIONS(X) \
    IMGVIEWER_SEQUENCE_ACTIONS(X) \
    IMGVIEWER_EDIT_ACTIONS(X) \
    IMGVIEWER_ANIMATION_ACTIONS(X)

enum class ImgViewerAction {
    None,
    ImageActionBegin,
#define IMGVIEWER_ACTION_ENUM(value, name, display_name, configurable_key, shown_in_settings) value,
    IMGVIEWER_IMAGE_ACTIONS(IMGVIEWER_ACTION_ENUM)
#undef IMGVIEWER_ACTION_ENUM
    ImageActionEnd,
    SequenceActionBegin,
#define IMGVIEWER_ACTION_ENUM(value, name, display_name, configurable_key, shown_in_settings) value,
    IMGVIEWER_SEQUENCE_ACTIONS(IMGVIEWER_ACTION_ENUM)
#undef IMGVIEWER_ACTION_ENUM
    SequenceActionEnd,
    EditActionBegin,
#define IMGVIEWER_ACTION_ENUM(value, name, display_name, configurable_key, shown_in_settings) value,
    IMGVIEWER_EDIT_ACTIONS(IMGVIEWER_ACTION_ENUM)
#undef IMGVIEWER_ACTION_ENUM
    EditActionEnd,
    AnimationActionBegin,
#define IMGVIEWER_ACTION_ENUM(value, name, display_name, configurable_key, shown_in_settings) value,
    IMGVIEWER_ANIMATION_ACTIONS(IMGVIEWER_ACTION_ENUM)
#undef IMGVIEWER_ACTION_ENUM
    AnimationActionEnd,
#define IMGVIEWER_ACTION_ENUM(value, name, display_name, configurable_key, shown_in_settings) value,
    IMGVIEWER_OTHER_ACTIONS(IMGVIEWER_ACTION_ENUM)
#undef IMGVIEWER_ACTION_ENUM
};

constexpr bool IsImgViewerActionInRange(ImgViewerAction action, ImgViewerAction begin, ImgViewerAction end)
{
    return static_cast<int>(begin) < static_cast<int>(action) &&
        static_cast<int>(action) < static_cast<int>(end);
}

constexpr bool IsImgViewerImageAction(ImgViewerAction action)
{
    return IsImgViewerActionInRange(action, ImgViewerAction::ImageActionBegin, ImgViewerAction::ImageActionEnd);
}

constexpr bool IsImgViewerEditAction(ImgViewerAction action)
{
    return IsImgViewerActionInRange(action, ImgViewerAction::EditActionBegin, ImgViewerAction::EditActionEnd);
}

constexpr bool IsImgViewerSequenceAction(ImgViewerAction action)
{
    return IsImgViewerActionInRange(action, ImgViewerAction::SequenceActionBegin, ImgViewerAction::SequenceActionEnd);
}

constexpr bool IsImgViewerAnimationAction(ImgViewerAction action)
{
    return IsImgViewerActionInRange(action, ImgViewerAction::AnimationActionBegin, ImgViewerAction::AnimationActionEnd);
}

struct ImgViewerActionInfo final {
    ImgViewerAction action = ImgViewerAction::None;
    const char* name = "";
    ImgViewerStringId display_name = ImgViewerStringId::Empty;
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
