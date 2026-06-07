#include "imgviewer.ui.animation_toolbar.hpp"

#include <cwchar>
#include <memory>

#include <d2d1helper.h>

#include "imgviewer.config.hpp"
#include "imgviewer.ui.action.hpp"
#include "ui.theme.hpp"

namespace {

constexpr wchar_t kLoopIcon[] = L"\xE8EE";
constexpr wchar_t kPreviousIcon[] = L"\xE892";
constexpr wchar_t kNextIcon[] = L"\xE893";
constexpr wchar_t kPlayIcon[] = L"\xE768";
constexpr wchar_t kPauseIcon[] = L"\xE769";
constexpr float kFrameLabelWidth = 58.0f;
constexpr float kToolbarGapAboveMain = 6.0f;

struct ButtonSpec final {
    ImgViewerUiAnimationToolbar::ButtonKey button = ImgViewerUiAnimationToolbar::ButtonKey::Loop;
    ImgViewerAction action = ImgViewerAction::None;
    const wchar_t* name = L"";
    const wchar_t* tooltip = L"";
    const wchar_t* automation_id = L"";
    const wchar_t* icon = L"";
};

constexpr std::array<ButtonSpec, ImgViewerUiAnimationToolbar::kButtonCount> kButtonSpecs{{
    {ImgViewerUiAnimationToolbar::ButtonKey::Loop, ImgViewerAction::ToggleAnimationLoop, L"Loop Animation",
        L"Loop animation", L"animation-loop", kLoopIcon},
    {ImgViewerUiAnimationToolbar::ButtonKey::PreviousFrame, ImgViewerAction::PreviousAnimationFrame,
        L"Previous Frame", L"Previous frame", L"animation-previous-frame", kPreviousIcon},
    {ImgViewerUiAnimationToolbar::ButtonKey::PlayPause, ImgViewerAction::ToggleAnimationPlayback,
        L"Play or Pause Animation", L"Play or pause animation", L"animation-play-pause", kPauseIcon},
    {ImgViewerUiAnimationToolbar::ButtonKey::NextFrame, ImgViewerAction::NextAnimationFrame,
        L"Next Frame", L"Next frame", L"animation-next-frame", kNextIcon},
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

constexpr size_t ImgViewerUiAnimationToolbar::ButtonIndex(ButtonKey button)
{
    return static_cast<size_t>(button);
}

ImgViewerUiAnimationToolbar::ImgViewerUiAnimationToolbar(UiElement& root)
{
    toolbar_ = std::make_unique<ImgViewerFloatingToolbar>(root, L"Animation toolbar", L"animation-toolbar");

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
        button.element = toolbar_->Panel()->AddItem(std::move(element), ui_theme::metrics::kToolbarButtonSize);
        button.id = button.element->Id();
    }

    auto label = std::make_unique<Label>(
        UiMetadata(UiElementRole::Text, kUiActionNone, L"Animation frame", L"", L"animation-frame-label", false, true),
        L"",
        LabelStyle::Muted);
    frame_label_ = toolbar_->Panel()->AddItem(std::move(label), kFrameLabelWidth);
    SetScalePercent(scale_percent_);
    SetState(state_);
}

void ImgViewerUiAnimationToolbar::SetScalePercent(int percent)
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

void ImgViewerUiAnimationToolbar::SetState(ImgViewerAnimationState state)
{
    state_ = state;
    if (state_.available) {
        wchar_t text[32] = {};
        swprintf_s(text, L"%zu/%zu", state_.current_frame, state_.total_frames);
        frame_text_ = text;
    } else {
        frame_text_.clear();
    }
    frame_label_->SetText(frame_text_.c_str());
    UpdateVisualState();
}

D2D1_SIZE_F ImgViewerUiAnimationToolbar::Measure(const UiDrawContext&, D2D1_SIZE_F) const
{
    if (!state_.available) {
        return D2D1::SizeF();
    }

    return toolbar_->Measure(kButtonSpecs.size(), toolbar_->ScaledValue(kFrameLabelWidth), 1);
}

void ImgViewerUiAnimationToolbar::Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F main_toolbar_rect)
{
    if (!state_.available) {
        toolbar_->ArrangeHidden();
        return;
    }

    toolbar_->ArrangeAboveAnchor(
        final_rect,
        main_toolbar_rect,
        kButtonSpecs.size(),
        toolbar_->ScaledValue(kFrameLabelWidth),
        1,
        kToolbarGapAboveMain);
}

void ImgViewerUiAnimationToolbar::Render(const UiDrawContext& draw_context, UiRootState state)
{
    if (!state_.available) {
        return;
    }

    toolbar_->RenderBackground(draw_context, ui_theme::color::kBorder, ui_theme::metrics::kStrokeWidth);
    toolbar_->Panel()->Render(draw_context, state);
}

UiEventResult ImgViewerUiAnimationToolbar::OnPointerEvent(const UiPointerEvent&)
{
    return {};
}

IconButton* ImgViewerUiAnimationToolbar::Button(ButtonKey button)
{
    return buttons_[ButtonIndex(button)].element;
}

void ImgViewerUiAnimationToolbar::UpdateVisualState()
{
    toolbar_->Panel()->SetEnabled(state_.available);
    Button(ButtonKey::Loop)->SetVisualActive(state_.available && state_.loop);
    Button(ButtonKey::PlayPause)->SetIcon(state_.playing ? kPauseIcon : kPlayIcon);
    for (const ButtonInstance& button : buttons_) {
        if (button.element != nullptr) {
            button.element->SetEnabled(state_.available);
        }
    }
}
