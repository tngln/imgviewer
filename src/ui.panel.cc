#include "ui.panel.hpp"

#include <algorithm>

#include <d2d1helper.h>

#include "math.hpp"

namespace {

float MainSize(D2D1_SIZE_F size, ui_layout::StackDirection direction)
{
    return direction == ui_layout::StackDirection::Horizontal ? size.width : size.height;
}

float CrossSize(D2D1_SIZE_F size, ui_layout::StackDirection direction)
{
    return direction == ui_layout::StackDirection::Horizontal ? size.height : size.width;
}

} // namespace

StackPanel::StackPanel(UiElementMetadata metadata, ui_layout::StackDirection direction) :
    UiElement(metadata),
    direction_(direction)
{
}

void StackPanel::SetGap(float gap)
{
    gap_ = (std::max)(0.0f, gap);
}

void StackPanel::SetPadding(UiThickness padding)
{
    padding_ = padding;
}

void StackPanel::SetCrossAxis(ui_layout::StackCrossAxis cross_axis)
{
    cross_axis_ = cross_axis;
}

void StackPanel::SetItemFixedMainSize(size_t index, float fixed_main_size)
{
    if (index >= item_fixed_main_sizes_.size()) {
        item_fixed_main_sizes_.resize(index + 1, 0.0f);
    }
    item_fixed_main_sizes_[index] = (std::max)(0.0f, fixed_main_size);
}

D2D1_SIZE_F StackPanel::Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const
{
    const float horizontal_padding = padding_.left + padding_.right;
    const float vertical_padding = padding_.top + padding_.bottom;
    const D2D1_SIZE_F child_available = D2D1::SizeF(
        (std::max)(0.0f, available_size.width - horizontal_padding),
        (std::max)(0.0f, available_size.height - vertical_padding));

    measured_children_.clear();
    measured_children_.reserve(ChildCount());

    float main = 0.0f;
    float cross = 0.0f;
    for (size_t index = 0; index < ChildCount(); ++index) {
        const UiElement* child = ChildAt(index);
        const D2D1_SIZE_F measured = child != nullptr ? child->Measure(context, child_available) : D2D1_SIZE_F{};
        measured_children_.push_back(measured);
        const float fixed = FixedMainSizeAt(index);
        main += fixed > 0.0f ? fixed : MainSize(measured, direction_);
        cross = (std::max)(cross, CrossSize(measured, direction_));
    }
    if (ChildCount() > 1) {
        main += gap_ * static_cast<float>(ChildCount() - 1);
    }

    return direction_ == ui_layout::StackDirection::Horizontal
        ? D2D1::SizeF(main + horizontal_padding, cross + vertical_padding)
        : D2D1::SizeF(cross + horizontal_padding, main + vertical_padding);
}

void StackPanel::Arrange(D2D1_RECT_F final_rect)
{
    UiElement::Arrange(final_rect);
    const D2D1_RECT_F content = ContentRect(final_rect);
    float offset = direction_ == ui_layout::StackDirection::Horizontal ? content.left : content.top;
    for (size_t index = 0; index < ChildCount(); ++index) {
        UiElement* child = ChildAt(index);
        if (child == nullptr) {
            continue;
        }

        const D2D1_SIZE_F measured = index < measured_children_.size() ? measured_children_[index] : D2D1_SIZE_F{};
        const float fixed = FixedMainSizeAt(index);
        const float main = fixed > 0.0f ? fixed : MainSize(measured, direction_);
        if (direction_ == ui_layout::StackDirection::Horizontal) {
            const float bottom = cross_axis_ == ui_layout::StackCrossAxis::Stretch
                ? content.bottom
                : content.top + CrossSize(measured, direction_);
            child->Arrange(D2D1::RectF(offset, content.top, offset + main, bottom));
        } else {
            const float right = cross_axis_ == ui_layout::StackCrossAxis::Stretch
                ? content.right
                : content.left + CrossSize(measured, direction_);
            child->Arrange(D2D1::RectF(content.left, offset, right, offset + main));
        }
        offset += main + gap_;
    }
}

void StackPanel::Render(const UiDrawContext& context, UiRootState state) const
{
    for (size_t index = 0; index < ChildCount(); ++index) {
        const UiElement* child = ChildAt(index);
        if (child != nullptr) {
            child->Render(context, state);
        }
    }
}

D2D1_RECT_F StackPanel::ContentRect(D2D1_RECT_F rect) const
{
    return math::Inset(rect, padding_.left, padding_.top, padding_.right, padding_.bottom);
}

float StackPanel::FixedMainSizeAt(size_t index) const
{
    return index < item_fixed_main_sizes_.size() ? item_fixed_main_sizes_[index] : 0.0f;
}
