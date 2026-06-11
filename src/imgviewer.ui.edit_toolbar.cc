#include "imgviewer.ui.edit_toolbar.hpp"

#include <memory>
#include <vector>

#include <d2d1helper.h>

#include "imgviewer.config.hpp"
#include "imgviewer.strings.hpp"
#include "imgviewer.ui.action.hpp"
#include "ui.button.hpp"
#include "ui.draw.hpp"
#include "ui.theme.hpp"

namespace {

constexpr wchar_t kSelectIcon[] = L"\xE8B0";
constexpr wchar_t kPenIcon[] = L"\xED63";
constexpr wchar_t kShapeIcon[] = L"\xF0E7";
constexpr wchar_t kTextIcon[] = L"\xE8D2";
constexpr wchar_t kCropIcon[] = L"\xE7A8";
constexpr wchar_t kRotateIcon[] = L"\xE7AD";
constexpr wchar_t kUndoIcon[] = L"\xE7A7";
constexpr wchar_t kRedoIcon[] = L"\xE7A6";
constexpr wchar_t kSaveIcon[] = L"\xE74E";
constexpr wchar_t kExitIcon[] = L"\xE711";
constexpr float kDirtyDotSize = 6.0f;

const ToolStripItemSpec kSpecs[] = {
    {ImgViewerAction::EditSelect, ImgViewerStringId::EditSelect, ImgViewerStringId::SelectEditObjects, ToolStripItemVisual::Icon, 0, {}, 0.0f, ImgViewerShapeKind::Rectangle, L"", kSelectIcon},
    {ImgViewerAction::EditPixelSelect, ImgViewerStringId::PixelSelect, ImgViewerStringId::SelectPixels, ToolStripItemVisual::PathIcon, 0, {}, 0.0f, ImgViewerShapeKind::Rectangle, L"", L"", &icons::kRegionScreenshotIcon},
    {ImgViewerAction::EditPen, ImgViewerStringId::EditPen, ImgViewerStringId::PenAnnotation, ToolStripItemVisual::Icon, 0, {}, 0.0f, ImgViewerShapeKind::Rectangle, L"", kPenIcon},
    {ImgViewerAction::EditShape, ImgViewerStringId::EditShape, ImgViewerStringId::ShapeAnnotation, ToolStripItemVisual::Icon, 0, {}, 0.0f, ImgViewerShapeKind::Rectangle, L"", kShapeIcon},
    {ImgViewerAction::EditText, ImgViewerStringId::EditText, ImgViewerStringId::TextAnnotation, ToolStripItemVisual::Icon, 0, {}, 0.0f, ImgViewerShapeKind::Rectangle, L"", kTextIcon},
    {ImgViewerAction::EditCrop, ImgViewerStringId::EditCrop, ImgViewerStringId::CropImage, ToolStripItemVisual::Icon, 0, {}, 0.0f, ImgViewerShapeKind::Rectangle, L"", kCropIcon},
    {ImgViewerAction::EditRotateClockwise, ImgViewerStringId::EditRotateClockwise, ImgViewerStringId::RotateEditClockwise, ToolStripItemVisual::Icon, 0, {}, 0.0f, ImgViewerShapeKind::Rectangle, L"", kRotateIcon},
    {ImgViewerAction::EditUndo, ImgViewerStringId::UndoEdit, ImgViewerStringId::UndoEdit, ToolStripItemVisual::Icon, 0, {}, 0.0f, ImgViewerShapeKind::Rectangle, L"", kUndoIcon},
    {ImgViewerAction::EditRedo, ImgViewerStringId::RedoEdit, ImgViewerStringId::RedoEdit, ToolStripItemVisual::Icon, 0, {}, 0.0f, ImgViewerShapeKind::Rectangle, L"", kRedoIcon},
    {ImgViewerAction::SaveImageAs, ImgViewerStringId::SaveAs, ImgViewerStringId::SaveEditedImageAsPng, ToolStripItemVisual::Icon, 0, {}, 0.0f, ImgViewerShapeKind::Rectangle, L"", kSaveIcon},
    {ImgViewerAction::ToggleEditMode, ImgViewerStringId::ExitEditMode, ImgViewerStringId::ExitEditMode, ToolStripItemVisual::Icon, 0, {}, 0.0f, ImgViewerShapeKind::Rectangle, L"", kExitIcon},
};

std::vector<ToolStripItemSpec> BuildSpecs()
{
    return {std::begin(kSpecs), std::end(kSpecs)};
}

} // namespace

ImgViewerUiEditToolbar::ImgViewerUiEditToolbar(UiElement& root)
{
    toolstrip_ = std::make_unique<ImgViewerUiToolStrip>(
        root, ImgViewerString(ImgViewerStringId::EditToolbar), BuildSpecs());
    SetScalePercent(125);
    SetState(state_);
}

void ImgViewerUiEditToolbar::SetScalePercent(int percent)
{
    toolstrip_->SetScalePercent(percent);
}

void ImgViewerUiEditToolbar::SetState(ImgViewerUiEditToolbarState state)
{
    state_ = state;
    toolstrip_->SetVisible(state.visible);

    for (size_t i = 0; i < 11; ++i) {
        if (auto* button = toolstrip_->Button(i)) {
            button->SetEnabled(state.visible);
            button->SetVisualActive(false);
        }
    }

    toolstrip_->SetBorderColor(state.dirty ? ui_theme::color::kAccent : ui_theme::color::kBorder);
    toolstrip_->SetBorderStrokeWidth(state.dirty ? ui_theme::metrics::kStrokeWidth * 2.0f : ui_theme::metrics::kStrokeWidth);

    static constexpr size_t kToolToButton[] = {kSelect, kPixelSelect, kPen, kShape, kText, kCrop};
    const size_t tool_index = static_cast<size_t>(state.tool);
    if (tool_index < std::size(kToolToButton)) {
        if (auto* button = toolstrip_->Button(kToolToButton[tool_index])) {
            button->SetVisualActive(state.visible);
        }
    }

    if (auto* button = toolstrip_->Button(kUndo)) {
        button->SetEnabled(state.visible && state.can_undo);
    }
    if (auto* button = toolstrip_->Button(kRedo)) {
        button->SetEnabled(state.visible && state.can_redo);
    }
}

D2D1_RECT_F ImgViewerUiEditToolbar::Rect() const
{
    return toolstrip_->Rect();
}

D2D1_SIZE_F ImgViewerUiEditToolbar::Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const
{
    return toolstrip_->Measure(context, available_size);
}

void ImgViewerUiEditToolbar::Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F anchor_toolbar_rect)
{
    toolstrip_->Arrange(final_rect, anchor_toolbar_rect);
}

void ImgViewerUiEditToolbar::Render(const UiDrawContext& draw_context, UiRootState state)
{
    toolstrip_->Render(draw_context, state);
    if (state_.dirty) {
        const UiDraw draw(draw_context);
        const D2D1_RECT_F rect = toolstrip_->Rect();
        const float dot_size = toolstrip_->ScaledValue(kDirtyDotSize);
        const float pad = toolstrip_->ScaledValue(4.0f);
        const D2D1_RECT_F dot = D2D1::RectF(
            rect.right - pad - dot_size,
            rect.top + pad,
            rect.right - pad,
            rect.top + pad + dot_size);
        draw.FillRoundedRect(D2D1::RoundedRect(dot, dot_size * 0.5f, dot_size * 0.5f), ui_theme::color::kAccent);
    }
}

UiEventResult ImgViewerUiEditToolbar::OnPointerEvent(const UiPointerEvent& event)
{
    return toolstrip_->OnPointerEvent(event);
}
