#include "imgviewer.ui.edit_toolbar.hpp"

#include <memory>

#include <d2d1helper.h>

#include "imgviewer.config.hpp"
#include "imgviewer.strings.hpp"
#include "imgviewer.ui.action.hpp"
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
constexpr float kToolbarGapAboveAnchor = 6.0f;
constexpr float kDirtyDotSize = 6.0f;

struct ButtonSpec final {
    ImgViewerUiEditToolbar::ButtonKey button = ImgViewerUiEditToolbar::ButtonKey::Select;
    ImgViewerAction action = ImgViewerAction::None;
    ImgViewerStringId name = ImgViewerStringId::Empty;
    ImgViewerStringId tooltip = ImgViewerStringId::Empty;
    const wchar_t* automation_id = L"";
    const wchar_t* icon = L"";
    const icons::PathIcon* path_icon = nullptr;
};

constexpr std::array<ButtonSpec, ImgViewerUiEditToolbar::kButtonCount> kButtonSpecs{{
    {ImgViewerUiEditToolbar::ButtonKey::Select, ImgViewerAction::EditSelect, ImgViewerStringId::EditSelect,
        ImgViewerStringId::SelectEditObjects, L"edit-select", kSelectIcon},
    {ImgViewerUiEditToolbar::ButtonKey::PixelSelect, ImgViewerAction::EditPixelSelect, ImgViewerStringId::PixelSelect,
        ImgViewerStringId::SelectPixels, L"edit-pixel-select", L"", &icons::kRegionScreenshotIcon},
    {ImgViewerUiEditToolbar::ButtonKey::Pen, ImgViewerAction::EditPen, ImgViewerStringId::EditPen,
        ImgViewerStringId::PenAnnotation, L"edit-pen", kPenIcon},
    {ImgViewerUiEditToolbar::ButtonKey::Shape, ImgViewerAction::EditShape, ImgViewerStringId::EditShape,
        ImgViewerStringId::ShapeAnnotation, L"edit-shape", kShapeIcon},
    {ImgViewerUiEditToolbar::ButtonKey::Text, ImgViewerAction::EditText, ImgViewerStringId::EditText,
        ImgViewerStringId::TextAnnotation, L"edit-text", kTextIcon},
    {ImgViewerUiEditToolbar::ButtonKey::Crop, ImgViewerAction::EditCrop, ImgViewerStringId::EditCrop,
        ImgViewerStringId::CropImage, L"edit-crop", kCropIcon},
    {ImgViewerUiEditToolbar::ButtonKey::RotateClockwise, ImgViewerAction::EditRotateClockwise,
        ImgViewerStringId::EditRotateClockwise, ImgViewerStringId::RotateEditClockwise, L"edit-rotate-clockwise", kRotateIcon},
    {ImgViewerUiEditToolbar::ButtonKey::Undo, ImgViewerAction::EditUndo, ImgViewerStringId::UndoEdit,
        ImgViewerStringId::UndoEdit, L"edit-undo", kUndoIcon},
    {ImgViewerUiEditToolbar::ButtonKey::Redo, ImgViewerAction::EditRedo, ImgViewerStringId::RedoEdit,
        ImgViewerStringId::RedoEdit, L"edit-redo", kRedoIcon},
    {ImgViewerUiEditToolbar::ButtonKey::SaveAs, ImgViewerAction::SaveImageAs, ImgViewerStringId::SaveAs,
        ImgViewerStringId::SaveEditedImageAsPng, L"edit-save-as", kSaveIcon},
    {ImgViewerUiEditToolbar::ButtonKey::Exit, ImgViewerAction::ToggleEditMode, ImgViewerStringId::ExitEditMode,
        ImgViewerStringId::ExitEditMode, L"edit-exit", kExitIcon},
}};

constexpr bool ButtonSpecsMatchKeys()
{
    for (size_t index = 0; index < kButtonSpecs.size(); ++index) {
        if (static_cast<size_t>(kButtonSpecs[index].button) != index) {
            return false;
        }
    }
    return true;
}

static_assert(ButtonSpecsMatchKeys());

} // namespace

constexpr size_t ImgViewerUiEditToolbar::ButtonIndex(ButtonKey button)
{
    return static_cast<size_t>(button);
}

ImgViewerUiEditToolbar::ImgViewerUiEditToolbar(UiElement& root)
{
    toolbar_ = std::make_unique<ImgViewerFloatingToolbar>(root, ImgViewerString(ImgViewerStringId::EditToolbar), L"edit-toolbar");

    for (const ButtonSpec& spec : kButtonSpecs) {
        ButtonInstance& button = buttons_[ButtonIndex(spec.button)];
        UiElementMetadata metadata = UiMetadata(
            UiElementRole::Button,
            UiActionFromImgViewerAction(spec.action),
            ImgViewerString(spec.name),
            ImgViewerString(spec.tooltip),
            spec.automation_id);
        auto element = spec.path_icon != nullptr
            ? std::make_unique<IconButton>(metadata, *spec.path_icon)
            : std::make_unique<IconButton>(metadata, spec.icon);
        button.element = toolbar_->Panel()->AddItem(std::move(element), ui_theme::metrics::kToolbarButtonSize);
        button.id = button.element->Id();
    }

    SetScalePercent(scale_percent_);
    SetState(state_);
}

void ImgViewerUiEditToolbar::SetScalePercent(int percent)
{
    scale_percent_ = ClampToolbarScalePercent(percent);
    toolbar_->SetScalePercent(scale_percent_);
    const float icon_scale = static_cast<float>(scale_percent_) / 100.0f;
    for (const ButtonInstance& button : buttons_) {
        if (button.element != nullptr) {
            button.element->SetIconScale(icon_scale);
        }
    }
}

void ImgViewerUiEditToolbar::SetState(ImgViewerUiEditToolbarState state)
{
    state_ = state;
    UpdateVisualState();
}

D2D1_RECT_F ImgViewerUiEditToolbar::Rect() const
{
    return toolbar_->Rect();
}

D2D1_SIZE_F ImgViewerUiEditToolbar::Measure(const UiDrawContext&, D2D1_SIZE_F) const
{
    if (!state_.visible) {
        return D2D1::SizeF();
    }

    return toolbar_->Measure(kButtonSpecs.size());
}

void ImgViewerUiEditToolbar::Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F anchor_toolbar_rect)
{
    if (!state_.visible) {
        toolbar_->ArrangeHidden();
        return;
    }

    toolbar_->ArrangeAboveAnchor(final_rect, anchor_toolbar_rect, kButtonSpecs.size(), 0.0f, 0, kToolbarGapAboveAnchor);
}

void ImgViewerUiEditToolbar::Render(const UiDrawContext& draw_context, UiRootState state)
{
    if (!state_.visible) {
        return;
    }

    toolbar_->RenderBackground(
        draw_context,
        state_.dirty ? ui_theme::color::kAccent : ui_theme::color::kBorder,
        state_.dirty ? ui_theme::metrics::kStrokeWidth * 2.0f : ui_theme::metrics::kStrokeWidth);
    toolbar_->Panel()->Render(draw_context, state);
    if (state_.dirty) {
        const UiDraw draw(draw_context);
        const ImgViewerFloatingToolbarMetrics metrics = toolbar_->Metrics();
        const D2D1_RECT_F rect = toolbar_->Rect();
        const float dot_size = toolbar_->ScaledValue(kDirtyDotSize);
        const D2D1_RECT_F dot = D2D1::RectF(
            rect.right - metrics.padding - dot_size,
            rect.top + metrics.padding,
            rect.right - metrics.padding,
            rect.top + metrics.padding + dot_size);
        draw.FillRoundedRect(D2D1::RoundedRect(dot, dot_size * 0.5f, dot_size * 0.5f), ui_theme::color::kAccent);
    }
}

UiEventResult ImgViewerUiEditToolbar::OnPointerEvent(const UiPointerEvent&)
{
    return {};
}

IconButton* ImgViewerUiEditToolbar::Button(ButtonKey button)
{
    return buttons_[ButtonIndex(button)].element;
}

void ImgViewerUiEditToolbar::UpdateVisualState()
{
    toolbar_->Panel()->SetEnabled(state_.visible);
    for (const ButtonInstance& button : buttons_) {
        if (button.element != nullptr) {
            button.element->SetEnabled(state_.visible);
            button.element->SetVisualActive(false);
        }
    }

    Button(ButtonKey::Select)->SetVisualActive(state_.visible && state_.tool == ImgViewerEditTool::Select);
    Button(ButtonKey::PixelSelect)->SetVisualActive(state_.visible && state_.tool == ImgViewerEditTool::PixelSelect);
    Button(ButtonKey::Pen)->SetVisualActive(state_.visible && state_.tool == ImgViewerEditTool::Pen);
    Button(ButtonKey::Shape)->SetVisualActive(state_.visible && state_.tool == ImgViewerEditTool::Shape);
    Button(ButtonKey::Text)->SetVisualActive(state_.visible && state_.tool == ImgViewerEditTool::Text);
    Button(ButtonKey::Crop)->SetVisualActive(state_.visible && state_.tool == ImgViewerEditTool::Crop);
    Button(ButtonKey::Undo)->SetEnabled(state_.visible && state_.can_undo);
    Button(ButtonKey::Redo)->SetEnabled(state_.visible && state_.can_redo);
}
