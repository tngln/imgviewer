#include "ui.layout.hpp"

#include <d2d1helper.h>

namespace ui_layout {

std::vector<D2D1_RECT_F> PlaceStack(const StackLayout& layout)
{
    std::vector<D2D1_RECT_F> rects;
    rects.reserve(layout.item_sizes.size());

    float offset = layout.direction == StackDirection::Horizontal ? layout.bounds.left : layout.bounds.top;
    for (float item_size : layout.item_sizes) {
        if (layout.direction == StackDirection::Horizontal) {
            const float bottom = layout.cross_axis == StackCrossAxis::Stretch
                ? layout.bounds.bottom
                : layout.bounds.top + layout.cross_axis_size;
            rects.push_back(D2D1::RectF(offset, layout.bounds.top, offset + item_size, bottom));
        } else {
            const float right = layout.cross_axis == StackCrossAxis::Stretch
                ? layout.bounds.right
                : layout.bounds.left + layout.cross_axis_size;
            rects.push_back(D2D1::RectF(layout.bounds.left, offset, right, offset + item_size));
        }
        offset += item_size + layout.gap;
    }

    return rects;
}

std::vector<D2D1_RECT_F> PlaceVerticalStack(
    D2D1_RECT_F bounds,
    const std::vector<float>& heights,
    float gap)
{
    return PlaceStack(StackLayout{
        .bounds = bounds,
        .direction = StackDirection::Vertical,
        .item_sizes = heights,
        .gap = gap,
        .cross_axis = StackCrossAxis::Stretch,
    });
}

std::vector<D2D1_RECT_F> PlaceHorizontalStack(
    D2D1_RECT_F bounds,
    const std::vector<float>& widths,
    float gap)
{
    return PlaceStack(StackLayout{
        .bounds = bounds,
        .direction = StackDirection::Horizontal,
        .item_sizes = widths,
        .gap = gap,
        .cross_axis = StackCrossAxis::Stretch,
    });
}

std::vector<D2D1_RECT_F> PlaceBottomRightRow(
    D2D1_RECT_F container,
    const std::vector<float>& widths,
    float item_height,
    float right_padding,
    float bottom_padding,
    float gap)
{
    std::vector<D2D1_RECT_F> rects(widths.size());
    float right = container.right - right_padding;
    const float bottom = container.bottom - bottom_padding;
    for (size_t reverse_index = 0; reverse_index < widths.size(); ++reverse_index) {
        const size_t index = widths.size() - reverse_index - 1;
        const float width = widths[index];
        rects[index] = D2D1::RectF(right - width, bottom - item_height, right, bottom);
        right -= width + gap;
    }
    return rects;
}

std::vector<D2D1_RECT_F> PlaceRightAlignedRow(
    D2D1_RECT_F container,
    float item_width,
    float item_height,
    size_t count,
    float gap)
{
    std::vector<D2D1_RECT_F> rects;
    rects.reserve(count);

    float right = container.right;
    for (size_t index = 0; index < count; ++index) {
        rects.push_back(D2D1::RectF(right - item_width, container.top, right, container.top + item_height));
        right -= item_width + gap;
    }

    return rects;
}

std::vector<D2D1_RECT_F> PlaceHorizontalRow(
    D2D1_POINT_2F origin,
    float height,
    const std::vector<float>& widths,
    float gap)
{
    return PlaceHorizontalStack(
        D2D1::RectF(origin.x, origin.y, origin.x, origin.y + height),
        widths,
        gap);
}

D2D1_RECT_F Below(D2D1_RECT_F anchor, float gap, float width, float height)
{
    const float top = anchor.bottom + gap;
    return D2D1::RectF(anchor.left, top, anchor.left + width, top + height);
}

} // namespace ui_layout
