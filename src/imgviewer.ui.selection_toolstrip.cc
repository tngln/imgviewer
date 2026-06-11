#include "imgviewer.ui.selection_toolstrip.hpp"

#include <memory>
#include <vector>

#include "imgviewer.action.hpp"
#include "imgviewer.strings.hpp"
#include "ui.theme.hpp"

namespace {

constexpr wchar_t kCopyIcon[] = L"\xE8C8";
constexpr wchar_t kMosaicIcon[] = L"\xE9F5";

const ToolStripItemSpec kSpecs[] = {
    {ImgViewerAction::EditCopySelection, ImgViewerStringId::CopySelection, ImgViewerStringId::CopySelectedPixels, L"edit-copy-selection", ToolStripItemVisual::Icon, 0, {}, 0.0f, ImgViewerShapeKind::Rectangle, L"", kCopyIcon},
    {ImgViewerAction::EditMosaicSelection, ImgViewerStringId::MosaicSelection, ImgViewerStringId::MosaicSelectedPixels, L"edit-mosaic-selection", ToolStripItemVisual::Icon, 0, {}, 0.0f, ImgViewerShapeKind::Rectangle, L"", kMosaicIcon},
};

std::vector<ToolStripItemSpec> BuildSpecs()
{
    return {std::begin(kSpecs), std::end(kSpecs)};
}

} // namespace

ImgViewerUiSelectionToolstrip::ImgViewerUiSelectionToolstrip(UiElement& root)
{
    toolstrip_ = std::make_unique<ImgViewerUiToolStrip>(
        root, ImgViewerString(ImgViewerStringId::PixelSelectionTools), L"pixel-selection-toolstrip", BuildSpecs());
    toolstrip_->SetBorderColor(ui_theme::color::kAccent);
    SetScalePercent(125);
    SetState(state_);
}

void ImgViewerUiSelectionToolstrip::SetScalePercent(int percent)
{
    toolstrip_->SetScalePercent(percent);
}

void ImgViewerUiSelectionToolstrip::SetState(ImgViewerUiSelectionToolstripState state)
{
    state_ = state;
    toolstrip_->SetVisible(state.visible);
}

D2D1_RECT_F ImgViewerUiSelectionToolstrip::Rect() const
{
    return toolstrip_->Rect();
}

D2D1_SIZE_F ImgViewerUiSelectionToolstrip::Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const
{
    return toolstrip_->Measure(context, available_size);
}

void ImgViewerUiSelectionToolstrip::Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F anchor_toolbar_rect)
{
    toolstrip_->Arrange(final_rect, anchor_toolbar_rect);
}

void ImgViewerUiSelectionToolstrip::Render(const UiDrawContext& draw_context, UiRootState state)
{
    toolstrip_->Render(draw_context, state);
}

UiEventResult ImgViewerUiSelectionToolstrip::OnPointerEvent(const UiPointerEvent& event)
{
    return toolstrip_->OnPointerEvent(event);
}
