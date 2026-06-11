#include "imgviewer.ui.animation_toolbar.hpp"

#include <cwchar>
#include <memory>
#include <vector>

#include <d2d1helper.h>

#include "imgviewer.config.hpp"
#include "imgviewer.strings.hpp"
#include "imgviewer.ui.action.hpp"
#include "ui.button.hpp"
#include "ui.theme.hpp"

namespace {

constexpr wchar_t kLoopIcon[] = L"\xE8EE";
constexpr wchar_t kPreviousIcon[] = L"\xE892";
constexpr wchar_t kNextIcon[] = L"\xE893";
constexpr wchar_t kPlayIcon[] = L"\xE768";
constexpr wchar_t kPauseIcon[] = L"\xE769";
constexpr float kFrameLabelWidth = 58.0f;

const ToolStripItemSpec kSpecs[] = {
    {ImgViewerAction::ToggleAnimationLoop, ImgViewerStringId::LoopAnimation, ImgViewerStringId::LoopAnimation, ToolStripItemVisual::Icon, 0, {}, 0.0f, ImgViewerShapeKind::Rectangle, L"", kLoopIcon},
    {ImgViewerAction::PreviousAnimationFrame, ImgViewerStringId::PreviousAnimationFrame, ImgViewerStringId::PreviousAnimationFrame, ToolStripItemVisual::Icon, 0, {}, 0.0f, ImgViewerShapeKind::Rectangle, L"", kPreviousIcon},
    {ImgViewerAction::ToggleAnimationPlayback, ImgViewerStringId::PlayOrPauseAnimation, ImgViewerStringId::PlayOrPauseAnimation, ToolStripItemVisual::Icon, 0, {}, 0.0f, ImgViewerShapeKind::Rectangle, L"", kPauseIcon},
    {ImgViewerAction::NextAnimationFrame, ImgViewerStringId::NextAnimationFrame, ImgViewerStringId::NextAnimationFrame, ToolStripItemVisual::Icon, 0, {}, 0.0f, ImgViewerShapeKind::Rectangle, L"", kNextIcon},
};

std::vector<ToolStripItemSpec> BuildSpecs()
{
    return {std::begin(kSpecs), std::end(kSpecs)};
}

} // namespace

ImgViewerUiAnimationToolbar::ImgViewerUiAnimationToolbar(UiElement& root)
{
    toolstrip_ = std::make_unique<ImgViewerUiToolStrip>(
        root, ImgViewerString(ImgViewerStringId::AnimationControls), BuildSpecs());
    toolstrip_->SetExtraWidth(kFrameLabelWidth);
    toolstrip_->SetExtraItemCount(1);

    auto label = std::make_unique<Label>(
        UiMetadata(UiElementRole::Text, kUiActionNone, ImgViewerString(ImgViewerStringId::AnimationFrame), false, true),
        L"",
        LabelStyle::Muted);
    frame_label_ = toolstrip_->Panel()->AddItem(std::move(label), kFrameLabelWidth);
    SetScalePercent(125);
    SetState(state_);
}

void ImgViewerUiAnimationToolbar::SetScalePercent(int percent)
{
    toolstrip_->SetScalePercent(percent);
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

    toolstrip_->SetVisible(state_.available);

    auto* play_pause = dynamic_cast<IconButton*>(toolstrip_->Button(kPlayPauseIndex));
    if (play_pause != nullptr) {
        play_pause->SetIcon(state_.playing ? kPauseIcon : kPlayIcon);
    }

    std::vector<bool> active(4);
    active[kLoopIndex] = state_.loop;
    toolstrip_->SetActiveStates(active);
}

D2D1_SIZE_F ImgViewerUiAnimationToolbar::Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const
{
    return toolstrip_->Measure(context, available_size);
}

void ImgViewerUiAnimationToolbar::Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F main_toolbar_rect)
{
    toolstrip_->Arrange(final_rect, main_toolbar_rect);
}

void ImgViewerUiAnimationToolbar::Render(const UiDrawContext& draw_context, UiRootState state)
{
    toolstrip_->Render(draw_context, state);
}

UiEventResult ImgViewerUiAnimationToolbar::OnPointerEvent(const UiPointerEvent& event)
{
    return toolstrip_->OnPointerEvent(event);
}
