#include "imgviewer.ui.pen_toolstrip.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "imgviewer.action.hpp"
#include "imgviewer.strings.hpp"
#include "math.hpp"

namespace {

const ToolStripItemSpec kSpecs[] = {
    {ImgViewerAction::EditPenColorRed, ImgViewerStringId::Red, ImgViewerStringId::RedPen, L"edit-pen-red", ToolStripItemVisual::ColorSwatch, D2D1::ColorF(D2D1::ColorF::Red)},
    {ImgViewerAction::EditPenColorYellow, ImgViewerStringId::Yellow, ImgViewerStringId::YellowPen, L"edit-pen-yellow", ToolStripItemVisual::ColorSwatch, D2D1::ColorF(D2D1::ColorF::Yellow)},
    {ImgViewerAction::EditPenColorGreen, ImgViewerStringId::Green, ImgViewerStringId::GreenPen, L"edit-pen-green", ToolStripItemVisual::ColorSwatch, D2D1::ColorF(D2D1::ColorF::Lime)},
    {ImgViewerAction::EditPenColorCyan, ImgViewerStringId::Cyan, ImgViewerStringId::CyanPen, L"edit-pen-cyan", ToolStripItemVisual::ColorSwatch, D2D1::ColorF(D2D1::ColorF::Cyan)},
    {ImgViewerAction::EditPenColorBlue, ImgViewerStringId::Blue, ImgViewerStringId::BluePen, L"edit-pen-blue", ToolStripItemVisual::ColorSwatch, D2D1::ColorF(D2D1::ColorF::DodgerBlue)},
    {ImgViewerAction::EditPenColorMagenta, ImgViewerStringId::Magenta, ImgViewerStringId::MagentaPen, L"edit-pen-magenta", ToolStripItemVisual::ColorSwatch, D2D1::ColorF(D2D1::ColorF::Magenta)},
    {ImgViewerAction::EditPenColorWhite, ImgViewerStringId::White, ImgViewerStringId::WhitePen, L"edit-pen-white", ToolStripItemVisual::ColorSwatch, D2D1::ColorF(D2D1::ColorF::White)},
    {ImgViewerAction::EditPenColorBlack, ImgViewerStringId::Black, ImgViewerStringId::BlackPen, L"edit-pen-black", ToolStripItemVisual::ColorSwatch, D2D1::ColorF(D2D1::ColorF::Black)},
    {ImgViewerAction::EditPenWidth2, ImgViewerStringId::PenWidth2, ImgViewerStringId::PenWidth2, L"edit-pen-width-2", ToolStripItemVisual::WidthLine, {}, 2.0f},
    {ImgViewerAction::EditPenWidth4, ImgViewerStringId::PenWidth4, ImgViewerStringId::PenWidth4, L"edit-pen-width-4", ToolStripItemVisual::WidthLine, {}, 4.0f},
    {ImgViewerAction::EditPenWidth8, ImgViewerStringId::PenWidth8, ImgViewerStringId::PenWidth8, L"edit-pen-width-8", ToolStripItemVisual::WidthLine, {}, 8.0f},
    {ImgViewerAction::EditPenWidth12, ImgViewerStringId::PenWidth12, ImgViewerStringId::PenWidth12, L"edit-pen-width-12", ToolStripItemVisual::WidthLine, {}, 12.0f},
};

std::vector<ToolStripItemSpec> BuildSpecs()
{
    return {std::begin(kSpecs), std::end(kSpecs)};
}

} // namespace

ImgViewerUiPenToolstrip::ImgViewerUiPenToolstrip(UiElement& root)
{
    toolstrip_ = std::make_unique<ImgViewerUiToolStrip>(
        root, ImgViewerString(ImgViewerStringId::PenTools), L"pen-toolstrip", BuildSpecs());
    SetScalePercent(125);
    SetState(state_);
}

void ImgViewerUiPenToolstrip::SetScalePercent(int percent)
{
    toolstrip_->SetScalePercent(percent);
}

void ImgViewerUiPenToolstrip::SetState(ImgViewerUiPenToolstripState state)
{
    state_ = state;
    toolstrip_->SetVisible(state.visible);

    const auto& specs = toolstrip_->Specs();
    std::vector<bool> active(specs.size());
    for (size_t i = 0; i < specs.size(); ++i) {
        const ToolStripItemSpec& spec = specs[i];
        active[i] = spec.width > 0.0f
            ? std::abs(state.width - spec.width) < 0.01f
            : math::NearlyEqual(state.color, spec.color);
    }
    toolstrip_->SetActiveStates(active);
}

D2D1_RECT_F ImgViewerUiPenToolstrip::Rect() const
{
    return toolstrip_->Rect();
}

D2D1_SIZE_F ImgViewerUiPenToolstrip::Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const
{
    return toolstrip_->Measure(context, available_size);
}

void ImgViewerUiPenToolstrip::Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F anchor_toolbar_rect)
{
    toolstrip_->Arrange(final_rect, anchor_toolbar_rect);
}

void ImgViewerUiPenToolstrip::Render(const UiDrawContext& draw_context, UiRootState state)
{
    toolstrip_->Render(draw_context, state);
}

UiEventResult ImgViewerUiPenToolstrip::OnPointerEvent(const UiPointerEvent& event)
{
    return toolstrip_->OnPointerEvent(event);
}
