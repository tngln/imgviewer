#pragma once

#include <array>
#include <cstddef>
#include <memory>

#include <d2d1_1.h>

#include "imgviewer.edit.hpp"
#include "imgviewer.ui.floating_toolbar.hpp"
#include "ui.events.hpp"

struct ImgViewerUiShapeToolstripState final {
    bool visible = false;
    ImgViewerShapeKind kind = ImgViewerShapeKind::Rectangle;
    D2D1_COLOR_F color = D2D1::ColorF(D2D1::ColorF::Red);
};

class ImgViewerUiShapeToolstrip final {
public:
    enum class ButtonKey : size_t {
        Rectangle,
        Ellipse,
        Line,
        Arrow,
        Red,
        Yellow,
        Green,
        Cyan,
        Blue,
        Magenta,
        White,
        Black,
        Count,
    };

    static constexpr size_t kButtonCount = static_cast<size_t>(ButtonKey::Count);
    static constexpr size_t ButtonIndex(ButtonKey button);

    explicit ImgViewerUiShapeToolstrip(UiElement& root);

    void SetScalePercent(int percent);
    void SetState(ImgViewerUiShapeToolstripState state);
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
    ImgViewerUiShapeToolstripState state_;
    int scale_percent_ = 125;
};
