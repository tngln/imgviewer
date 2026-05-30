#include "ui.layout.hpp"

#include <algorithm>

#include <d2d1helper.h>

namespace ui_layout {

float RectWidth(D2D1_RECT_F rect)
{
    return rect.right - rect.left;
}

float RectHeight(D2D1_RECT_F rect)
{
    return rect.bottom - rect.top;
}

D2D1_RECT_F Inset(D2D1_RECT_F rect, float all)
{
    return Inset(rect, all, all, all, all);
}

D2D1_RECT_F Inset(D2D1_RECT_F rect, float left, float top, float right, float bottom)
{
    return D2D1::RectF(rect.left + left, rect.top + top, rect.right - right, rect.bottom - bottom);
}

D2D1_RECT_F StableRect(float left, float top, float right, float bottom)
{
    return D2D1::RectF(left, top, (std::max)(left + 1.0f, right), (std::max)(top + 1.0f, bottom));
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
