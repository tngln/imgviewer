#include "imgviewer.ui.hpp"

#include <memory>
#include <vector>

#include <d2d1helper.h>

#include "ui.theme.hpp"

namespace {

UiElementMetadata Metadata(
    UiElementId id,
    UiElementRole role,
    ImgViewerAction action,
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
        Metadata(UiElementId::None, UiElementRole::Pane, ImgViewerAction::None, L"ImgViewer", L"", L"root"))),
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

void ImgViewerUi::Draw(
    ID2D1DeviceContext* d2d_context,
    D2D1_SIZE_F viewport_size,
    IDWriteFactory* dwrite_factory,
    IDWriteTextFormat* body_text_format,
    IDWriteTextFormat* icon_text_format,
    ImgViewerUiState state)
{
    const UiDrawContext draw_context{
        .d2d_context = d2d_context,
        .body_text_format = body_text_format,
        .icon_text_format = icon_text_format,
    };
    Layout(viewport_size);
    titlebar_.Draw(draw_context, viewport_size, dwrite_factory, body_text_format, state, top_most_, maximized_);
    toolbar_.Draw(draw_context, viewport_size, state, color_picker_active_);
    menu_.Draw(draw_context, UiElementState{});
    toast_.Draw(draw_context, viewport_size, dwrite_factory, body_text_format);
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

bool ImgViewerUi::HandleUiAction(ImgViewerAction action)
{
    if (action != ImgViewerAction::OpenMenu) {
        return false;
    }

    if (menu_.IsOpen()) {
        menu_.Close();
        return true;
    }

    menu_.Open(
        D2D1::Point2F(6.0f, ui_theme::metrics::kTitleBarHeight + 4.0f),
        std::vector<MenuItem>{
            {L"Open Image", ImgViewerAction::OpenImage},
            {L"Settings", ImgViewerAction::OpenSettings},
            {L"", ImgViewerAction::None, true},
            {L"Zoom In", ImgViewerAction::ZoomIn},
            {L"Zoom Out", ImgViewerAction::ZoomOut},
            {L"Reset View", ImgViewerAction::ResetView},
            {L"Color Picker", ImgViewerAction::ToggleColorPicker, false, color_picker_active_},
            {L"", ImgViewerAction::None, true},
            {L"Rotate Clockwise", ImgViewerAction::RotateClockwise},
            {L"Flip Horizontal", ImgViewerAction::FlipHorizontal},
            {L"Flip Vertical", ImgViewerAction::FlipVertical},
            {L"", ImgViewerAction::None, true},
            {L"Top Most", ImgViewerAction::ToggleTopMost, false, top_most_},
            {L"Minimize", ImgViewerAction::Minimize},
            {L"Maximize or Restore", ImgViewerAction::ToggleMaximize},
            {L"Close", ImgViewerAction::Close},
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

void ImgViewerUi::Layout(D2D1_SIZE_F viewport_size)
{
    root_->SetRect(D2D1::RectF(0.0f, 0.0f, viewport_size.width, viewport_size.height));
}
