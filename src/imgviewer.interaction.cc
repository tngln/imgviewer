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

namespace {

constexpr unsigned ModalBit(ImgViewerModalOwner owner)
{
    return 1u << static_cast<unsigned>(owner);
}

} // namespace

bool ImgViewerInteractionState::IsModal(ImgViewerModalOwner owner) const
{
    return (modal_mask_ & ModalBit(owner)) != 0;
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
    return modal_mask_ != 0;
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
    modal_mask_ |= ModalBit(owner);
    ResetTransientInput();
    if (owner == ImgViewerModalOwner::Popup) {
        keyboard_owner_ = ImgViewerKeyboardOwner::Popup;
    }
}

void ImgViewerInteractionState::ClearModal(ImgViewerModalOwner owner)
{
    modal_mask_ &= ~ModalBit(owner);
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
