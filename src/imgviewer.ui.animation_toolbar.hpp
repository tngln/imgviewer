#pragma once

#include <array>
#include <cstddef>
#include <string>

#include <d2d1_1.h>

#include "imgviewer.viewer.hpp"
#include "ui.button.hpp"
#include "ui.label.hpp"
#include "ui.panel.hpp"

class ImgViewerUiAnimationToolbar final {
public:
    enum class ButtonKey : size_t {
        Loop,
        PreviousFrame,
        PlayPause,
        NextFrame,
        Count,
    };

    static constexpr size_t kButtonCount = static_cast<size_t>(ButtonKey::Count);
    static constexpr size_t ButtonIndex(ButtonKey button);

    explicit ImgViewerUiAnimationToolbar(UiElement& root);

    void SetScalePercent(int percent);
    void SetState(ImgViewerAnimationState state);
    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const;
    void Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F main_toolbar_rect);
    void Render(const UiDrawContext& draw_context, UiRootState state);

private:
    struct ButtonInstance final {
        UiElementId id = UiElementId::None;
        IconButton* element = nullptr;
    };

    IconButton* Button(ButtonKey button);
    void UpdateVisualState();

    std::array<ButtonInstance, kButtonCount> buttons_{};
    StackPanel* panel_ = nullptr;
    Label* frame_label_ = nullptr;
    D2D1_RECT_F toolbar_rect_ = {};
    std::wstring frame_text_;
    ImgViewerAnimationState state_;
    int scale_percent_ = 125;
};
