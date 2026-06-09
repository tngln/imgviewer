#include "imgviewer.host.pointer_router.hpp"

bool CanUiReceivePointer(const ImgViewerInteractionState& interaction)
{
    return interaction.PointerCapture() == ImgViewerPointerCaptureOwner::Ui ||
        !interaction.HasPointerCapture();
}

bool IsViewerPointerCapture(ImgViewerPointerCaptureOwner owner)
{
    return owner == ImgViewerPointerCaptureOwner::ViewerPan ||
        owner == ImgViewerPointerCaptureOwner::ViewerRotate;
}

bool IsEditPointerCapture(ImgViewerPointerCaptureOwner owner)
{
    return owner == ImgViewerPointerCaptureOwner::EditStroke ||
        owner == ImgViewerPointerCaptureOwner::EditCrop ||
        owner == ImgViewerPointerCaptureOwner::EditPixelSelection;
}

ImgViewerPointerTarget CapturedPointerTarget(ImgViewerPointerCaptureOwner owner)
{
    if (owner == ImgViewerPointerCaptureOwner::Ui) {
        return ImgViewerPointerTarget::Ui;
    }
    if (owner == ImgViewerPointerCaptureOwner::ColorPicker) {
        return ImgViewerPointerTarget::ColorPicker;
    }
    if (owner == ImgViewerPointerCaptureOwner::EdgeClickNavigation) {
        return ImgViewerPointerTarget::EdgeClickNavigation;
    }
    if (IsEditPointerCapture(owner)) {
        return ImgViewerPointerTarget::EditTool;
    }
    if (IsViewerPointerCapture(owner)) {
        return ImgViewerPointerTarget::Viewer;
    }
    return ImgViewerPointerTarget::None;
}

ImgViewerPointerTarget CanvasPointerTarget(const ImgViewerInteractionState& interaction, bool edit_active)
{
    if (interaction.CanvasOwner() == ImgViewerCanvasOwner::ColorPicker) {
        return ImgViewerPointerTarget::ColorPicker;
    }
    if (interaction.CanvasOwner() == ImgViewerCanvasOwner::EditTool && edit_active) {
        return ImgViewerPointerTarget::EditTool;
    }
    if (interaction.CanvasOwner() == ImgViewerCanvasOwner::Viewer) {
        return ImgViewerPointerTarget::Viewer;
    }
    return ImgViewerPointerTarget::None;
}

ImgViewerPointerTarget ActivePointerTarget(const ImgViewerInteractionState& interaction, bool edit_active)
{
    const ImgViewerPointerTarget captured_target = CapturedPointerTarget(interaction.PointerCapture());
    if (captured_target != ImgViewerPointerTarget::None && captured_target != ImgViewerPointerTarget::Ui) {
        return captured_target;
    }
    return CanvasPointerTarget(interaction, edit_active);
}
