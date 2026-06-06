#pragma once

#include "ui.element.hpp"
#include "ui.events.hpp"

class Slider final : public UiElement {
public:
    Slider(UiElementMetadata metadata, int minimum, int maximum, int value, int small_step, int large_step);

    int Minimum() const;
    int Maximum() const;
    int Value() const;
    bool SetValue(int value);

    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const override;
    void Render(const UiDrawContext& context, UiRootState state) const override;
    UiEventResult OnInputEvent(const UiInputEvent& event) override;
    UiEventResult OnPointerEvent(const UiPointerEvent& event) override;
    UiEventResult OnKeyEvent(const UiKeyEvent& event) override;

private:
    int ValueFromPoint(D2D1_POINT_2F point) const;
    D2D1_RECT_F TrackRect() const;
    D2D1_RECT_F ThumbRect() const;

    int minimum_ = 0;
    int maximum_ = 100;
    int value_ = 0;
    int small_step_ = 1;
    int large_step_ = 10;
    bool dragging_ = false;
};
