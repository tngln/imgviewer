#pragma once

#include "imgviewer.interaction.hpp"

enum class ImgViewerPointerTarget {
    None,
    Ui,
    Viewer,
    EditTool,
    ColorPicker,
};

bool CanUiReceivePointer(const ImgViewerInteractionState& interaction);
bool IsViewerPointerCapture(ImgViewerPointerCaptureOwner owner);
bool IsEditPointerCapture(ImgViewerPointerCaptureOwner owner);

ImgViewerPointerTarget CapturedPointerTarget(ImgViewerPointerCaptureOwner owner);
ImgViewerPointerTarget CanvasPointerTarget(const ImgViewerInteractionState& interaction, bool edit_active);
ImgViewerPointerTarget ActivePointerTarget(const ImgViewerInteractionState& interaction, bool edit_active);
