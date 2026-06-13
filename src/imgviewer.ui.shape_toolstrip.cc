#include "imgviewer.ui.shape_toolstrip.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "imgviewer.action.hpp"
#include "imgviewer.palette.hpp"
#include "imgviewer.strings.hpp"
#include "math.hpp"

namespace {

const ToolStripItemSpec kSpecs[] = {
    {ImgViewerAction::EditSetShapeKind, ImgViewerStringId::Rectangle, ImgViewerStringId::RectangleShape, ToolStripItemVisual::ShapeKind, static_cast<int32_t>(ImgViewerShapeKind::Rectangle), {}, 0.0f, ImgViewerShapeKind::Rectangle},
    {ImgViewerAction::EditSetShapeKind, ImgViewerStringId::Ellipse, ImgViewerStringId::EllipseShape, ToolStripItemVisual::ShapeKind, static_cast<int32_t>(ImgViewerShapeKind::Ellipse), {}, 0.0f, ImgViewerShapeKind::Ellipse},
    {ImgViewerAction::EditSetShapeKind, ImgViewerStringId::Line, ImgViewerStringId::LineShape, ToolStripItemVisual::ShapeKind, static_cast<int32_t>(ImgViewerShapeKind::Line), {}, 0.0f, ImgViewerShapeKind::Line},
    {ImgViewerAction::EditSetShapeKind, ImgViewerStringId::Arrow, ImgViewerStringId::ArrowShape, ToolStripItemVisual::ShapeKind, static_cast<int32_t>(ImgViewerShapeKind::Arrow), {}, 0.0f, ImgViewerShapeKind::Arrow},
    {ImgViewerAction::EditSetPenColor, ImgViewerStringId::Red, ImgViewerStringId::RedShape, ToolStripItemVisual::ColorSwatch, PackColor(D2D1::ColorF(D2D1::ColorF::Red)), D2D1::ColorF(D2D1::ColorF::Red)},
    {ImgViewerAction::EditSetPenColor, ImgViewerStringId::Yellow, ImgViewerStringId::YellowShape, ToolStripItemVisual::ColorSwatch, PackColor(D2D1::ColorF(D2D1::ColorF::Yellow)), D2D1::ColorF(D2D1::ColorF::Yellow)},
    {ImgViewerAction::EditSetPenColor, ImgViewerStringId::Green, ImgViewerStringId::GreenShape, ToolStripItemVisual::ColorSwatch, PackColor(D2D1::ColorF(D2D1::ColorF::Lime)), D2D1::ColorF(D2D1::ColorF::Lime)},
    {ImgViewerAction::EditSetPenColor, ImgViewerStringId::Cyan, ImgViewerStringId::CyanShape, ToolStripItemVisual::ColorSwatch, PackColor(D2D1::ColorF(D2D1::ColorF::Cyan)), D2D1::ColorF(D2D1::ColorF::Cyan)},
    {ImgViewerAction::EditSetPenColor, ImgViewerStringId::Blue, ImgViewerStringId::BlueShape, ToolStripItemVisual::ColorSwatch, PackColor(D2D1::ColorF(D2D1::ColorF::DodgerBlue)), D2D1::ColorF(D2D1::ColorF::DodgerBlue)},
    {ImgViewerAction::EditSetPenColor, ImgViewerStringId::Magenta, ImgViewerStringId::MagentaShape, ToolStripItemVisual::ColorSwatch, PackColor(D2D1::ColorF(D2D1::ColorF::Magenta)), D2D1::ColorF(D2D1::ColorF::Magenta)},
    {ImgViewerAction::EditSetPenColor, ImgViewerStringId::White, ImgViewerStringId::WhiteShape, ToolStripItemVisual::ColorSwatch, PackColor(D2D1::ColorF(D2D1::ColorF::White)), D2D1::ColorF(D2D1::ColorF::White)},
    {ImgViewerAction::EditSetPenColor, ImgViewerStringId::Black, ImgViewerStringId::BlackShape, ToolStripItemVisual::ColorSwatch, PackColor(D2D1::ColorF(D2D1::ColorF::Black)), D2D1::ColorF(D2D1::ColorF::Black)},
};

std::vector<ToolStripItemSpec> BuildSpecs()
{
    return {std::begin(kSpecs), std::end(kSpecs)};
}

} // namespace

ImgViewerUiShapeToolstrip::ImgViewerUiShapeToolstrip(UiElement& root)
{
    toolstrip_ = std::make_unique<ImgViewerUiToolStrip>(
        root, ImgViewerString(ImgViewerStringId::ShapeTools), BuildSpecs());
    SetScalePercent(125);
    SetState(state_);
}

void ImgViewerUiShapeToolstrip::SetScalePercent(int percent)
{
    toolstrip_->SetScalePercent(percent);
}

void ImgViewerUiShapeToolstrip::SetState(ImgViewerUiShapeToolstripState state)
{
    state_ = state;
    toolstrip_->SetVisible(state.visible);

    const auto& specs = toolstrip_->Specs();
    std::vector<bool> active(specs.size());
    for (size_t i = 0; i < specs.size(); ++i) {
        const ToolStripItemSpec& spec = specs[i];
        active[i] = spec.visual == ToolStripItemVisual::ColorSwatch
            ? math::NearlyEqual(state.color, spec.color)
            : state.kind == spec.shape_kind;
    }
    toolstrip_->SetActiveStates(active);
}

D2D1_RECT_F ImgViewerUiShapeToolstrip::Rect() const
{
    return toolstrip_->Rect();
}

D2D1_SIZE_F ImgViewerUiShapeToolstrip::Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const
{
    return toolstrip_->Measure(context, available_size);
}

void ImgViewerUiShapeToolstrip::Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F anchor_toolbar_rect)
{
    toolstrip_->Arrange(final_rect, anchor_toolbar_rect);
}

void ImgViewerUiShapeToolstrip::Render(const UiDrawContext& draw_context, UiRootState state)
{
    toolstrip_->Render(draw_context, state);
}

UiEventResult ImgViewerUiShapeToolstrip::OnPointerEvent(const UiPointerEvent& event)
{
    return toolstrip_->OnPointerEvent(event);
}
