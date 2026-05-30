#include "ui.layout.hpp"

#include <d2d1helper.h>

namespace ui_layout {

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
    std::vector<D2D1_RECT_F> rects;
    rects.reserve(widths.size());

    float left = origin.x;
    for (float width : widths) {
        rects.push_back(D2D1::RectF(left, origin.y, left + width, origin.y + height));
        left += width + gap;
    }

    return rects;
}

D2D1_RECT_F Below(D2D1_RECT_F anchor, float gap, float width, float height)
{
    const float top = anchor.bottom + gap;
    return D2D1::RectF(anchor.left, top, anchor.left + width, top + height);
}

} // namespace ui_layout
