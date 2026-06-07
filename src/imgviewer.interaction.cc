#include "imgviewer.interaction.hpp"

ImgViewerInteractionMode ImgViewerInteractionState::Mode() const
{
    return mode_;
}

ImgViewerCanvasOwner ImgViewerInteractionState::CanvasOwner() const
{
    return canvas_owner_;
}

ImgViewerKeyboardOwner ImgViewerInteractionState::KeyboardOwner() const
{
    return keyboard_owner_;
}

ImgViewerPointerCaptureOwner ImgViewerInteractionState::PointerCapture() const
{
    return pointer_capture_;
}

ImgViewerModalOwner ImgViewerInteractionState::Modal() const
{
    return modal_;
}

bool ImgViewerInteractionState::IsEditing() const
{
    return mode_ == ImgViewerInteractionMode::Editing;
}

bool ImgViewerInteractionState::HasPointerCapture() const
{
    return pointer_capture_ != ImgViewerPointerCaptureOwner::None;
}

bool ImgViewerInteractionState::HasModal() const
{
    return modal_ != ImgViewerModalOwner::None;
}

void ImgViewerInteractionState::EnterViewing()
{
    mode_ = ImgViewerInteractionMode::Viewing;
    if (canvas_owner_ == ImgViewerCanvasOwner::EditTool) {
        canvas_owner_ = ImgViewerCanvasOwner::Viewer;
    }
    if (keyboard_owner_ == ImgViewerKeyboardOwner::EditText) {
        keyboard_owner_ = ImgViewerKeyboardOwner::ViewerShortcut;
    }
    ClearPointerCapture();
}

void ImgViewerInteractionState::EnterEditing()
{
    mode_ = ImgViewerInteractionMode::Editing;
    canvas_owner_ = ImgViewerCanvasOwner::EditTool;
    if (keyboard_owner_ != ImgViewerKeyboardOwner::Popup &&
        keyboard_owner_ != ImgViewerKeyboardOwner::UiFocus) {
        keyboard_owner_ = ImgViewerKeyboardOwner::ViewerShortcut;
    }
    ClearPointerCapture();
}

void ImgViewerInteractionState::SetCanvasOwner(ImgViewerCanvasOwner owner)
{
    canvas_owner_ = owner;
}

void ImgViewerInteractionState::BeginColorPick()
{
    mode_ = ImgViewerInteractionMode::Viewing;
    canvas_owner_ = ImgViewerCanvasOwner::ColorPicker;
    if (keyboard_owner_ == ImgViewerKeyboardOwner::EditText) {
        keyboard_owner_ = ImgViewerKeyboardOwner::ViewerShortcut;
    }
    ClearPointerCapture();
}

void ImgViewerInteractionState::EndColorPick()
{
    if (canvas_owner_ == ImgViewerCanvasOwner::ColorPicker) {
        canvas_owner_ = mode_ == ImgViewerInteractionMode::Editing
            ? ImgViewerCanvasOwner::EditTool
            : ImgViewerCanvasOwner::Viewer;
    }
}

void ImgViewerInteractionState::SetKeyboardOwner(ImgViewerKeyboardOwner owner)
{
    keyboard_owner_ = owner;
}

void ImgViewerInteractionState::BeginPointerCapture(ImgViewerPointerCaptureOwner owner)
{
    pointer_capture_ = owner;
}

void ImgViewerInteractionState::EndPointerCapture(ImgViewerPointerCaptureOwner owner)
{
    if (pointer_capture_ == owner) {
        pointer_capture_ = ImgViewerPointerCaptureOwner::None;
    }
}

void ImgViewerInteractionState::ClearPointerCapture()
{
    pointer_capture_ = ImgViewerPointerCaptureOwner::None;
}

void ImgViewerInteractionState::SetModal(ImgViewerModalOwner owner)
{
    modal_ = owner;
    ResetTransientInput();
    if (owner == ImgViewerModalOwner::Popup) {
        keyboard_owner_ = ImgViewerKeyboardOwner::Popup;
    }
}

void ImgViewerInteractionState::ClearModal(ImgViewerModalOwner owner)
{
    if (modal_ == owner) {
        modal_ = ImgViewerModalOwner::None;
    }
    if (owner == ImgViewerModalOwner::Popup && keyboard_owner_ == ImgViewerKeyboardOwner::Popup) {
        keyboard_owner_ = ImgViewerKeyboardOwner::ViewerShortcut;
    }
}

void ImgViewerInteractionState::ResetTransientInput()
{
    ClearPointerCapture();
    if (keyboard_owner_ == ImgViewerKeyboardOwner::EditText) {
        keyboard_owner_ = ImgViewerKeyboardOwner::ViewerShortcut;
    }
}
