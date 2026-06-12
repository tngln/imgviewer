#pragma once

#include <memory>
#include <vector>

#include "ui.element.hpp"
#include "ui.layout.hpp"

struct UiThickness final {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

class StackPanel final : public UiElement {
public:
    explicit StackPanel(
        UiElementMetadata metadata,
        ui_layout::StackDirection direction = ui_layout::StackDirection::Vertical);

    void SetGap(float gap);
    void SetPadding(UiThickness padding);
    void SetCrossAxis(ui_layout::StackCrossAxis cross_axis);
    void SetItemFixedMainSize(size_t index, float fixed_main_size);

    template <typename T>
    T* AddItem(std::unique_ptr<T> child, float fixed_main_size = 0.0f)
    {
        T* raw_child = child.get();
        AddChild(std::move(child));
        item_fixed_main_sizes_.push_back(fixed_main_size);
        return raw_child;
    }

    template <typename TOwned>
    auto AddItem(TOwned child, float fixed_main_size = 0.0f)
        -> decltype(this->AddItem(child.Release(), fixed_main_size))
    {
        return AddItem(child.Release(), fixed_main_size);
    }

    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const override;
    void Arrange(D2D1_RECT_F final_rect) override;
    void Render(const UiDrawContext& context, UiRootState state) const override;

private:
    D2D1_RECT_F ContentRect(D2D1_RECT_F rect) const;
    float FixedMainSizeAt(size_t index) const;

    ui_layout::StackDirection direction_ = ui_layout::StackDirection::Vertical;
    ui_layout::StackCrossAxis cross_axis_ = ui_layout::StackCrossAxis::Stretch;
    UiThickness padding_ = {};
    float gap_ = 0.0f;
    mutable std::vector<D2D1_SIZE_F> measured_children_;
    std::vector<float> item_fixed_main_sizes_;
};

class ScrollPanel final : public UiElement {
public:
    explicit ScrollPanel(UiElementMetadata metadata);

    UiElement* SetContent(std::unique_ptr<UiElement> content);
    UiElement* Content();
    const UiElement* Content() const;
    void SetScrollStep(float step);

    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const override;
    void Arrange(D2D1_RECT_F final_rect) override;
    void Render(const UiDrawContext& context, UiRootState state) const override;
    UiEventResult OnPointerEvent(const UiPointerEvent& event) override;

private:
    float ContentHeight() const;
    float ViewportHeight() const;
    float MaxScrollOffset() const;
    float EffectiveScrollOffset() const;
    bool ScrollByWheelDelta(int wheel_delta);
    bool SetScrollOffset(float offset);
    bool SetScrollOffsetFromThumbTop(float thumb_top);
    D2D1_RECT_F ViewportRect() const;
    D2D1_RECT_F ScrollbarTrackRect() const;
    D2D1_RECT_F ScrollbarThumbRect() const;

    UiElement* content_ = nullptr;
    mutable D2D1_SIZE_F measured_content_ = {};
    float scroll_offset_ = 0.0f;
    float scroll_step_ = 36.0f;
    float drag_thumb_pointer_offset_ = 0.0f;
};
