#pragma once

#include <array>
#include <cstddef>
#include <memory>

#include <d2d1_1.h>

#include "imgviewer.ui.floating_toolbar.hpp"
#include "ui.events.hpp"

struct ImgViewerUiPenToolstripState final {
    bool visible = false;
    D2D1_COLOR_F color = D2D1::ColorF(D2D1::ColorF::Red);
    float width = 4.0f;
};

class ImgViewerUiPenToolstrip final {
public:
    enum class ButtonKey : size_t {
        Red,
        Yellow,
        Green,
        Cyan,
        Blue,
        Magenta,
        White,
        Black,
        Width2,
        Width4,
        Width8,
        Width12,
        Count,
    };

    static constexpr size_t kButtonCount = static_cast<size_t>(ButtonKey::Count);
    static constexpr size_t ButtonIndex(ButtonKey button);

    explicit ImgViewerUiPenToolstrip(UiElement& root);

    void SetScalePercent(int percent);
    void SetState(ImgViewerUiPenToolstripState state);
    D2D1_RECT_F Rect() const;
    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const;
    void Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F anchor_toolbar_rect);
    void Render(const UiDrawContext& draw_context, UiRootState state);
    UiEventResult OnPointerEvent(const UiPointerEvent& event);

private:
    struct ButtonInstance final {
        UiElementId id = UiElementId::None;
        UiElement* element = nullptr;
    };

    void UpdateVisualState();

    std::array<ButtonInstance, kButtonCount> buttons_{};
    std::unique_ptr<ImgViewerFloatingToolbar> toolbar_;
    ImgViewerUiPenToolstripState state_;
    int scale_percent_ = 125;
};
