#include "imgviewer.ui.hpp"

#include <memory>
#include <utility>
#include <vector>

#include <d2d1helper.h>

#include "imgviewer.ui.action.hpp"
#include "ui.popup.hpp"
#include "ui.theme.hpp"

namespace {

constexpr D2D1_POINT_2F kMainMenuOrigin{3.0f, ui_theme::metrics::kTitleBarHeight + 2.0f};

} // namespace

ImgViewerUi::ImgViewerUi() :
    root_(std::make_unique<UiElement>(
        UiRootMetadata(UiElementRole::Pane, kUiActionNone, L"ImgViewer", L"", L"root"))),
    titlebar_(*root_),
    toolbar_(*root_),
    info_panel_(*root_)
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

D2D1_SIZE_F ImgViewerUi::Measure(const UiDrawContext& context, D2D1_SIZE_F available_size)
{
    titlebar_.Measure(context, available_size);
    toolbar_.Measure(context, available_size);
    info_panel_.Measure(context, available_size);
    return available_size;
}

void ImgViewerUi::Arrange(D2D1_RECT_F final_rect)
{
    root_->Arrange(final_rect);
    titlebar_.Arrange(D2D1::RectF(final_rect.left, final_rect.top, final_rect.right, final_rect.top + ui_theme::metrics::kTitleBarHeight));
    toolbar_.Arrange(final_rect);
    info_panel_.Arrange(final_rect);
}

void ImgViewerUi::Render(
    const UiDrawContext& draw_context,
    UiRootState state)
{
    titlebar_.Render(draw_context, state, top_most_, maximized_);
    toolbar_.Render(draw_context, state, color_picker_active_);
    info_panel_.Render(draw_context);
    toast_.Render(draw_context);
}

UiEventResult ImgViewerUi::OnPointerEvent(const UiPointerEvent& event)
{
    UiEventResult info_panel_result = info_panel_.OnPointerEvent(event);
    if (info_panel_result.handled) {
        return info_panel_result;
    }
    return toolbar_.OnPointerEvent(event);
}

UiEventResult ImgViewerUi::OnKeyEvent(const UiKeyEvent& event)
{
    UNREFERENCED_PARAMETER(event);
    return {};
}

bool ImgViewerUi::HandleUiAction(UiAction action, PopupHost* popup_host)
{
    if (action != UiActionFromImgViewerAction(ImgViewerAction::OpenMenu)) {
        return false;
    }
    if (popup_host == nullptr) {
        return false;
    }

    return SUCCEEDED(popup_host->OpenMenu(
        kMainMenuOrigin,
        std::vector<MenuItem>{
            {L"Open Image", UiActionFromImgViewerAction(ImgViewerAction::OpenImage)},
            {L"Desktop Screenshot", UiActionFromImgViewerAction(ImgViewerAction::CaptureDesktop)},
            {L"Region Screenshot", UiActionFromImgViewerAction(ImgViewerAction::CaptureRegion)},
            {L"Save As", UiActionFromImgViewerAction(ImgViewerAction::SaveImageAs), false, false, save_image_as_enabled_},
            {L"Show in File Explorer", UiActionFromImgViewerAction(ImgViewerAction::ShowInFileExplorer), false, false, show_in_file_explorer_enabled_},
            {L"", kUiActionNone, true},
            {L"Settings", UiActionFromImgViewerAction(ImgViewerAction::OpenSettings)},
            {L"About", UiActionFromImgViewerAction(ImgViewerAction::OpenAbout)},
            {L"", kUiActionNone, true},
            {L"Info Panel", UiActionFromImgViewerAction(ImgViewerAction::ToggleInfoPanel), false, info_panel_.IsVisible()},
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
        }));
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

void ImgViewerUi::SetToolbarScalePercent(int percent)
{
    toolbar_.SetScalePercent(percent);
}

void ImgViewerUi::SetActionEnabled(UiAction action, bool enabled)
{
    if (action == UiActionFromImgViewerAction(ImgViewerAction::SaveImageAs)) {
        save_image_as_enabled_ = enabled;
    }
    if (action == UiActionFromImgViewerAction(ImgViewerAction::ShowInFileExplorer)) {
        show_in_file_explorer_enabled_ = enabled;
    }
}

void ImgViewerUi::SetInfoPanelState(ImgViewerUiInfoPanelState state)
{
    info_panel_.SetState(std::move(state));
}
