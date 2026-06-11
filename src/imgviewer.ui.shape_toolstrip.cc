#include "imgviewer.ui.shape_toolstrip.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "imgviewer.action.hpp"
#include "imgviewer.strings.hpp"
#include "math.hpp"

namespace {

const ToolStripItemSpec kSpecs[] = {
    {ImgViewerAction::EditShapeRectangle, ImgViewerStringId::Rectangle, ImgViewerStringId::RectangleShape, L"edit-shape-rectangle", ToolStripItemVisual::ShapeKind, {}, 0.0f, ImgViewerShapeKind::Rectangle},
    {ImgViewerAction::EditShapeEllipse, ImgViewerStringId::Ellipse, ImgViewerStringId::EllipseShape, L"edit-shape-ellipse", ToolStripItemVisual::ShapeKind, {}, 0.0f, ImgViewerShapeKind::Ellipse},
    {ImgViewerAction::EditShapeLine, ImgViewerStringId::Line, ImgViewerStringId::LineShape, L"edit-shape-line", ToolStripItemVisual::ShapeKind, {}, 0.0f, ImgViewerShapeKind::Line},
    {ImgViewerAction::EditShapeArrow, ImgViewerStringId::Arrow, ImgViewerStringId::ArrowShape, L"edit-shape-arrow", ToolStripItemVisual::ShapeKind, {}, 0.0f, ImgViewerShapeKind::Arrow},
    {ImgViewerAction::EditPenColorRed, ImgViewerStringId::Red, ImgViewerStringId::RedShape, L"edit-shape-red", ToolStripItemVisual::ColorSwatch, D2D1::ColorF(D2D1::ColorF::Red)},
    {ImgViewerAction::EditPenColorYellow, ImgViewerStringId::Yellow, ImgViewerStringId::YellowShape, L"edit-shape-yellow", ToolStripItemVisual::ColorSwatch, D2D1::ColorF(D2D1::ColorF::Yellow)},
    {ImgViewerAction::EditPenColorGreen, ImgViewerStringId::Green, ImgViewerStringId::GreenShape, L"edit-shape-green", ToolStripItemVisual::ColorSwatch, D2D1::ColorF(D2D1::ColorF::Lime)},
    {ImgViewerAction::EditPenColorCyan, ImgViewerStringId::Cyan, ImgViewerStringId::CyanShape, L"edit-shape-cyan", ToolStripItemVisual::ColorSwatch, D2D1::ColorF(D2D1::ColorF::Cyan)},
    {ImgViewerAction::EditPenColorBlue, ImgViewerStringId::Blue, ImgViewerStringId::BlueShape, L"edit-shape-blue", ToolStripItemVisual::ColorSwatch, D2D1::ColorF(D2D1::ColorF::DodgerBlue)},
    {ImgViewerAction::EditPenColorMagenta, ImgViewerStringId::Magenta, ImgViewerStringId::MagentaShape, L"edit-shape-magenta", ToolStripItemVisual::ColorSwatch, D2D1::ColorF(D2D1::ColorF::Magenta)},
    {ImgViewerAction::EditPenColorWhite, ImgViewerStringId::White, ImgViewerStringId::WhiteShape, L"edit-shape-white", ToolStripItemVisual::ColorSwatch, D2D1::ColorF(D2D1::ColorF::White)},
    {ImgViewerAction::EditPenColorBlack, ImgViewerStringId::Black, ImgViewerStringId::BlackShape, L"edit-shape-black", ToolStripItemVisual::ColorSwatch, D2D1::ColorF(D2D1::ColorF::Black)},
};

std::vector<ToolStripItemSpec> BuildSpecs()
{
    return {std::begin(kSpecs), std::end(kSpecs)};
}

} // namespace

ImgViewerUiShapeToolstrip::ImgViewerUiShapeToolstrip(UiElement& root)
{
    toolstrip_ = std::make_unique<ImgViewerUiToolStrip>(
        root, ImgViewerString(ImgViewerStringId::ShapeTools), L"shape-toolstrip", BuildSpecs());
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
