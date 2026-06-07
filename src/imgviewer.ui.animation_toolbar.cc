#include "imgviewer.ui.animation_toolbar.hpp"

#include <algorithm>
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

struct AnimationToolbarMetrics final {
    float button_size = ui_theme::metrics::kToolbarButtonSize;
    float button_gap = ui_theme::metrics::kToolbarButtonGap;
    float padding = ui_theme::metrics::kToolbarPadding;
    float corner_radius = ui_theme::metrics::kToolbarCornerRadius;
    float label_width = kFrameLabelWidth;
};

AnimationToolbarMetrics MetricsForScale(int percent)
{
    const float scale = static_cast<float>(ClampToolbarScalePercent(percent)) / 100.0f;
    return AnimationToolbarMetrics{
        .button_size = ui_theme::metrics::kToolbarButtonSize * scale,
        .button_gap = ui_theme::metrics::kToolbarButtonGap * scale,
        .padding = ui_theme::metrics::kToolbarPadding * scale,
        .corner_radius = ui_theme::metrics::kToolbarCornerRadius * scale,
        .label_width = kFrameLabelWidth * scale,
    };
}

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
    panel_ = static_cast<StackPanel*>(root.AddChild(std::make_unique<StackPanel>(
        UiMetadata(UiElementRole::Pane, kUiActionNone, L"Animation toolbar", L"", L"animation-toolbar", false, false),
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

    auto label = std::make_unique<Label>(
        UiMetadata(UiElementRole::Text, kUiActionNone, L"Animation frame", L"", L"animation-frame-label", false, true),
        L"",
        LabelStyle::Muted);
    frame_label_ = panel_->AddItem(std::move(label), kFrameLabelWidth);
    SetScalePercent(scale_percent_);
    SetState(state_);
}

void ImgViewerUiAnimationToolbar::SetScalePercent(int percent)
{
    scale_percent_ = ClampToolbarScalePercent(percent);
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

    const AnimationToolbarMetrics metrics = MetricsForScale(scale_percent_);
    const float item_count = static_cast<float>(kButtonSpecs.size() + 1);
    return D2D1::SizeF(
        metrics.button_size * static_cast<float>(kButtonSpecs.size()) +
            metrics.label_width +
            metrics.button_gap * (item_count - 1.0f) +
            metrics.padding * 2.0f,
        metrics.button_size + metrics.padding * 2.0f);
}

void ImgViewerUiAnimationToolbar::Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F main_toolbar_rect)
{
    const AnimationToolbarMetrics metrics = MetricsForScale(scale_percent_);
    const D2D1_SIZE_F toolbar_size = Measure(UiDrawContext{}, D2D1::SizeF());
    if (!state_.available) {
        toolbar_rect_ = D2D1::RectF();
        panel_->SetHitTestVisible(false);
        panel_->Arrange(toolbar_rect_);
        return;
    }

    const float viewport_width = final_rect.right - final_rect.left;
    const float left = (std::max)(0.0f, (viewport_width - toolbar_size.width) * 0.5f);
    const float preferred_top = main_toolbar_rect.top - toolbar_size.height - kToolbarGapAboveMain;
    const float top = (std::max)(ui_theme::metrics::kTitleBarHeight + metrics.padding, preferred_top);
    toolbar_rect_ = D2D1::RectF(left, top, left + toolbar_size.width, top + toolbar_size.height);
    panel_->SetHitTestVisible(true);
    panel_->SetGap(metrics.button_gap);
    panel_->SetPadding(UiThickness{metrics.padding, metrics.padding, metrics.padding, metrics.padding});
    for (size_t index = 0; index < kButtonSpecs.size(); ++index) {
        panel_->SetItemFixedMainSize(index, metrics.button_size);
    }
    panel_->SetItemFixedMainSize(kButtonSpecs.size(), metrics.label_width);
    panel_->Arrange(toolbar_rect_);
}

void ImgViewerUiAnimationToolbar::Render(const UiDrawContext& draw_context, UiRootState state)
{
    if (!state_.available) {
        return;
    }

    const UiDraw draw(draw_context);
    const AnimationToolbarMetrics metrics = MetricsForScale(scale_percent_);
    const D2D1_ROUNDED_RECT background = D2D1::RoundedRect(toolbar_rect_, metrics.corner_radius, metrics.corner_radius);
    draw.FillRoundedRect(
        background,
        D2D1::ColorF(
            ui_theme::color::kToolbarBackground.r,
            ui_theme::color::kToolbarBackground.g,
            ui_theme::color::kToolbarBackground.b,
            ui_theme::color::kToolbarBackgroundOpacity));
    draw.DrawRoundedRect(background, ui_theme::color::kBorder, ui_theme::metrics::kStrokeWidth);
    panel_->Render(draw_context, state);
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
    panel_->SetEnabled(state_.available);
    Button(ButtonKey::Loop)->SetVisualActive(state_.available && state_.loop);
    Button(ButtonKey::PlayPause)->SetIcon(state_.playing ? kPauseIcon : kPlayIcon);
    for (const ButtonInstance& button : buttons_) {
        if (button.element != nullptr) {
            button.element->SetEnabled(state_.available);
        }
    }
}
