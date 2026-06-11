#pragma once

#include <functional>
#include <string>

#include "ui.element.hpp"
#include "ui.events.hpp"

class Slider final : public UiElement, public UiRangeAccessible {
public:
    Slider(UiElementMetadata metadata, int minimum, int maximum, int value, int small_step, int large_step);

    int Minimum() const;
    int Maximum() const;
    int Value() const;
    bool SetValue(int value);
    void SetAccessibilityValueChangedHandler(std::function<void(int)> handler);
    double AccessibilityRangeValue() const override;
    double AccessibilityRangeMinimum() const override;
    double AccessibilityRangeMaximum() const override;
    double AccessibilityRangeSmallChange() const override;
    double AccessibilityRangeLargeChange() const override;
    HRESULT AccessibilitySetRangeValue(double value) override;

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
    std::function<void(int)> accessibility_value_changed_handler_;
};

// Horizontal composite: [Slider] [gap] [value text].
class SliderRow final : public UiElement {
public:
    SliderRow(UiElementMetadata metadata, int minimum, int maximum, int value,
              int small_step, int large_step, float value_width = 36.0f);

    int Value() const;
    bool SetValue(int value);
    void SetValueText(const wchar_t* text);
    Slider* GetSlider();

    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const override;
    void Arrange(D2D1_RECT_F final_rect) override;
    void Render(const UiDrawContext& context, UiRootState state) const override;
    UiEventResult OnInputEvent(const UiInputEvent& event) override;

private:
    Slider* slider_ = nullptr;  // owned via AddChild
    std::wstring value_text_;
    float value_width_ = 36.0f;
    D2D1_RECT_F value_rect_ = {};
    static constexpr float kGap = 8.0f;
};
