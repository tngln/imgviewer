#include "imgviewer.ui.selection_toolstrip.hpp"

#include <memory>

#include <d2d1helper.h>

#include "imgviewer.config.hpp"
#include "imgviewer.strings.hpp"
#include "imgviewer.ui.action.hpp"
#include "ui.theme.hpp"

namespace {

constexpr wchar_t kCopyIcon[] = L"\xE8C8";
constexpr wchar_t kMosaicIcon[] = L"\xE9F5";
constexpr float kToolbarGapAboveAnchor = 6.0f;

struct ButtonSpec final {
    ImgViewerUiSelectionToolstrip::ButtonKey button = ImgViewerUiSelectionToolstrip::ButtonKey::Copy;
    ImgViewerAction action = ImgViewerAction::None;
    ImgViewerStringId name = ImgViewerStringId::Empty;
    ImgViewerStringId tooltip = ImgViewerStringId::Empty;
    const wchar_t* automation_id = L"";
    const wchar_t* icon = L"";
};

constexpr std::array<ButtonSpec, ImgViewerUiSelectionToolstrip::kButtonCount> kButtonSpecs{{
    {ImgViewerUiSelectionToolstrip::ButtonKey::Copy, ImgViewerAction::EditCopySelection,
        ImgViewerStringId::CopySelection, ImgViewerStringId::CopySelectedPixels, L"edit-copy-selection", kCopyIcon},
    {ImgViewerUiSelectionToolstrip::ButtonKey::Mosaic, ImgViewerAction::EditMosaicSelection,
        ImgViewerStringId::MosaicSelection, ImgViewerStringId::MosaicSelectedPixels, L"edit-mosaic-selection", kMosaicIcon},
}};

} // namespace

constexpr size_t ImgViewerUiSelectionToolstrip::ButtonIndex(ButtonKey button)
{
    return static_cast<size_t>(button);
}

ImgViewerUiSelectionToolstrip::ImgViewerUiSelectionToolstrip(UiElement& root)
{
    toolbar_ = std::make_unique<ImgViewerFloatingToolbar>(root, ImgViewerString(ImgViewerStringId::PixelSelectionTools), L"pixel-selection-toolstrip");

    for (const ButtonSpec& spec : kButtonSpecs) {
        ButtonInstance& button = buttons_[ButtonIndex(spec.button)];
        auto element = std::make_unique<IconButton>(
            UiMetadata(
                UiElementRole::Button,
                UiActionFromImgViewerAction(spec.action),
                ImgViewerString(spec.name),
                ImgViewerString(spec.tooltip),
                spec.automation_id),
            spec.icon);
        button.element = toolbar_->Panel()->AddItem(std::move(element), ui_theme::metrics::kToolbarButtonSize);
        button.id = button.element->Id();
    }

    SetScalePercent(scale_percent_);
    SetState(state_);
}

void ImgViewerUiSelectionToolstrip::SetScalePercent(int percent)
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

void ImgViewerUiSelectionToolstrip::SetState(ImgViewerUiSelectionToolstripState state)
{
    state_ = state;
    UpdateVisualState();
}

D2D1_RECT_F ImgViewerUiSelectionToolstrip::Rect() const
{
    return toolbar_->Rect();
}

D2D1_SIZE_F ImgViewerUiSelectionToolstrip::Measure(const UiDrawContext&, D2D1_SIZE_F) const
{
    if (!state_.visible) {
        return D2D1::SizeF();
    }

    return toolbar_->Measure(kButtonSpecs.size());
}

void ImgViewerUiSelectionToolstrip::Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F anchor_toolbar_rect)
{
    if (!state_.visible) {
        toolbar_->ArrangeHidden();
        return;
    }

    toolbar_->ArrangeAboveAnchor(final_rect, anchor_toolbar_rect, kButtonSpecs.size(), 0.0f, 0, kToolbarGapAboveAnchor);
}

void ImgViewerUiSelectionToolstrip::Render(const UiDrawContext& draw_context, UiRootState state)
{
    if (!state_.visible) {
        return;
    }

    toolbar_->RenderBackground(draw_context, ui_theme::color::kAccent, ui_theme::metrics::kStrokeWidth);
    toolbar_->Panel()->Render(draw_context, state);
}

UiEventResult ImgViewerUiSelectionToolstrip::OnPointerEvent(const UiPointerEvent&)
{
    return {};
}

void ImgViewerUiSelectionToolstrip::UpdateVisualState()
{
    toolbar_->Panel()->SetEnabled(state_.visible);
    for (const ButtonInstance& button : buttons_) {
        if (button.element != nullptr) {
            button.element->SetEnabled(state_.visible);
        }
    }
}
