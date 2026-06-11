#pragma once

#include <memory>
#include <string>

#include <d2d1_1.h>

#include "imgviewer.viewer.hpp"
#include "imgviewer.ui.toolstrip.hpp"
#include "ui.label.hpp"

class ImgViewerUiAnimationToolbar final {
public:
    explicit ImgViewerUiAnimationToolbar(UiElement& root);

    void SetScalePercent(int percent);
    void SetState(ImgViewerAnimationState state);
    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const;
    void Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F main_toolbar_rect);
    void Render(const UiDrawContext& draw_context, UiRootState state);
    UiEventResult OnPointerEvent(const UiPointerEvent& event);

private:
    static constexpr size_t kLoopIndex = 0;
    static constexpr size_t kPlayPauseIndex = 2;

    std::unique_ptr<ImgViewerUiToolStrip> toolstrip_;
    Label* frame_label_ = nullptr;
    std::wstring frame_text_;
    ImgViewerAnimationState state_;
};
