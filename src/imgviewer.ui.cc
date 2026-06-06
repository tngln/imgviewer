#include "imgviewer.ui.hpp"

#include <memory>
#include <vector>

#include <d2d1helper.h>

#include "imgviewer.ui.action.hpp"
#include "ui.theme.hpp"

namespace {

UiElementMetadata Metadata(
    UiElementId id,
    UiElementRole role,
    UiAction action,
    const wchar_t* name,
    const wchar_t* tooltip,
    const wchar_t* automation_id,
    bool is_control = true,
    bool is_content = true)
{
    return UiElementMetadata{
        .id = id,
        .role = role,
        .action = action,
        .name = name,
        .tooltip = tooltip,
        .automation_id = automation_id,
        .is_control = is_control,
        .is_content = is_content,
    };
}

} // namespace

ImgViewerUi::ImgViewerUi() :
    root_(std::make_unique<UiElement>(
        Metadata(UiElementId::None, UiElementRole::Pane, kUiActionNone, L"ImgViewer", L"", L"root"))),
    titlebar_(*root_, ids_),
    toolbar_(*root_, ids_)
{
}

UiElement* ImgViewerUi::Root()
{
    return root_.get();
}

const UiElement* ImgViewerUi::Root() const
{
    return root_.get();
}

const wchar_t* ImgViewerUi::AccessibilityRootName() const
{
    return L"ImgViewer";
}

void ImgViewerUi::Draw(
    const UiDrawContext& draw_context,
    UiRootState state)
{
    Layout(draw_context.viewport_size);
    titlebar_.Draw(draw_context, state, top_most_, maximized_);
    toolbar_.Draw(draw_context, state, color_picker_active_);
    menu_.Draw(draw_context, UiElementState{});
    toast_.Draw(draw_context);
}

UiEventResult ImgViewerUi::OnPointerEvent(const UiPointerEvent& event)
{
    UiEventResult menu_result = menu_.OnPointerEvent(event);
    if (menu_result.handled) {
        return menu_result;
    }
    return toolbar_.OnPointerEvent(event);
}

UiEventResult ImgViewerUi::OnKeyEvent(const UiKeyEvent& event)
{
    return menu_.OnKeyEvent(event);
}

bool ImgViewerUi::HandleUiAction(UiAction action)
{
    if (action != UiActionFromImgViewerAction(ImgViewerAction::OpenMenu)) {
        return false;
    }

    if (menu_.IsOpen()) {
        menu_.Close();
        return true;
    }

    menu_.Open(
        D2D1::Point2F(6.0f, ui_theme::metrics::kTitleBarHeight + 4.0f),
        std::vector<MenuItem>{
            {L"Open Image", UiActionFromImgViewerAction(ImgViewerAction::OpenImage)},
            {L"Save As", UiActionFromImgViewerAction(ImgViewerAction::SaveImageAs), false, false, save_image_as_enabled_},
            {L"Settings", UiActionFromImgViewerAction(ImgViewerAction::OpenSettings)},
            {L"", kUiActionNone, true},
            {L"Zoom In", UiActionFromImgViewerAction(ImgViewerAction::ZoomIn)},
            {L"Zoom Out", UiActionFromImgViewerAction(ImgViewerAction::ZoomOut)},
            {L"Fit Window", UiActionFromImgViewerAction(ImgViewerAction::FitWindow)},
            {L"Actual Size", UiActionFromImgViewerAction(ImgViewerAction::ActualSize)},
            {L"Reset View", UiActionFromImgViewerAction(ImgViewerAction::ResetView)},
            {L"Color Picker", UiActionFromImgViewerAction(ImgViewerAction::ToggleColorPicker), false, color_picker_active_},
            {L"", kUiActionNone, true},
            {L"Rotate Clockwise", UiActionFromImgViewerAction(ImgViewerAction::RotateClockwise)},
            {L"Flip Horizontal", UiActionFromImgViewerAction(ImgViewerAction::FlipHorizontal)},
            {L"Flip Vertical", UiActionFromImgViewerAction(ImgViewerAction::FlipVertical)},
            {L"", kUiActionNone, true},
            {L"Top Most", UiActionFromImgViewerAction(ImgViewerAction::ToggleTopMost), false, top_most_},
            {L"Minimize", UiActionFromImgViewerAction(ImgViewerAction::Minimize)},
            {L"Maximize or Restore", UiActionFromImgViewerAction(ImgViewerAction::ToggleMaximize)},
            {L"Close", UiActionFromImgViewerAction(ImgViewerAction::Close)},
        });
    return true;
}

bool ImgViewerUi::IsPointInCaptionDragArea(D2D1_POINT_2F point) const
{
    return titlebar_.IsPointInCaptionDragArea(*root_, point);
}

void ImgViewerUi::SetTitleText(const wchar_t* title)
{
    titlebar_.SetTitleText(title);
}

void ImgViewerUi::ShowToast(const wchar_t* text)
{
    toast_.Show(text);
}

bool ImgViewerUi::HideToast()
{
    return toast_.Hide();
}

void ImgViewerUi::SetWindowState(bool top_most, bool maximized)
{
    top_most_ = top_most;
    maximized_ = maximized;
}

void ImgViewerUi::SetColorPickerActive(bool active)
{
    color_picker_active_ = active;
}

void ImgViewerUi::SetActionEnabled(UiAction action, bool enabled)
{
    if (action == UiActionFromImgViewerAction(ImgViewerAction::SaveImageAs)) {
        save_image_as_enabled_ = enabled;
    }
}

void ImgViewerUi::Layout(D2D1_SIZE_F viewport_size)
{
    root_->SetRect(D2D1::RectF(0.0f, 0.0f, viewport_size.width, viewport_size.height));
}
