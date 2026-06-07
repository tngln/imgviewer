#include "imgviewer.ui.edit_toolbar.hpp"

#include <algorithm>
#include <memory>

#include <d2d1helper.h>

#include "imgviewer.config.hpp"
#include "imgviewer.ui.action.hpp"
#include "ui.draw.hpp"
#include "ui.layout.hpp"
#include "ui.theme.hpp"

namespace {

constexpr wchar_t kSelectIcon[] = L"\xE8B0";
constexpr wchar_t kPenIcon[] = L"\xED63";
constexpr wchar_t kTextIcon[] = L"\xE8D2";
constexpr wchar_t kCropIcon[] = L"\xE7A8";
constexpr wchar_t kRotateIcon[] = L"\xE7AD";
constexpr wchar_t kUndoIcon[] = L"\xE7A7";
constexpr wchar_t kRedoIcon[] = L"\xE7A6";
constexpr wchar_t kSaveIcon[] = L"\xE74E";
constexpr wchar_t kExitIcon[] = L"\xE711";
constexpr float kToolbarGapAboveAnchor = 6.0f;
constexpr float kDirtyDotSize = 6.0f;

struct EditToolbarMetrics final {
    float button_size = ui_theme::metrics::kToolbarButtonSize;
    float button_gap = ui_theme::metrics::kToolbarButtonGap;
    float padding = ui_theme::metrics::kToolbarPadding;
    float corner_radius = ui_theme::metrics::kToolbarCornerRadius;
};

EditToolbarMetrics MetricsForScale(int percent)
{
    const float scale = static_cast<float>(ClampToolbarScalePercent(percent)) / 100.0f;
    return EditToolbarMetrics{
        .button_size = ui_theme::metrics::kToolbarButtonSize * scale,
        .button_gap = ui_theme::metrics::kToolbarButtonGap * scale,
        .padding = ui_theme::metrics::kToolbarPadding * scale,
        .corner_radius = ui_theme::metrics::kToolbarCornerRadius * scale,
    };
}

struct ButtonSpec final {
    ImgViewerUiEditToolbar::ButtonKey button = ImgViewerUiEditToolbar::ButtonKey::Select;
    ImgViewerAction action = ImgViewerAction::None;
    const wchar_t* name = L"";
    const wchar_t* tooltip = L"";
    const wchar_t* automation_id = L"";
    const wchar_t* icon = L"";
};

constexpr std::array<ButtonSpec, ImgViewerUiEditToolbar::kButtonCount> kButtonSpecs{{
    {ImgViewerUiEditToolbar::ButtonKey::Select, ImgViewerAction::EditSelect, L"Edit Select",
        L"Select edit objects", L"edit-select", kSelectIcon},
    {ImgViewerUiEditToolbar::ButtonKey::Pen, ImgViewerAction::EditPen, L"Edit Pen",
        L"Pen annotation", L"edit-pen", kPenIcon},
    {ImgViewerUiEditToolbar::ButtonKey::Text, ImgViewerAction::EditText, L"Edit Text",
        L"Text annotation", L"edit-text", kTextIcon},
    {ImgViewerUiEditToolbar::ButtonKey::Crop, ImgViewerAction::EditCrop, L"Edit Crop",
        L"Crop image", L"edit-crop", kCropIcon},
    {ImgViewerUiEditToolbar::ButtonKey::RotateClockwise, ImgViewerAction::EditRotateClockwise,
        L"Edit Rotate Clockwise", L"Rotate edit clockwise", L"edit-rotate-clockwise", kRotateIcon},
    {ImgViewerUiEditToolbar::ButtonKey::Undo, ImgViewerAction::EditUndo, L"Undo Edit",
        L"Undo edit", L"edit-undo", kUndoIcon},
    {ImgViewerUiEditToolbar::ButtonKey::Redo, ImgViewerAction::EditRedo, L"Redo Edit",
        L"Redo edit", L"edit-redo", kRedoIcon},
    {ImgViewerUiEditToolbar::ButtonKey::SaveAs, ImgViewerAction::SaveImageAs, L"Save As",
        L"Save edited image as PNG", L"edit-save-as", kSaveIcon},
    {ImgViewerUiEditToolbar::ButtonKey::Exit, ImgViewerAction::ToggleEditMode, L"Exit Edit Mode",
        L"Exit edit mode", L"edit-exit", kExitIcon},
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
    panel_ = static_cast<StackPanel*>(root.AddChild(std::make_unique<StackPanel>(
        UiMetadata(UiElementRole::Pane, kUiActionNone, L"Edit toolbar", L"", L"edit-toolbar", false, false),
        ui_layout::StackDirection::Horizontal)));
    panel_->SetGap(ui_theme::metrics::kToolbarButtonGap);

    for (const ButtonSpec& spec : kButtonSpecs) {
        ButtonInstance& button = buttons_[ButtonIndex(spec.button)];
        auto element = std::make_unique<IconButton>(
            UiMetadata(
                UiElementRole::Button,
                UiActionFromImgViewerAction(spec.action),
                spec.name,
                spec.tooltip,
                spec.automation_id),
            spec.icon);
        button.element = panel_->AddItem(std::move(element), ui_theme::metrics::kToolbarButtonSize);
        button.id = button.element->Id();
    }

    SetScalePercent(scale_percent_);
    SetState(state_);
}

void ImgViewerUiEditToolbar::SetScalePercent(int percent)
{
    scale_percent_ = ClampToolbarScalePercent(percent);
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
    return toolbar_rect_;
}

D2D1_SIZE_F ImgViewerUiEditToolbar::Measure(const UiDrawContext&, D2D1_SIZE_F) const
{
    if (!state_.visible) {
        return D2D1::SizeF();
    }

    const EditToolbarMetrics metrics = MetricsForScale(scale_percent_);
    return D2D1::SizeF(
        metrics.button_size * static_cast<float>(kButtonSpecs.size()) +
            metrics.button_gap * static_cast<float>(kButtonSpecs.size() - 1) +
            metrics.padding * 2.0f,
        metrics.button_size + metrics.padding * 2.0f);
}

void ImgViewerUiEditToolbar::Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F anchor_toolbar_rect)
{
    const EditToolbarMetrics metrics = MetricsForScale(scale_percent_);
    const D2D1_SIZE_F toolbar_size = Measure(UiDrawContext{}, D2D1::SizeF());
    if (!state_.visible) {
        toolbar_rect_ = D2D1::RectF();
        panel_->SetHitTestVisible(false);
        panel_->Arrange(toolbar_rect_);
        return;
    }

    const float viewport_width = final_rect.right - final_rect.left;
    const float left = (std::max)(0.0f, (viewport_width - toolbar_size.width) * 0.5f);
    const float preferred_top = anchor_toolbar_rect.top - toolbar_size.height - kToolbarGapAboveAnchor;
    const float top = (std::max)(ui_theme::metrics::kTitleBarHeight + metrics.padding, preferred_top);
    toolbar_rect_ = D2D1::RectF(left, top, left + toolbar_size.width, top + toolbar_size.height);
    panel_->SetHitTestVisible(true);
    panel_->SetGap(metrics.button_gap);
    panel_->SetPadding(UiThickness{metrics.padding, metrics.padding, metrics.padding, metrics.padding});
    for (size_t index = 0; index < kButtonSpecs.size(); ++index) {
        panel_->SetItemFixedMainSize(index, metrics.button_size);
    }
    panel_->Arrange(toolbar_rect_);
}

void ImgViewerUiEditToolbar::Render(const UiDrawContext& draw_context, UiRootState state)
{
    if (!state_.visible) {
        return;
    }

    const UiDraw draw(draw_context);
    const EditToolbarMetrics metrics = MetricsForScale(scale_percent_);
    const D2D1_ROUNDED_RECT background = D2D1::RoundedRect(toolbar_rect_, metrics.corner_radius, metrics.corner_radius);
    draw.FillRoundedRect(
        background,
        D2D1::ColorF(
            ui_theme::color::kToolbarBackground.r,
            ui_theme::color::kToolbarBackground.g,
            ui_theme::color::kToolbarBackground.b,
            ui_theme::color::kToolbarBackgroundOpacity));
    draw.DrawRoundedRect(
        background,
        state_.dirty ? ui_theme::color::kAccent : ui_theme::color::kBorder,
        state_.dirty ? ui_theme::metrics::kStrokeWidth * 2.0f : ui_theme::metrics::kStrokeWidth);
    panel_->Render(draw_context, state);
    if (state_.dirty) {
        const float dot_size = kDirtyDotSize * static_cast<float>(ClampToolbarScalePercent(scale_percent_)) / 100.0f;
        const D2D1_RECT_F dot = D2D1::RectF(
            toolbar_rect_.right - metrics.padding - dot_size,
            toolbar_rect_.top + metrics.padding,
            toolbar_rect_.right - metrics.padding,
            toolbar_rect_.top + metrics.padding + dot_size);
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
    panel_->SetEnabled(state_.visible);
    for (const ButtonInstance& button : buttons_) {
        if (button.element != nullptr) {
            button.element->SetEnabled(state_.visible);
            button.element->SetVisualActive(false);
        }
    }

    Button(ButtonKey::Select)->SetVisualActive(state_.visible && state_.tool == ImgViewerEditTool::Select);
    Button(ButtonKey::Pen)->SetVisualActive(state_.visible && state_.tool == ImgViewerEditTool::Pen);
    Button(ButtonKey::Text)->SetVisualActive(state_.visible && state_.tool == ImgViewerEditTool::Text);
    Button(ButtonKey::Crop)->SetVisualActive(state_.visible && state_.tool == ImgViewerEditTool::Crop);
    Button(ButtonKey::Undo)->SetEnabled(state_.visible && state_.can_undo);
    Button(ButtonKey::Redo)->SetEnabled(state_.visible && state_.can_redo);
}
